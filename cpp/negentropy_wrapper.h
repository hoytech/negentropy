
#ifndef _NEGENTROPY_WRAPPER_H
#define _NEGENTROPY_WRAPPER_H

#include "negentropy.h"
#include "negentropy/storage/BTreeMem.h"

#define length(array) ((sizeof(array)) / (sizeof(array[0])))

//This is a C-wrapper for the C++ library that helps in integrating negentropy with nim code.
//TODO: Do error handling by catching exceptions

extern "C" void* storage_new(const char* db_path, const char* name);

extern "C" void* negentropy_new(void* storage, uint64_t frameSizeLimit);

extern "C" const char* negentropy_initiate(void* negentropy);

extern "C" void negentropy_setinitiator(void* negentropy);

extern "C" bool storage_insert(void* storage, uint64_t createdAt, const char* id);

extern "C" bool storage_erase(void* storage, uint64_t createdAt, const char* id);

extern "C" const char* reconcile(void* negentropy, const char* query);

extern "C" const char* reconcile_with_ids(void* negentropy, const char* query, const char* have_ids[], 
                                        uint64_t have_ids_len, const char* need_ids[], uint64_t need_ids_len);

#endif

