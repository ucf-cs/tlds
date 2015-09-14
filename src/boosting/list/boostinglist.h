#ifndef BOOSTINGLIST_H
#define BOOSTINGLIST_H

#include <vector>
#include "boosting/lockkey.h"
#include "boosting/list/lockfreelist.h"
#include "common/assert.h"


class BoostingList
{
    enum OpType
    {
        FIND = 0,
        INSERT,
        DELETE
    };

    struct Operation
    {
        Operation() : type(0), key(0){}
        Operation(uint8_t _type, uint32_t _key) : type(_type), key(_key){}

        uint8_t type;
        uint32_t key;
    };


    typedef std::vector<Operation> LogType;

public:
   ~BoostingList(); 

    void Init();
    
    void Uninit();
    
    bool Insert(uint32_t key);

    bool Delete(uint32_t key);
    
    bool Find(uint32_t key);
    
    void OnAbort();

    void OnCommit();

    void Print();
    
private:
    LockfreeList m_list;
    LockKey m_lock;
    static __thread LogType* m_log;

    ASSERT_CODE
    (
        uint32_t g_count = 0;
        uint32_t g_count_ins = 0;
        uint32_t g_count_del = 0;
        uint32_t g_count_fnd = 0;
        uint32_t g_count_commit = 0;
        uint32_t g_count_abort = 0;
        uint32_t g_count_abort_ins = 0;
        uint32_t g_count_abort_del = 0;
    )
};

#endif /* end of include guard: BOOSTINGLIST_H */
