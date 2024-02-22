
#ifndef _NEGENTROPY_WRAPPER_H
#define _NEGENTROPY_WRAPPER_H

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

typedef struct _buffer_{
    uint64_t len ;
    char* data;
}buffer;

//This is a C-wrapper for the C++ library that helps in integrating negentropy with nim code.
//TODO: Do error handling by catching exceptions

EXTERNC void* storage_new(const char* db_path, const char* name);

EXTERNC void* negentropy_new(void* storage, uint64_t frameSizeLimit);

EXTERNC const char* negentropy_initiate(void* negentropy);

EXTERNC void negentropy_setinitiator(void* negentropy);

EXTERNC bool storage_insert(void* storage, uint64_t createdAt, buffer* id);

EXTERNC bool storage_erase(void* storage, uint64_t createdAt, buffer* id);

EXTERNC const char* reconcile(void* negentropy, buffer* query);

EXTERNC const char* reconcile_with_ids(void* negentropy, buffer*  query, char* have_ids[], 
                                        uint64_t *have_ids_len, char* need_ids[], uint64_t *need_ids_len);

#endif

