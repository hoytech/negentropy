#include <iostream>

#include "negentropy.h"
#include "negentropy/storage/BTreeMem.h"
#include "negentropy_wrapper.h"

//This is a C-wrapper for the C++ library that helps in integrating negentropy with nim code.
//TODO: Do error handling by catching exceptions

void printHexString(std::string_view toPrint){
    for (size_t i = 0; i < toPrint.size(); ++i) {
        printf("%0hhx", toPrint[i]);
    }
    printf("\n");
}

void* storage_new(const char* db_path, const char* name){
    negentropy::storage::BTreeMem* storage;
/* 
    auto env = lmdb::env::create();
    env.set_max_dbs(64);
    env.open(db_path, 0);

    lmdb::dbi btreeDbi;

    {
        auto txn = lmdb::txn::begin(env);
        btreeDbi = negentropy::storage::BTreeMem::setupDB(txn, name);
        txn.commit();
    } */
    //TODO: Finish constructor 
    storage = new negentropy::storage::BTreeMem();
    return storage;
}

void* negentropy_new(void* storage, uint64_t frameSizeLimit){
    //TODO: Make these typecasts into macros??
    negentropy::storage::BTreeMem* lmdbStorage;
    //TODO: reinterpret cast is risky, need to use more safe type conversion.
    lmdbStorage = reinterpret_cast<negentropy::storage::BTreeMem*>(storage);

    Negentropy<negentropy::storage::BTreeMem>* ne;
    try{
    ne = new Negentropy<negentropy::storage::BTreeMem>(*lmdbStorage, frameSizeLimit);
    }catch(negentropy::err e){
        //TODO:Find a way to return this error
        return NULL;
    }
    return ne;
}

size_t negentropy_initiate(void* negentropy, buffer* out){
    Negentropy<negentropy::storage::BTreeMem>* ngn_inst;
    ngn_inst = reinterpret_cast<Negentropy<negentropy::storage::BTreeMem>*>(negentropy);

    std::string* output = new std::string();
    try {
        *output = ngn_inst->initiate();
        std::cout << "output of initiate is, len:" << output->size() << ", output:";
        printHexString(std::string_view(*output));
    } catch(negentropy::err e){
        //TODO:Find a way to return this error
        return 0;
    }
    memcpy( out->data, output->c_str() ,output->size());
    return output->size();
}

void negentropy_setinitiator(void* negentropy){
    Negentropy<negentropy::storage::BTreeMem> *ngn_inst;
    ngn_inst = reinterpret_cast<Negentropy<negentropy::storage::BTreeMem>*>(negentropy);

    ngn_inst->setInitiator();

}


bool storage_insert(void* storage, uint64_t createdAt, buffer* id){
    negentropy::storage::BTreeMem* lmdbStorage;
    lmdbStorage = reinterpret_cast<negentropy::storage::BTreeMem*>(storage);
    std::string_view data(reinterpret_cast< char const* >(id->data), id->len);
    
    std::cout << "inserting entry in storage, createdAt:" << createdAt << ",id:"; 
    printHexString(data);
    
    //TODO: Error handling. Is it required?
    //How does out of memory get handled?
    return lmdbStorage->insert(createdAt, data);
}


bool storage_erase(void* storage, uint64_t createdAt, buffer* id){
    negentropy::storage::BTreeMem* lmdbStorage;
    lmdbStorage = reinterpret_cast<negentropy::storage::BTreeMem*>(storage);
    std::string_view data(reinterpret_cast< char const* >(id->data), id->len);

    std::cout << "erasing entry from storage, createdAt:" << createdAt << ",id:"; 
    printHexString(data);
    
    //TODO: Error handling
    return lmdbStorage->erase(createdAt, data);
}


size_t reconcile(void* negentropy, buffer* query, buffer* output){
    Negentropy<negentropy::storage::BTreeMem> *ngn_inst;
    ngn_inst = reinterpret_cast<Negentropy<negentropy::storage::BTreeMem>*>(negentropy);
    std::string* out = new std::string();
    try {
        *out = ngn_inst->reconcile(std::string_view(reinterpret_cast< char const* >(query->data), query->len));
        std::cout << "reconcile output of reconcile is, len:" << out->size() << ", output:";
        printHexString(std::string_view(*out));
    } catch(negentropy::err e){
        //TODO:Find a way to return this error
        return 0;
    }
    memcpy( output->data, out->c_str() ,out->size());
    return out->size();
}

void transform(std::vector<std::string> &from_ids, buffer* to_ids)
{
   for (int i=0; i < from_ids.size(); i ++){    
    to_ids[i].len = from_ids[i].size();
    to_ids[i].data =  (unsigned char*)from_ids[i].c_str();
   }
}

int reconcile_with_ids(void* negentropy, buffer*  query,reconcile_cbk cbk){
    Negentropy<negentropy::storage::BTreeMem> *ngn_inst;
    ngn_inst = reinterpret_cast<Negentropy<negentropy::storage::BTreeMem>*>(negentropy);

    std::optional<std::string> out;
    std::vector<std::string> haveIds, needIds;
    uint64_t have_ids_len, need_ids_len;
    buffer* have_ids;
    buffer* need_ids;

    try {
        out = ngn_inst->reconcile(std::string_view(reinterpret_cast< char const* >(query->data), query->len), haveIds, needIds);

        have_ids_len = haveIds.size();
        need_ids_len = needIds.size();
        have_ids = (buffer*)malloc(have_ids_len*sizeof(buffer));
        need_ids = (buffer*)malloc(need_ids_len*sizeof(buffer));

        std::cout << "have_ids_len:" << have_ids_len << "need_ids_len:" << need_ids_len << std::endl;
        std::flush(std::cout);

        transform(haveIds, have_ids);
        transform(needIds, need_ids);
        std::cout << "for debug" << std::endl;
    } catch(negentropy::err e){
        std::cout << "caught error "<< e.what() << std::endl;
        //TODO:Find a way to return this error and cleanup partially allocated memory if any
        return -1;
    }
    buffer output;
    if (out) {
        output.len = out.value().size();
        output.data = (unsigned char*)out.value().c_str();
        std::cout << "reconcile_with_ids output of reconcile is, len:" << out.value().size() << ", output:";
        printHexString(std::string_view(out.value()));
        std::cout << "invoking callback" << std::endl;        
    }
    cbk(have_ids, have_ids_len, need_ids, need_ids_len, &output);
    free(have_ids);
    free(need_ids);
    return 0;
}   
