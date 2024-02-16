#pragma once

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#include "negentropy.h"
#include "negentropy/storage/BTreeLMDB.h"

//This is a C-wrapper for the C++ library that helps in integrating negentropy with nim code.
//TODO: Do error handling by catching exceptions

EXTERNC void* storage_new(){
    negentropy::storage::BTreeLMDB* storage;
    //storage = new negentropy::storage::BTreeLMDB();
    return storage;
}

EXTERNC void* negentropy_new(void* storage, uint64_t frameSizeLimit){
    negentropy::storage::BTreeLMDB* lmdbStorage;
    lmdbStorage = reinterpret_cast<negentropy::storage::BTreeLMDB*>(storage);

    Negentropy<negentropy::storage::BTreeLMDB>* ne;
    try{
    ne = new Negentropy<negentropy::storage::BTreeLMDB>(*lmdbStorage, frameSizeLimit);
    }catch(negentropy::err e){
        //TODO:Find a way to return this error
        return NULL;
    }
    return ne;
}

EXTERNC const char* negentropy_initiate(void* negentropy){
    Negentropy<negentropy::storage::BTreeLMDB>* ngn_inst;
    ngn_inst = reinterpret_cast<Negentropy<negentropy::storage::BTreeLMDB>*>(negentropy);

    std::string* output = new std::string();
    try {
        *output = ngn_inst->initiate();
    } catch(negentropy::err e){
        //TODO:Find a way to return this error
        return NULL;
    }
    return output->c_str();
}

EXTERNC void negentropy_setinitiator(void* negentropy){
    Negentropy<negentropy::storage::BTreeLMDB> *ngn_inst;
    ngn_inst = reinterpret_cast<Negentropy<negentropy::storage::BTreeLMDB>*>(negentropy);

    ngn_inst->setInitiator();

}

