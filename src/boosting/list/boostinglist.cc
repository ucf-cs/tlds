#include "boosting/list/boostinglist.h"

__thread BoostingList::LogType* BoostingList::m_log;

BoostingList::~BoostingList()
{
    printf("Total commit %u, abort (total/fake) %u/%u\n", g_count_commit, g_count_abort, g_count_fake_abort);

    ASSERT_CODE
    (
         printf("Total node count %u, Inserts %u, Deletions %u, Finds %u\n", g_count, g_count_ins, g_count_del, g_count_fnd);
         //Print();
    );
}

void BoostingList::Init()
{
    m_log = new LogType;
    m_lock.Init();
}


void BoostingList::Uninit()
{
    delete m_log;
    m_lock.Uninit();
}

BoostingList::ReturnCode BoostingList::Insert(uint32_t key)
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

BoostingList::ReturnCode BoostingList::Delete(uint32_t key)
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

BoostingList::ReturnCode BoostingList::Find(uint32_t key)
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

void BoostingList::OnAbort(ReturnCode ret)
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

void BoostingList::OnCommit()
{
    __sync_fetch_and_add(&g_count_commit, 1);

    m_log->clear();
    m_lock.Unlock();
}

void BoostingList::Print()
{
    m_list.Print();
}
