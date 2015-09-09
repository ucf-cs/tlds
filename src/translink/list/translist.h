#ifndef TRANSLIST_H
#define TRANSLIST_H

#include <cstdint>
#include "common/assert.h"
#include "common/allocator.h"

class TransList
{
public:
    enum OpStatus
    {
        INPROGRESS = 0,
        SUCCEED,
        FAIL
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

        uint8_t status;
        uint8_t size;
        Operator ops[];
    };

    struct Node
    {
        Node(): key(0), next(NULL), desc(NULL), opid(0), adopt(NULL){}
        Node(uint32_t _key, Node* _next, Desc* _desc, uint8_t _opid, Node* _adopt)
            : key(_key), next(_next), desc(_desc), opid(_opid), adopt(_adopt){}

        uint32_t key;
        Node* next;

        Desc* desc;
        uint8_t opid; //TODO: maybe store a copy of operator in node
        Node* adopt;
    };

    TransList(Allocator<Node>* nodeAllocator, Allocator<Desc>* descAllocator);
    ~TransList();

    bool ExecuteOps(Desc* desc);

    Desc* AllocateDesc(uint8_t size);

private:
    bool Insert(uint32_t key, Desc* desc, uint8_t opid);
    bool Delete(uint32_t key, Desc* desc, uint8_t opid);
    bool Find(uint32_t key, Desc* desc);

    bool HelpOps(Desc* desc, uint8_t opid);
    void HelpAdopt(Node* node);
    bool IsKeyExist(Node* node, uint32_t key, Desc* desc);
    void LocatePred(Node*& pred, Node*& curr, uint32_t key);

    void Print();

private:
    Node* m_head;

    Allocator<Node>* m_nodeAllocator;
    Allocator<Desc>* m_descAllocator;

    ASSERT_CODE
    (
        uint32_t g_count = 0;
        uint32_t g_count_ins = 0;
        uint32_t g_count_del = 0;
        uint32_t g_count_fnd = 0;
    )
};

#endif /* end of include guard: TRANSLIST_H */    
