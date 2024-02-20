#include "negentropy.h"
#include "negentropy/storage/BTreeMem.h"
#include "negentropy_wrapper.h"

//This is a C-wrapper for the C++ library that helps in integrating negentropy with nim code.
//TODO: Do error handling by catching exceptions



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

const char* negentropy_initiate(void* negentropy){
    Negentropy<negentropy::storage::BTreeMem>* ngn_inst;
    ngn_inst = reinterpret_cast<Negentropy<negentropy::storage::BTreeMem>*>(negentropy);

    std::string* output = new std::string();
    try {
        *output = ngn_inst->initiate();
    } catch(negentropy::err e){
        //TODO:Find a way to return this error
        return NULL;
    }
    return output->c_str();
}

void negentropy_setinitiator(void* negentropy){
    Negentropy<negentropy::storage::BTreeMem> *ngn_inst;
    ngn_inst = reinterpret_cast<Negentropy<negentropy::storage::BTreeMem>*>(negentropy);

    ngn_inst->setInitiator();

}


bool storage_insert(void* storage, uint64_t createdAt, const char* id){
    negentropy::storage::BTreeMem* lmdbStorage;
    lmdbStorage = reinterpret_cast<negentropy::storage::BTreeMem*>(storage);
    
    //TODO: Error handling. Is it required?
    //How does out of memory get handled?
    return lmdbStorage->insert(createdAt, id);
}


bool storage_erase(void* storage, uint64_t createdAt, const char* id){
    negentropy::storage::BTreeMem* lmdbStorage;
    lmdbStorage = reinterpret_cast<negentropy::storage::BTreeMem*>(storage);
    
    //TODO: Error handling
    return lmdbStorage->erase(createdAt, id);
}


const char* reconcile(void* negentropy, const char* query, uint64_t query_len){
    Negentropy<negentropy::storage::BTreeMem> *ngn_inst;
    ngn_inst = reinterpret_cast<Negentropy<negentropy::storage::BTreeMem>*>(negentropy);

    std::string* output = new std::string();
    try {
        *output = ngn_inst->reconcile(std::string_view(query, query_len));
    } catch(negentropy::err e){
        //TODO:Find a way to return this error
        return NULL;
    }
    return output->c_str();
}

char *convert(const std::string & s)
{
   char *pc = new char[s.size()+1];
   std::strcpy(pc, s.c_str());
   return pc;
}

const char* reconcile_with_ids(void* negentropy, const char* query, uint64_t query_len, char* have_ids[], 
                                        uint64_t *have_ids_len, char* need_ids[], uint64_t *need_ids_len){
    Negentropy<negentropy::storage::BTreeMem> *ngn_inst;
    ngn_inst = reinterpret_cast<Negentropy<negentropy::storage::BTreeMem>*>(negentropy);

    std::optional<std::string>* output;
    std::vector<std::string> haveIds;
    std::vector<std::string> needIds;

    try {
        *output = ngn_inst->reconcile(std::string_view(query, query_len), haveIds, needIds);

        *have_ids_len = haveIds.size();
        *need_ids_len = needIds.size();
        //TODO: Optimize to not copy and rather return memory reference.

        std::transform(haveIds.begin(), haveIds.end(), have_ids, convert);
        std::transform(needIds.begin(), needIds.end(), need_ids, convert);

    } catch(negentropy::err e){
        //TODO:Find a way to return this error
        return NULL;
    }
    if (output->has_value()) {
        //TODO: Figure out diff between error and this.
        return NULL;
    }else {
        
        return output->value().c_str();
    }
}
