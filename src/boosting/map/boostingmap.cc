#include "boosting/map/boostingmap.h"

__thread BoostingMap::LogType* BoostingMap::m_log;

BoostingMap::~BoostingMap()
{
    printf("Total commit %u, abort (total/fake) %u/%u\n", g_count_commit, g_count_abort, g_count_fake_abort);

    ASSERT_CODE
    (
         printf("Total node count %u, Inserts %u, Deletions %u, Finds %u\n", g_count, g_count_ins, g_count_del, g_count_fnd);
         //Print();
    );
}

void BoostingMap::Init()
{
    m_log = new LogType;
    m_lock.Init();
}


void BoostingMap::Uninit()
{
    delete m_log;
    m_lock.Uninit();
}

BoostingMap::ReturnCode BoostingMap::Insert(uint32_t key, uint32_t val)
{
    if(!m_lock.Lock(key))
    {
        return LOCK_FAIL;
    }

    //putIfAbsent_first(KEY k, VALUE v, int T){//T is the executing thread's ID
    if(!m_list.Insert(key))
    {
        return OP_FAIL;
    }

    m_log->push_back(Operation(DELETE, key));

    return OK;
}

BoostingMap::ReturnCode BoostingMap::Delete(uint32_t key)
{
    if(!m_lock.Lock(key))
    {
        return LOCK_FAIL;
    }

    if(!m_list.Delete(key))
    {
        return OP_FAIL;
    }

    m_log->push_back(Operation(INSERT, key));

    return OK;
}

BoostingMap::ReturnCode BoostingMap::Find(uint32_t key)
{
    if(!m_lock.Lock(key))
    {
        return LOCK_FAIL; 
    }
            
    if(!m_list.Find(key))
    {
        return OP_FAIL; 
    }

    return OK;
}

ReturnCode Update(uint32_t key, uint32_t expected, uint32_t val)
{
    if(!m_lock.Lock(key))
    {
        return LOCK_FAIL;
    }

    if(!m_list.Insert(key))
    {
        return OP_FAIL;
    }

    m_log->push_back(Operation(DELETE, key));

    return OK;
}

void BoostingMap::OnAbort(ReturnCode ret)
{
    if(ret == LOCK_FAIL)
    {
        __sync_fetch_and_add(&g_count_fake_abort, 1);
    }

    __sync_fetch_and_add(&g_count_abort, 1);

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

void BoostingMap::OnCommit()
{
    __sync_fetch_and_add(&g_count_commit, 1);

    m_log->clear();
    m_lock.Unlock();
}

void BoostingMap::Print()
{
    m_list.Print();
}
