#include <iostream>
#include <unordered_map>

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

void storage_delete(void* storage){
     negentropy::storage::BTreeMem* lmdbStorage = reinterpret_cast<negentropy::storage::BTreeMem*>(storage);
     delete lmdbStorage;
}

void negentropy_delete(void* negentropy){
    Negentropy<negentropy::storage::BTreeMem>* ngn_inst = reinterpret_cast<Negentropy<negentropy::storage::BTreeMem>*>(negentropy);
    delete ngn_inst;
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
/*         std::cout << "output of initiate is, len:" << output->size() << ", output:";
        printHexString(std::string_view(*output)); */
    } catch(negentropy::err e){
        std::cout << "Exception raised in initiate " << e.what() << std::endl;
        //TODO:Find a way to return this error
        return 0;
    }
    memcpy( out->data, output->c_str() ,output->size());
    size_t outlen = output->size();
    delete output;
    return outlen;
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
    
/*     std::cout << "inserting entry in storage, createdAt:" << createdAt << ",id:"; 
    printHexString(data); */
    
    //TODO: Error handling. Is it required?
    //How does out of memory get handled?
    return lmdbStorage->insert(createdAt, data);
}

bool storage_erase(void* storage, uint64_t createdAt, buffer* id){
    negentropy::storage::BTreeMem* lmdbStorage;
    lmdbStorage = reinterpret_cast<negentropy::storage::BTreeMem*>(storage);
    std::string_view data(reinterpret_cast< char const* >(id->data), id->len);

/*     std::cout << "erasing entry from storage, createdAt:" << createdAt << ",id:"; 
    printHexString(data); */
    
    //TODO: Error handling
    return lmdbStorage->erase(createdAt, data);
}

size_t reconcile(void* negentropy, buffer* query, buffer* output){
    Negentropy<negentropy::storage::BTreeMem> *ngn_inst;
    ngn_inst = reinterpret_cast<Negentropy<negentropy::storage::BTreeMem>*>(negentropy);
    std::string* out = new std::string();
    try {
        *out = ngn_inst->reconcile(std::string_view(reinterpret_cast< char const* >(query->data), query->len));
/*         std::cout << "reconcile output of reconcile is, len:" << out->size() << ", output:";
        printHexString(std::string_view(*out)); */
    } catch(negentropy::err e){
        //TODO:Find a way to return this error
        std::cout << "Exception raised in reconcile " << e.what() << std::endl;
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

int reconcile_with_ids(void* negentropy, buffer*  query,reconcile_cbk cbk, char* outptr){
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

        transform(haveIds, have_ids);
        transform(needIds, need_ids);
    } catch(negentropy::err e){
        std::cout << "exception raised in  reconcile_with_ids"<< e.what() << std::endl;
        //TODO:Find a way to return this error and cleanup partially allocated memory if any
        return -1;
    }
    buffer output = {0,NULL};
    if (out) {
        output.len = out.value().size();
        output.data = (unsigned char*)out.value().c_str();
        std::cout << "reconcile_with_ids output of reconcile is, len:" << out.value().size() << ", output:";
        printHexString(std::string_view(out.value()));
    }
    std::cout << "invoking callback" << std::endl;
    std::flush(std::cout);

    cbk(have_ids, have_ids_len, need_ids, need_ids_len, &output, outptr);
    std::cout << "invoked callback" << std::endl;
    std::flush(std::cout);

    free(have_ids);
    free(need_ids);
    return 0;
}   

void transform_with_alloc(std::vector<std::string> &from_ids, buffer* to_ids)
{
   for (int i=0; i < from_ids.size(); i ++){
    to_ids[i].data = (unsigned char*) calloc(from_ids[i].size(), sizeof(unsigned char));
    to_ids[i].len = from_ids[i].size();
    memcpy(to_ids[i].data, from_ids[i].c_str(),to_ids[i].len);
   }
}

void reconcile_with_ids_no_cbk(void* negentropy, buffer*  query, result* result){
    Negentropy<negentropy::storage::BTreeMem> *ngn_inst;
    ngn_inst = reinterpret_cast<Negentropy<negentropy::storage::BTreeMem>*>(negentropy);

    std::optional<std::string> out;
    std::vector<std::string> haveIds, needIds;
    try {
        out = ngn_inst->reconcile(std::string_view(reinterpret_cast< char const* >(query->data), query->len), haveIds, needIds);
        result->have_ids_len = haveIds.size();
        result->need_ids_len = needIds.size();
        if (haveIds.size() > 0){
            result->have_ids = (buffer*)calloc(result->have_ids_len, sizeof(buffer));
            transform_with_alloc(haveIds, result->have_ids);
        }

        if (needIds.size() > 0) {
            result->need_ids = (buffer*)calloc(result->need_ids_len, sizeof(buffer));
            transform_with_alloc(needIds, result->need_ids);
        }

        // std::cout << "have_ids_len:" << result->have_ids_len << "need_ids_len:" << result->need_ids_len << std::endl;


    } catch(negentropy::err e){
        std::cout << "caught error "<< e.what() << std::endl;
        //TODO:Find a way to return this error and cleanup partially allocated memory if any
        return ;
    }
    buffer output = {0,NULL};
    if (out) {
        result->output.len = out.value().size();
        result->output.data = (unsigned char*)calloc(out.value().size(), sizeof(unsigned char));
        memcpy(result->output.data, (unsigned char*)out.value().c_str(),result->output.len) ;
/*         std::cout << "reconcile_with_ids output of reconcile is, len:" << out.value().size() << ", output:";
        printHexString(std::string_view(out.value()));  */
    }else {
        std::cout << "reconcile_with_ids_no_cbk output is empty " << std::endl;
        result->output.len = 0;
        result->output.data = NULL;
    }
    return ;
}

//Note: This function assumes that all relevant heap memory is alloced and just tries to free
void free_result(result* r){
    if (r->output.len > 0) {
        free((void *) r->output.data);
    }
    
    if (r->have_ids_len > 0){
        for (int i = 0; i < r->have_ids_len; i++) {
            free((void *) r->have_ids[i].data);
        }
        free((void *)r->have_ids);
    }

    if (r->need_ids_len > 0) {
        for (int i = 0; i < r->need_ids_len; i++) {
            free((void *) r->need_ids[i].data);
        }
        free((void *)r->need_ids);
    }
}
