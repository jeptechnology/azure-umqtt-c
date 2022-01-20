#include "../inc/azure_uamqp_c/payload.h"
#include "../inc/azure_uamqp_c/payload_structures.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef WISER
#include "Logger.h"
#else
#define DPRINTF_AMQP(...)
#define FATAL(...) exit(-1)
#endif

static const size_t UNCALCULATED_SIZE = 0xFFFFFFFF;
int32_t payloadCount = 0;

static bool count_bytes(void *context, const unsigned char *buffer, size_t length)
{
   if (context == NULL)
   {
      return false;
   }
   else
   {
      size_t *size = (size_t *)context;
      (void)buffer;
      *size += length;
      return true;
   }
}

static size_t get_size_of_part(const PAYLOAD *payload)
{
   size_t size = 0;

   switch (payload->type)
   {
      case PAYLOAD_TYPE_BYTE_ARRAY:
         size = payload->x.byte_array.size;
         break;

      case PAYLOAD_TYPE_CALLBACK:
         if ((payload->x.callback.calculated_size != UNCALCULATED_SIZE))
         {
            size = payload->x.callback.calculated_size;
         }
         else if (payload->x.callback.writer_callback(payload->x.callback.user_context,
                                                      count_bytes,
                                                      &size) == true)
         {
            // naughty update of const payload...
            ((PAYLOAD *)payload)->x.callback.calculated_size = size;
         }
         else
         {
            DPRINTF_AMQP("[ERROR] Failed to get number of bytes from writer_callback");
            size = 0;
         }
         break;

      default:
         /* Silently ignore unsupported type. */
         break;
   }

   return size;
}

static PAYLOAD* get_last_part(PAYLOAD *payload)
{
   PAYLOAD* last = payload;
   while (last->next != 0)
   {
      last = last->next;
   }
   return last;
}

static void payload_copy_bytes(PAYLOAD *payload, const unsigned char *buffer, size_t length)
{
   payload->type = PAYLOAD_TYPE_BYTE_ARRAY;
   payload->x.byte_array.bytes = (unsigned char *)calloc(length, 1);
   if (payload->x.byte_array.bytes > 0)
   {
      memcpy((void*)payload->x.byte_array.bytes, (void*)buffer, (uint32_t)length);
      payload->x.byte_array.capacity = (uint32_t)length;
      payload->x.byte_array.size = (uint32_t)length;
   }
}

static void payload_set_callback(PAYLOAD *payload, PAYLOAD_CALLBACK_FUNCTION *callback, void *context, size_t size)
{
   payload->type = PAYLOAD_TYPE_CALLBACK;
   payload->x.callback.writer_callback = callback;
   payload->x.callback.user_context = context;
   payload->x.callback.calculated_size = size;
}

size_t payload_get_spare_capacity(const PAYLOAD *payload)
{
   while (payload != NULL)
   {
      switch (payload->type)
      {
      case PAYLOAD_TYPE_BYTE_ARRAY:
         /* Spare capacity comes from last part. */
         if (payload->next == NULL)
         {
            return payload->x.byte_array.capacity - payload->x.byte_array.size;
         }
         break;

      case PAYLOAD_TYPE_CALLBACK:
         /* Nothing required for callback type. */
         break;

      default:
         /* Unsupported type? We just move to next part silently. */
         break;
      }

      payload = payload->next;
   }

   return 0;
}

PAYLOAD *payload_create()
{
   PAYLOAD* new_payload = (PAYLOAD*)calloc(sizeof(PAYLOAD), 1);
   if (new_payload)
   {
      ++payloadCount;
      new_payload->type = PAYLOAD_TYPE_BYTE_ARRAY;
      new_payload->x.byte_array.bytes = NULL;
      new_payload->x.byte_array.capacity = 0;
      new_payload->x.byte_array.size = 0;
      new_payload->next = NULL;
   }
   return new_payload;
}

PAYLOAD* payload_create_and_reserve(size_t expected_length)
{
   PAYLOAD* new_payload = payload_create();
   if (new_payload)
   {
      payload_reserve_data(new_payload, expected_length);
   }
   return new_payload;
}

PAYLOAD *payload_clone(const PAYLOAD *payload)
{
   if (!payload) return NULL;

   PAYLOAD *clone = payload_create();
   payload_append_payload_as_copy(clone, payload);
   return clone;
}

void payload_clear(PAYLOAD *payload)
{
   if (!payload)
   {
      return;
   }

   // get rid of any other parts to this payload
   payload_destroy(&payload->next);

   // clear this payload
   if (payload->type == PAYLOAD_TYPE_BYTE_ARRAY && payload->x.byte_array.bytes != NULL)
   {
      free((void *)payload->x.byte_array.bytes);
   }

   payload->type = PAYLOAD_TYPE_BYTE_ARRAY;
   payload->x.byte_array.bytes = NULL;
   payload->x.byte_array.size = 0;
   payload->x.byte_array.capacity = 0;
}

void payload_destroy(PAYLOAD **payload_to_destroy)
{
   if (payload_to_destroy)
   {
      PAYLOAD *payload = *payload_to_destroy;

      while (payload)
      {
         PAYLOAD *next = payload->next;
         if (payload->type == PAYLOAD_TYPE_BYTE_ARRAY && payload->x.byte_array.bytes != NULL)
         {
            free((void *)payload->x.byte_array.bytes);
            payload->x.byte_array.bytes = NULL;
         }
         free(payload);
         
         --payloadCount;

         payload = next;
      }

      // clear this payload so it is not longer accessible
      *payload_to_destroy = NULL;
   }
}

const unsigned char *payload_peek_bytes(const PAYLOAD *payload)
{
   if (payload != NULL && payload->type == PAYLOAD_TYPE_BYTE_ARRAY)
   {
      return payload->x.byte_array.bytes;
   }
   return NULL;
}

bool payload_are_equal(const PAYLOAD *payload1, const PAYLOAD *payload2)
{
   if (payload1 == payload2) return true;
   if (payload1 == NULL || payload2 == NULL) return false;
   if (payload1->type != payload2->type) return false;
   if (payload1->type == PAYLOAD_TYPE_BYTE_ARRAY)
   {
      if (payload1->x.byte_array.size != payload2->x.byte_array.size)
         return false;

      if (memcmp(payload1->x.byte_array.bytes, payload2->x.byte_array.bytes, payload1->x.byte_array.size))
         return false;
   }
   else
   {
      if (payload1->x.callback.user_context != payload2->x.callback.user_context)
         return false;

      if (payload1->x.callback.writer_callback != payload2->x.callback.writer_callback)
         return false;
   }

   if (payload1->next != payload2->next)
      return false;

   if (payload1->next == NULL)
      return true;

   return payload_are_equal(payload1->next, payload2->next);
}

bool payload_has_callback_data(const PAYLOAD *payload)
{
   while (payload)
   {
      if (payload->type == PAYLOAD_TYPE_CALLBACK)
      {
         return true;
      }
      payload = payload->next;
   }

   return false;
}

size_t payload_get_length(const PAYLOAD *payload)
{
   size_t length = 0;
   while (payload)
   {
      length += get_size_of_part(payload);
      payload = payload->next;
   }
   return length;
}

size_t payload_get_parts(const PAYLOAD *payload)
{
   size_t number_of_parts = 0;
   while (payload)
   {
      number_of_parts++;
      payload = payload->next;
   }
   return number_of_parts;
}


static bool stream_array_output(const PAYLOAD *payload, PAYLOAD_WRITE_FUNCTION *stream_writer, void *context)
{
   return stream_writer(context, payload->x.byte_array.bytes, payload->x.byte_array.size);
}

#if defined(DEBUG_PAYLOAD_WHILST_STREAMING)
static void payload_debug(const PAYLOAD *payload, unsigned payloadCounter)
{
   if (payload->type == PAYLOAD_TYPE_BYTE_ARRAY)
   {
      printf("\nPAYLOAD %d, array[size %d, capacity %d]\n",
         payloadCounter,
         payload->x.byte_array.size,
         payload->x.byte_array.capacity);

      success = stream_array_output(payload, stream_writer, stream_context);
   }
   else if (payload->type == PAYLOAD_TYPE_CALLBACK)
   {
      printf("\nPAYLOAD %d, callback[calculated_size %d]\n",
         payloadCounter,
         payload->x.callback.calculated_size);
      success = payload->x.callback.writer_callback(payload->x.callback.user_context, stream_writer, stream_context);
   }
}
#else
static void payload_debug(const PAYLOAD *payload, unsigned payloadCounter) 
{
   (void)payload; (void)payloadCounter;
}
#endif

bool payload_stream_output(const PAYLOAD *payload, PAYLOAD_WRITE_FUNCTION *stream_writer, void *stream_context)
{
   bool success = false;
   unsigned payloadCounter = 0;
   if (payload && stream_writer)
   {
      success = true;

      while (payload && success)
      {
         payload_debug(payload, payloadCounter++);

         if (payload->type == PAYLOAD_TYPE_BYTE_ARRAY)
         {
            success = stream_array_output(payload, stream_writer, stream_context);
         }
         else if (payload->type == PAYLOAD_TYPE_CALLBACK)
         {
            success = payload->x.callback.writer_callback(payload->x.callback.user_context, stream_writer, stream_context);
         }

         payload = payload->next;
      }
   }

   return success;
}

typedef struct
{
   unsigned char* buffer;
   size_t position;
   size_t capacity;
} string_context;

static bool WritePayloadToString(void* context, const unsigned char* buffer, size_t length)
{
   string_context* sc = (string_context*)context;
   size_t remaining_capacity = sc->capacity - sc->position;
   size_t bytes_to_copy = min(remaining_capacity, length);
   memcpy(sc->buffer + sc->position, buffer, bytes_to_copy);
   sc->position += bytes_to_copy;
   return true;
}

size_t payload_stream_to_heap(const PAYLOAD* payload, unsigned char** output)
{
   if (output == NULL) return 0;

   size_t payload_length = payload_get_length(payload);
   *output = (unsigned char*)calloc(payload_length, 1);
   string_context context = { *output, 0, payload_length };
   payload_stream_output(payload, WritePayloadToString, &context);
   return payload_length;
}

void payload_move_to_payload_end(PAYLOAD *destination, PAYLOAD **source)
{
   if (!destination) FATAL("Payload is null");

   if (source == NULL || *source == NULL)
   {
      return; // nothing to do
   }

   PAYLOAD *tail = get_last_part(destination);

   if (!payload_is_empty(tail))
   {
      tail->next = payload_create();
      tail = tail->next;
   }

   // move source to tail
   *tail = **source;

   // clear old source as it has been moved
   *source = NULL;
}

void payload_append_payload_as_copy(PAYLOAD *payload, const PAYLOAD *payload_to_append)
{
   if (!payload) FATAL("Payload is null");

   PAYLOAD *tail = get_last_part(payload);

   while (payload_to_append != NULL)
   {
      if (payload_to_append->type == PAYLOAD_TYPE_BYTE_ARRAY)
      {
         payload_append_data(tail, payload_to_append->x.byte_array.bytes, payload_to_append->x.byte_array.size);
      }
      else if (payload_to_append->type == PAYLOAD_TYPE_CALLBACK)
      {
         if (!payload_is_empty(tail))
         {
            tail->next = payload_create();
            tail = tail->next;
         }
         
         payload_set_callback(
            tail, 
            payload_to_append->x.callback.writer_callback,
            payload_to_append->x.callback.user_context,
            payload_to_append->x.callback.calculated_size);
      }

      if (tail->next != NULL)
      {
         tail = tail->next;
      }
      payload_to_append = payload_to_append->next;
   }
}

void payload_append_data(PAYLOAD *payload, const unsigned char *buffer, size_t length)
{
   if (!payload) FATAL("Payload is null");

   if ((buffer != NULL) && (length > 0))
   {
      PAYLOAD *tail = get_last_part(payload);

      if (payload_is_empty(tail) && payload_get_spare_capacity(tail) == 0)
      {
         // Empty tail part - just copy on top of it...
         payload_copy_bytes(tail, buffer, length);
      }
      else if (payload_get_spare_capacity(tail) >= length)
      {
         // Short cut - if we can append to our last payload without additional malloc, then do it!
         memcpy(&tail->x.byte_array.bytes[tail->x.byte_array.size], (void *)buffer, length);
         tail->x.byte_array.size += (uint32_t)length;
      }
      else
      {
         tail->next = payload_create();
         payload_copy_bytes(tail->next, buffer, length);
      }
   }
}

void payload_append_string(PAYLOAD *payload, const char *buffer)
{
   payload_append_data(payload, (const unsigned char *)buffer, strlen(buffer));
}

bool payload_reserve_data(PAYLOAD *payload, size_t length)
{
   if (!payload) FATAL("Payload is null");

   PAYLOAD *tail = get_last_part(payload);
   
   if (!payload_is_empty(tail))
   {
      tail->next = payload_create();
      tail = tail->next;
   }

   tail->type = PAYLOAD_TYPE_BYTE_ARRAY;
   tail->x.byte_array.bytes = malloc(length);
   tail->x.byte_array.capacity = (uint32_t)length;
   tail->x.byte_array.size = 0;

   return tail->x.byte_array.bytes != NULL;
}

void payload_append_callback(PAYLOAD *payload, PAYLOAD_CALLBACK_FUNCTION *callback, void *context)
{
   if (!payload) FATAL("Payload is NULL");
   if (!callback) FATAL("Callback is NULL");

   PAYLOAD *tail = get_last_part(payload);

   if (!payload_is_empty(tail))
   {
      tail->next = payload_create();
      tail = tail->next;
   }

   payload_set_callback(tail, callback, context, UNCALCULATED_SIZE);
}

bool payload_is_empty(const PAYLOAD *payload)
{
   while (payload != NULL
      &&  payload->type == PAYLOAD_TYPE_BYTE_ARRAY
      &&  payload->x.byte_array.size == 0)
   {
      payload = payload->next;
   }

   return payload == NULL;
}

static bool payload_bytearray_is_valid(const PAYLOAD_BYTE_ARRAY *byte_array)
{
   if (byte_array->bytes == NULL 
      && (byte_array->capacity != 0 || byte_array->size != 0))
   {
      return false;
   }
   return true;
}

static bool payload_callback_is_valid(const PAYLOAD_CALLBACK *callback)
{
   return callback->writer_callback != NULL;
}

static bool payload_part_is_valid(const PAYLOAD *payload)
{
   return (payload->type == PAYLOAD_TYPE_BYTE_ARRAY && payload_bytearray_is_valid(&payload->x.byte_array))
      || (payload->type == PAYLOAD_TYPE_CALLBACK && payload_callback_is_valid(&payload->x.callback));
}

bool payload_is_valid(const PAYLOAD *payload)
{
   while (payload != NULL && payload_part_is_valid(payload))
   {
      payload = payload->next;
   }

   return payload == NULL;
}
