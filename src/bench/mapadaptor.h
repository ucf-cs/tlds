#ifndef MAPADAPTOR_H
#define MAPADAPTOR_H

#include "common/allocator.h" //might have to move this lower?

#include "translink/map/transmap.h"

enum MapOpType
{
    FIND = 0,
    INSERT,
    DELETE,
    UPDATE
};

struct MapOperator
{
    uint8_t type;
    uint32_t key;
    uint32_t value;
};

enum MapOpStatus
{
    LIVE = 0,
    COMMITTED,
    ABORTED
};

typedef std::vector<MapOperator> MapOpArray;

template<typename T>
class MapAdaptor
{
};

template<>
class MapAdaptor<TransMap>
{
public:
    MapAdaptor(uint64_t cap, uint64_t threadCount, uint32_t transSize)
        : m_descAllocator(cap * threadCount * TransMap::Desc::SizeOf(transSize), threadCount, TransMap::Desc::SizeOf(transSize))
        //, m_nodeAllocator(cap * threadCount *  sizeof(TransMap::Node) * transSize, threadCount, sizeof(TransMap::Node))
        , m_nodeDescAllocator(cap * threadCount *  sizeof(TransMap::NodeDesc) * transSize, threadCount, sizeof(TransMap::NodeDesc))
        , m_map(&m_nodeAllocator, &m_descAllocator, &m_nodeDescAllocator, cap, threadCount)
    { }

    void Init()
    {
        m_descAllocator.Init();
        m_nodeAllocator.Init();
        m_nodeDescAllocator.Init();
    }

    void Uninit(){}

    bool ExecuteOps(const MapOpArray& ops)
    {
        //TransMap::Desc* desc = m_map.AllocateDesc(ops.size());
        TransMap::Desc* desc = m_descAllocator.Alloc();
        desc->size = ops.size();
        desc->status = TransMap::ACTIVE;

        for(uint32_t i = 0; i < ops.size(); ++i)
        {
            desc->ops[i].type = ops[i].type; 
            desc->ops[i].key = ops[i].key; 
            desc->ops[i].value = ops[i].value; 
        }

        return m_map.ExecuteOps(desc);
    }

private:
    Allocator<TransMap::Desc> m_descAllocator;
    Allocator<TransMap::Node> m_nodeAllocator;
    Allocator<TransMap::NodeDesc> m_nodeDescAllocator;
    TransMap m_map;
};


#endif /* end of include guard: MAPADAPTOR_H */