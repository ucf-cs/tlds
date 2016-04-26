#ifndef SETADAPTOR_H
#define SETADAPTOR_H

#include "translink/list/translist.h"
#include "rstm/list/rstmlist.hpp"
#include "boosting/list/boostinglist.h"
#include "common/allocator.h"

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
        , m_list(&m_nodeAllocator, &m_descAllocator)
    { }

    void Init()
    {
        m_descAllocator.Init();
        m_nodeAllocator.Init();
    }

    void Uninit(){}

    bool ExecuteOps(const SetOpArray& ops)
    {
        //TransList::Desc* desc = m_list.AllocateDesc(ops.size());
        TransList::Desc* desc = m_descAllocator.Alloc();
        desc->size = ops.size();
        desc->status = TransList::INPROGRESS;

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
    TransList m_list;
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
        bool ret = false;

        TM_BEGIN(atomic)
        {
            ret = false;
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
                    break;
                }
            }
        } 
        TM_END;

        return ret;
    }

private:
    RSTMList m_list;
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

#endif /* end of include guard: SETADAPTOR_H */