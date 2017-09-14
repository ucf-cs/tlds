#ifndef BOOSTINGMAP_H
#define BOOSTINGMAP_H

#include <vector>
#include "boosting/lockkey.h"
// #include "boosting/map/nbmap.h"
#include "common/assert.h"


class BoostingMap
{
    enum OpType
    {
        FIND = 0,
        INSERT,
        DELETE,
        UPDATE
    };

    struct Operation
    {
        Operation() : type(0), key(0), val(0), expected(0){}
        Operation(uint8_t _type, uint32_t _key, uint32_t _val, uint32_t _expected) : type(_type), key(_key){}
        Operation(uint8_t _type, uint32_t _key, uint32_t _val, uint32_t _expected) : type(_type), key(_key), val(_val){}
        Operation(uint8_t _type, uint32_t _key, uint32_t _val, uint32_t _expected) : type(_type), key(_key), val(_val), expected(_expected){}

        uint8_t type;
        uint32_t key;
        uint32_t val;
        uint32_t expected;
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
   ~BoostingMap(); 

    void Init();
    
    void Uninit();
    
    ReturnCode Insert(uint32_t key, uint32_t val);

    ReturnCode Delete(uint32_t key);
    
    ReturnCode Find(uint32_t key);

    ReturnCode Update(uint32_t key, uint32_t expected, uint32_t val);
    
    void OnAbort(ReturnCode ret);

    void OnCommit();

    void Print();
    
private:
    WaitFreeHashTable m_list;
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

#endif /* end of include guard: BOOSTINGMAP_H */
