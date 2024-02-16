
#ifndef _NEGENTROPY_WRAPPER_H
#define _NEGENTROPY_WRAPPER_H

#include "negentropy.h"
#include "negentropy/storage/BTreeLMDB.h"

#define length(array) ((sizeof(array)) / (sizeof(array[0])))

//This is a C-wrapper for the C++ library that helps in integrating negentropy with nim code.
//TODO: Do error handling by catching exceptions

void* storage_new(const char* db_path, const char* name);

void* negentropy_new(void* storage, uint64_t frameSizeLimit);

const char* negentropy_initiate(void* negentropy);

void negentropy_setinitiator(void* negentropy);

bool storage_insert(void* storage, uint64_t createdAt, const char* id);

bool storage_erase(void* storage, uint64_t createdAt, const char* id);

const char* reconcile(void* negentropy, const char* query);

const char* reconcile_with_ids(void* negentropy, const char* query, const char* have_ids[], 
                                        uint64_t have_ids_len, const char* need_ids[], uint64_t need_ids_len);

#endif

