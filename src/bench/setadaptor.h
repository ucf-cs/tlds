#ifndef SETADAPTOR_H
#define SETADAPTOR_H

#include <array>
#include "translink/list/translist.h"
#include "rstm/list/rstmlist.hpp"

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
    SetAdaptor(){}

    void Init(){}
    void Uninit(){}

    bool ExecuteOps(const SetOpArray& ops)
    {
        TransList::Desc* desc = m_list.AllocateDesc(ops.size());

        for(uint32_t i = 0; i < ops.size(); ++i)
        {
            desc->ops[i].type = ops[i].type; 
            desc->ops[i].key = ops[i].key; 
        }

        return m_list.ExecuteOps(desc);
    }

private:
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


#endif /* end of include guard: SETADAPTOR_H */
