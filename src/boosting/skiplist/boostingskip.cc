#include "boosting/skiplist/boostingskip.h"

__thread BoostingSkip::LogType* BoostingSkip::m_log;

BoostingSkip::BoostingSkip()
{
    m_list = boostskip_alloc();
    init_boostskip_subsystem();
}

BoostingSkip::~BoostingSkip()
{
    printf("Total commit %u, abort (total/fake) %u/%u\n", g_count_commit, g_count_abort, g_count_fake_abort);

    ASSERT_CODE
    (
         printf("Total node count %u, Inserts %u, Deletions %u, Finds %u\n", g_count, g_count_ins, g_count_del, g_count_fnd);
         //Print();
    );

    destroy_boostskip_subsystem();
}

void BoostingSkip::Init()
{
    m_log = new LogType;
    m_lock.Init();
}


void BoostingSkip::Uninit()
{
    delete m_log;
    m_lock.Uninit();
}

BoostingSkip::ReturnCode BoostingSkip::Insert(uint32_t key)
{
    bool ret = m_lock.Lock(key);

    if(!ret)
    {
        return LOCK_FAIL;
    }

    setval_t v = set_update(m_list, key, (void*)0xf0f0f0f0, false);
    ret = v == NULL;
    if(!ret)
    {
        return OP_FAIL;
    }

    m_log->push_back(Operation(DELETE, key));
    return OK;
}

BoostingSkip::ReturnCode BoostingSkip::Delete(uint32_t key)
{
    bool ret = m_lock.Lock(key);
    if(!ret)
    {
        return LOCK_FAIL;
    }

    setval_t v = set_remove(m_list, key);
    ret = v != NULL;
    if(!ret)
    {
        return OP_FAIL;
    }

    m_log->push_back(Operation(INSERT, key));
    return OK;
}

BoostingSkip::ReturnCode BoostingSkip::Find(uint32_t key)
{
    bool ret = m_lock.Lock(key);
    if(!ret)
    {
        return LOCK_FAIL;
    }

    setval_t v = set_lookup(m_list, key);
    ret = v != NULL;

    return ret ? OK : OP_FAIL;
}

void BoostingSkip::OnAbort(ReturnCode ret)
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
            setval_t v = set_update(m_list, op.key, (void*)0xf0f0f0, false);
            ret = v == NULL;
        }
        else
        {
            setval_t v = set_remove(m_list, op.key);
            ret = v != NULL;
        }

        ASSERT(ret, "Revert operations have to succeed");
    }

    m_log->clear();
    m_lock.Unlock();
}

void BoostingSkip::OnCommit()
{
    __sync_fetch_and_add(&g_count_commit, 1);

    m_log->clear();
    m_lock.Unlock();
}

void BoostingSkip::Print()
{
    boostskip_print(m_list);
}
