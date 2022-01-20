#pragma once

#ifdef __cplusplus
extern "C" {
#include <cstdint>
#include <cstddef>
#include <cstdbool>
#else
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#endif /* __cplusplus */

typedef struct PAYLOAD_TAG PAYLOAD;
typedef bool  (PAYLOAD_WRITE_FUNCTION)(void *context, const unsigned char *buffer, size_t length);
typedef bool  (PAYLOAD_CALLBACK_FUNCTION)(void *user_context, PAYLOAD_WRITE_FUNCTION *stream_writer, void *stream_context);

typedef struct {
   PAYLOAD_CALLBACK_FUNCTION *callback;
   void *user_context;
} CALLBACK_HANDLE;

PAYLOAD *payload_create();
PAYLOAD* payload_create_and_reserve(size_t expected_length);
void     payload_destroy(PAYLOAD** payload);
PAYLOAD *payload_clone(const PAYLOAD *payload);
void     payload_clear(PAYLOAD *payload);
size_t   payload_get_length(const PAYLOAD *payload);
size_t   payload_get_spare_capacity(const PAYLOAD *payload);
const unsigned char *payload_peek_bytes(const PAYLOAD *payload);
size_t   payload_get_parts(const PAYLOAD *payload);
bool     payload_stream_output(const PAYLOAD *payload, PAYLOAD_WRITE_FUNCTION *stream_writer, void *user_context);
size_t   payload_stream_to_heap(const PAYLOAD* payload, unsigned char** output);
void     payload_append_string(PAYLOAD *payload, const char *buffer);
void     payload_append_data(PAYLOAD *payload, const unsigned char *buffer, size_t length);
bool     payload_reserve_data(PAYLOAD *payload, size_t length);
void     payload_append_callback(PAYLOAD *payload, PAYLOAD_CALLBACK_FUNCTION *callback, void *context);
void     payload_append_payload_as_copy(PAYLOAD *destination, const PAYLOAD *source);
void     payload_move_to_payload_end(PAYLOAD *destination, PAYLOAD **source);   // NB: source payload no longer accessible after call
bool     payload_is_empty(const PAYLOAD *payload);
bool     payload_is_valid(const PAYLOAD *payload);
bool     payload_has_callback_data(const PAYLOAD *payload);
bool     payload_are_equal(const PAYLOAD *payload1, const PAYLOAD *payload2);

#ifdef __cplusplus
}
#endif /* __cplusplus */
