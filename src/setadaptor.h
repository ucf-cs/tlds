#ifndef SETADAPTOR_H
#define SETADAPTOR_H

#include <array>
#include "translist/translist.h"

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
    SetAdaptor()
    {}

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


#endif /* end of include guard: SETADAPTOR_H */
