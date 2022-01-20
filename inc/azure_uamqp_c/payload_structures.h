#ifndef PAYLOAD_STRUCTURES_H
#define PAYLOAD_STRUCTURES_H

#include "payload.h"

#ifdef __cplusplus
extern "C" 
{
#endif

typedef enum {
   PAYLOAD_TYPE_BYTE_ARRAY = 1,
   PAYLOAD_TYPE_CALLBACK   = 2
} PAYLOAD_TYPE;

typedef struct
{
   unsigned char* bytes;
   uint32_t size;     // current size
   uint32_t capacity; // total capacity of bytes buffer
} PAYLOAD_BYTE_ARRAY;

typedef struct
{
   void *user_context;
   PAYLOAD_CALLBACK_FUNCTION *writer_callback;   // this callback knows how to stream output to a given writer
   size_t calculated_size;
} PAYLOAD_CALLBACK;

struct PAYLOAD_TAG
{
   PAYLOAD_TYPE type;
   union
   {
      PAYLOAD_BYTE_ARRAY byte_array;
      PAYLOAD_CALLBACK callback;
   } x;
	struct PAYLOAD_TAG *next;
};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PAYLOAD_STRUCTURES_H */
