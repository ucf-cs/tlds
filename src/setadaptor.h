#ifndef SETADAPTOR_H
#define SETADAPTOR_H

#include <array>
#include "translist/translist.h"

enum SetOp
{
    INSERT,
    DELETE,
    FIND
};

typedef std::array<SetOp, 4> OpArray;

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

    bool ExecuteOp(const OpArray& ops)
    {
        for(uint32_t i = 0; i < ops.size(); ++i)
        {
        }
        return false;
    }

private:
    TransList m_list;
};


#endif /* end of include guard: SETADAPTOR_H */
