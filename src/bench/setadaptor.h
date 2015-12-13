#ifndef SETADAPTOR_H
#define SETADAPTOR_H

#include "translink/list/translist.h"
#include "translink/skiplist/transskip.h"
#include "rstm/list/rstmlist.hpp"
#include "boosting/list/boostinglist.h"
#include "boosting/skiplist/boostingskip.h"
#include "common/allocator.h"
#include "ostm/skiplist/stmskip.h"

enum SetOpType
{
    FIND = 0,
    INSERT,
    DELETE
};

struct SetOperator
{
    uint8_t type;
    uint32_t key;
};

enum SetOpStatus
{
    LIVE = 0,
    COMMITTED,
    ABORTED
};

typedef std::vector<SetOperator> SetOpArray;

template<typename T>
class SetAdaptor
{
};

template<>
class SetAdaptor<TransList>
{
public:
    SetAdaptor(uint64_t cap, uint64_t threadCount, uint32_t transSize)
        : m_descAllocator(cap * threadCount * TransList::Desc::SizeOf(transSize), threadCount, TransList::Desc::SizeOf(transSize))
        , m_nodeAllocator(cap * threadCount *  sizeof(TransList::Node) * transSize, threadCount, sizeof(TransList::Node))
        , m_nodeDescAllocator(cap * threadCount *  sizeof(TransList::NodeDesc) * transSize, threadCount, sizeof(TransList::NodeDesc))
        , m_list(&m_nodeAllocator, &m_descAllocator, &m_nodeDescAllocator)
    { }

    void Init()
    {
        m_descAllocator.Init();
        m_nodeAllocator.Init();
        m_nodeDescAllocator.Init();
    }

    void Uninit(){}

    bool ExecuteOps(const SetOpArray& ops)
    {
        //TransList::Desc* desc = m_list.AllocateDesc(ops.size());
        TransList::Desc* desc = m_descAllocator.Alloc();
        desc->size = ops.size();
        desc->status = TransList::LIVE;

        for(uint32_t i = 0; i < ops.size(); ++i)
        {
            desc->ops[i].type = ops[i].type; 
            desc->ops[i].key = ops[i].key; 
        }

        return m_list.ExecuteOps(desc);
    }

private:
    Allocator<TransList::Desc> m_descAllocator;
    Allocator<TransList::Node> m_nodeAllocator;
    Allocator<TransList::NodeDesc> m_nodeDescAllocator;
    TransList m_list;
};

template<>
class SetAdaptor<trans_skip>
{
public:
    SetAdaptor(uint64_t cap, uint64_t threadCount, uint32_t transSize)
        : m_descAllocator(cap * threadCount * Desc::SizeOf(transSize), threadCount, Desc::SizeOf(transSize))
        , m_nodeDescAllocator(cap * threadCount *  sizeof(NodeDesc) * transSize, threadCount, sizeof(NodeDesc))
    { 
        m_skiplist = transskip_alloc(&m_descAllocator, &m_nodeDescAllocator);
        init_transskip_subsystem(); 
    }

    ~SetAdaptor()
    {
        transskip_free(m_skiplist);
    }

    void Init()
    {
        m_descAllocator.Init();
        m_nodeDescAllocator.Init();
    }

    void Uninit()
    {
        destroy_transskip_subsystem(); 
    }

    bool ExecuteOps(const SetOpArray& ops)
    {
        //TransList::Desc* desc = m_list.AllocateDesc(ops.size());
        Desc* desc = m_descAllocator.Alloc();
        desc->size = ops.size();
        desc->status = LIVE;

        for(uint32_t i = 0; i < ops.size(); ++i)
        {
            desc->ops[i].type = ops[i].type; 
            desc->ops[i].key = ops[i].key; 
        }

        return execute_ops(m_skiplist, desc);
    }

private:
    Allocator<Desc> m_descAllocator;
    Allocator<NodeDesc> m_nodeDescAllocator;
    trans_skip* m_skiplist;
};


template<>
class SetAdaptor<RSTMList>
{
public:
    SetAdaptor()
    {
        TM_SYS_INIT();
    }
    
    ~SetAdaptor()
    {
        TM_SYS_SHUTDOWN();

        printf("Total commit %u, abort %u\n", g_count_commit, g_count_abort);
    }

    void Init()
    {
        TM_THREAD_INIT();
    }

    void Uninit()
    {
        TM_THREAD_SHUTDOWN();
    }

    bool ExecuteOps(const SetOpArray& ops)
    {
        bool ret = true;

        TM_BEGIN(atomic)
        {
            if(ret == true)
            {
                for(uint32_t i = 0; i < ops.size(); ++i)
                {
                    uint32_t val = ops[i].key;
                    if(ops[i].type == FIND)
                    {
                        ret = m_list.lookup(val TM_PARAM);
                    }
                    else if(ops[i].type == INSERT)
                    {
                        ret = m_list.insert(val TM_PARAM);
                    }
                    else
                    {
                        ret = m_list.remove(val TM_PARAM);
                    }

                    if(ret == false)
                    {
                        //stm::restart();
                        tx->tmabort(tx);
                        break;
                    }
                }
            }
        } 
        TM_END;

        if(ret)
        {
            __sync_fetch_and_add(&g_count_commit, 1);
        }
        else
        {
            __sync_fetch_and_add(&g_count_abort, 1);
        }

        return ret;
    }

private:
    RSTMList m_list;

    uint32_t g_count_commit = 0;
    uint32_t g_count_abort = 0;
};


template<>
class SetAdaptor<stm_skip>
{
public:
    SetAdaptor()
    {
        init_stmskip_subsystem();
        m_list = stmskip_alloc();
    }
    
    ~SetAdaptor()
    {
        destory_stmskip_subsystem();
    }

    void Init()
    { }

    void Uninit()
    { }

    bool ExecuteOps(const SetOpArray& ops)
    {
        bool ret = stmskip_execute_ops(m_list, (set_op*)ops.data(), ops.size());

        return ret;
    }

private:
    stm_skip* m_list;
};


template<>
class SetAdaptor<BoostingList>
{
public:
    SetAdaptor()
    {
    }
    
    ~SetAdaptor()
    {
    }

    void Init()
    {
        m_list.Init();
    }

    void Uninit()
    {
        m_list.Uninit();
    }

    bool ExecuteOps(const SetOpArray& ops)
    {
        bool ret = false;

        for(uint32_t i = 0; i < ops.size(); ++i)
        {
            uint32_t key = ops[i].key;

            if(ops[i].type == FIND)
            {
                ret = m_list.Find(key);
            }
            else if(ops[i].type == INSERT)
            {
                ret = m_list.Insert(key);
            }
            else
            {
                ret = m_list.Delete(key);
            }

            if(ret == false)
            {
                m_list.OnAbort();
                break;
            }
        }

        if(ret == true)
        {
            m_list.OnCommit();
        }

        return ret;
    }

private:
    BoostingList m_list;
};

template<>
class SetAdaptor<BoostingSkip>
{
public:
    SetAdaptor() { }

    ~SetAdaptor() { }

    void Init()
    {
        m_list.Init();
    }

    void Uninit()
    {
        m_list.Uninit();
    }

    bool ExecuteOps(const SetOpArray& ops)
    {
        bool ret = false;

        for(uint32_t i = 0; i < ops.size(); ++i)
        {
            uint32_t key = ops[i].key;

            if(ops[i].type == FIND)
            {
                ret = m_list.Find(key);
            }
            else if(ops[i].type == INSERT)
            {
                ret = m_list.Insert(key);
            }
            else
            {
                ret = m_list.Delete(key);
            }

            if(ret == false)
            {
                m_list.OnAbort();
                break;
            }
        }

        if(ret == true)
        {
            m_list.OnCommit();
        }

        return ret;
    }

private:
BoostingSkip m_list;
};


#endif /* end of include guard: SETADAPTOR_H */
