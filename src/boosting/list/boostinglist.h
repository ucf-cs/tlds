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
    void Init()
    {
        m_log = new LogType;
        m_lock.Init();
    }

    void Uninit()
    {
        delete m_log;
        m_lock.Uninit();
    }

    bool Insert(uint32_t key)
    {
        bool ret = m_lock.Lock(key);

        if(ret)
        {
            ret = m_list.Insert(key);
            if(ret)
            {
                m_log->push_back(Operation(DELETE, key));
            }
        }

        return ret;
    }

    bool Delete(uint32_t key)
    {
        bool ret = m_lock.Lock(key);

        if(ret)
        {
            ret = m_list.Delete(key);
            if(ret)
            {
                m_log->push_back(Operation(INSERT, key));
            }
        }

        return ret;
    }

    bool Find(uint32_t key)
    {
        return m_lock.Lock(key) && m_list.Find(key);
    }

    void OnAbort()
    {
        //TODO: revert logged ops
        for(int i = m_log->size() - 1; i >= 0; --i)
        {
            bool ret = true;

            const Operation& op = m_log->at(i);

            if(op.type == FIND)
            {
                ASSERT(false, "Revert operation should be Find");
            }
            else if(op.type == INSERT)
            {
                ret = m_list.Insert(op.key);
            }
            else
            {
                ret = m_list.Delete(op.key);
            }
            
            ASSERT(ret, "Revert operations have to succeed");
        }

        m_log->clear();
        m_lock.Unlock();
    }

    void OnCommit()
    {
        m_log->clear();
        m_lock.Unlock();
    }

private:
    LockfreeList m_list;
    LockKey m_lock;
    static __thread LogType* m_log;
};

#endif /* end of include guard: BOOSTINGLIST_H */
