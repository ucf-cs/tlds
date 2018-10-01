#ifndef BOOSTINGSKIP_H
#define BOOSTINGSKIP_H

#include <vector>
#include "boosting/lockkey.h"
extern "C"
{
#include "boosting/skiplist/lockfreeskip.h"
}
#include "common/assert.h"


class BoostingSkip
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
    enum ReturnCode
    {
        OK = 0,
        LOCK_FAIL,
        OP_FAIL
    };

public:
    BoostingSkip();
   ~BoostingSkip(); 

    void Init();
    
    void Uninit();
    
    ReturnCode Insert(uint32_t key);

    ReturnCode Delete(uint32_t key);
    
    ReturnCode Find(uint32_t key);
    
    void OnAbort(ReturnCode ret);

    void OnCommit();

    void ResetMetrics();

    void Print();
    
private:
    boost_skip* m_list;
    LockKey m_lock;
    static __thread LogType* m_log;

    ASSERT_CODE
    (
        uint32_t g_count = 0;
        uint32_t g_count_ins = 0;
        uint32_t g_count_del = 0;
        uint32_t g_count_fnd = 0;
    )

    uint32_t g_count_commit = 0;
    uint32_t g_count_abort = 0;
    uint32_t g_count_fake_abort = 0;
};

#endif /* end of include guard: BOOSTINGLIST_H */
