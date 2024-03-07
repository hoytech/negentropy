
#ifndef _NEGENTROPY_WRAPPER_H
#define _NEGENTROPY_WRAPPER_H

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

typedef struct _buffer_{
    uint64_t len ;
    unsigned char* data;
}buffer;

typedef struct _result_ {
  buffer output;
  uint64_t have_ids_len;
  uint64_t need_ids_len;
  buffer* have_ids;
  buffer* need_ids;
} result;

//This is a C-wrapper for the C++ library that helps in integrating negentropy with nim code.
//TODO: Do error handling by catching exceptions

EXTERNC void* storage_new(const char* db_path, const char* name);

EXTERNC void storage_delete(void* storage);

EXTERNC void* negentropy_new(void* storage, uint64_t frameSizeLimit);

EXTERNC void negentropy_delete(void* negentropy);

EXTERNC size_t negentropy_initiate(void* negentropy, buffer* output);

EXTERNC void negentropy_setinitiator(void* negentropy);

EXTERNC bool storage_insert(void* storage, uint64_t createdAt, buffer* id);

EXTERNC bool storage_erase(void* storage, uint64_t createdAt, buffer* id);

EXTERNC size_t reconcile(void* negentropy, buffer* query, buffer* output);

EXTERNC typedef void (*reconcile_cbk)(buffer* have_ids, uint64_t have_ids_len, buffer* need_ids, uint64_t need_ids_len, buffer* output, char* outptr );

EXTERNC int reconcile_with_ids(void* negentropy, buffer*  query, reconcile_cbk cbk, char* outptr);

EXTERNC void reconcile_with_ids_no_cbk(void* negentropy, buffer*  query, result* result);

EXTERNC void free_result(result* result);

#endif

