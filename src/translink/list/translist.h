#ifndef TRANSLIST_H
#define TRANSLIST_H

#include <cstdint>
#include <vector>
#include "common/assert.h"
#include "common/allocator.h"

class TransList
{
public:
    enum OpStatus
    {
        LIVE = 0,
        COMMITTED,
        ABORTED
    };

    enum OpType
    {
        FIND = 0,
        INSERT,
        DELETE
    };

    struct Operator
    {
        uint8_t type;
        uint32_t key;
    };

    struct Desc
    {
        static size_t SizeOf(uint8_t size)
        {
            return sizeof(uint8_t) + sizeof(uint8_t) + sizeof(Operator) * size;
        }

        volatile uint8_t status;
        uint8_t size;
        Operator ops[];
    };
    
    struct NodeDesc
    {
        NodeDesc(Desc* _desc, uint8_t _opid)
            : desc(_desc), opid(_opid){}

        Desc* desc;
        uint8_t opid;
    };

    struct Node
    {
        Node(): key(0), next(NULL), nodeDesc(NULL){}
        Node(uint32_t _key, Node* _next, NodeDesc* _nodeDesc)
            : key(_key), next(_next), nodeDesc(_nodeDesc){}

        uint32_t key;
        Node* next;
        
        NodeDesc* nodeDesc;
    };

    struct HelpStack
    {
        void Init()
        {
            index = 0;
        }

        void Push(Desc* desc)
        {
            ASSERT(index < 255, "index out of range");

            helps[index++] = desc;
        }

        void Pop()
        {
            ASSERT(index > 0, "nothing to pop");

            index--;
        }

        bool Contain(Desc* desc)
        {
            for(uint8_t i = 0; i < index; i++)
            {
                if(helps[i] == desc)
                {
                    return true;
                }
            }

            return false;
        }

        Desc* helps[256];
        uint8_t index;
    };

    TransList(Allocator<Node>* nodeAllocator, Allocator<Desc>* descAllocator, Allocator<NodeDesc>* nodeDescAllocator);
    ~TransList();

    bool ExecuteOps(Desc* desc);

    Desc* AllocateDesc(uint8_t size);

private:
    bool Insert(uint32_t key, Desc* desc, uint8_t opid);
    Node* Delete(uint32_t key, Desc* desc, uint8_t opid);
    bool Find(uint32_t key, Desc* desc);

    bool HelpOps(Desc* desc, uint8_t opid);
    bool IsSameOperation(NodeDesc* nodeDesc1, NodeDesc* nodeDesc2);
    bool FinishPendingTxn(NodeDesc* nodeDesc, Desc* desc);
    bool IsNodeExist(Node* node, uint32_t key);
    bool IsNodeActive(NodeDesc* nodeDesc, Desc* desc);
    bool IsKeyExist(NodeDesc* nodeDesc, Desc* desc);
    void LocatePred(Node*& pred, Node*& curr, uint32_t key);

    void Print();

private:
    Node* m_tail;
    Node* m_head;

    Allocator<Node>* m_nodeAllocator;
    Allocator<Desc>* m_descAllocator;
    Allocator<NodeDesc>* m_nodeDescAllocator;

    ASSERT_CODE
    (
        uint32_t g_count = 0;
        uint32_t g_count_ins = 0;
        uint32_t g_count_ins_new = 0;
        uint32_t g_count_del = 0;
        uint32_t g_count_del_new = 0;
        uint32_t g_count_fnd = 0;
    )

    uint32_t g_count_commit = 0;
    uint32_t g_count_abort = 0;
};

#endif /* end of include guard: TRANSLIST_H */    
