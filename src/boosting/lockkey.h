#ifndef LOCKKEY_H
#define LOCKKEY_H

#include <tbb/concurrent_hash_map.h>
#include <mutex>
#include <unordered_set>
#include <chrono>

class LockKey
{
    typedef std::recursive_timed_mutex LockType;
    typedef tbb::concurrent_hash_map<uint32_t, LockType*> LockMap;
    typedef std::unordered_set<LockType*> LockSet;

public:
    LockKey() 
    {}
    
    ~LockKey()
    {
        for(LockMap::iterator it = m_lockMap.begin(); it != m_lockMap.end(); ++it)   
        {
            delete it->second;
        }
    }

    void Init()
    {
        m_lockSet = new LockSet;
    }

    void Uninit()
    {
        delete m_lockSet;
    }

    bool Lock(uint32_t key)
    {
        LockMap::accessor acc;
        LockType* lock = NULL;

        if(m_lockMap.insert(acc, key))
        {
            lock = new LockType;
            acc->second = lock;
        }
        else
        {
            lock = acc->second;
        }

        std::pair<LockSet::iterator, bool> ret = m_lockSet->insert(lock);
        if(ret.second)
        {
            if(lock->try_lock_for(std::chrono::milliseconds(100)))
            {
                return true;
            }
            else
            {
                m_lockSet->erase(lock);
            }
        }

        return false;
    }

    void Unlock()
    {
        for(LockSet::iterator it = m_lockSet->begin(); it != m_lockSet->end(); ++it)
        {
            (*it)->unlock();
        }

        m_lockSet->clear();
    }

private:
    LockMap m_lockMap;
    static __thread LockSet* m_lockSet;
};

#endif /* end of include guard: LOCKKEY_H */
