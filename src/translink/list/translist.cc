//------------------------------------------------------------------------------
// 
//     
//
//------------------------------------------------------------------------------


#include <cstdlib>
#include <cstdio>
#include <new>
#include "translink/list/translist.h"


#define SET_ADPINV(_p)    ((Node *)(((uintptr_t)(_p)) | 1))
#define CLR_ADPINV(_p)    ((Node *)(((uintptr_t)(_p)) & ~1))
#define IS_ADPINV(_p)     (((uintptr_t)(_p)) & 1)


TransList::TransList(Allocator<Node>* nodeAllocator, Allocator<Desc>* descAllocator)
    : m_head(new Node)
    , m_nodeAllocator(nodeAllocator)
    , m_descAllocator(descAllocator)
{}

TransList::~TransList()
{
    ASSERT_CODE
    (
        printf("Total node count %u, Inserts (total/new) %u/%u, Deletions %u, Finds %u\n", g_count, g_count_ins, g_count_ins_new, g_count_del, g_count_fnd);
        //Print();
    );

    //Node* curr = m_head;
    //while(curr != NULL)
    //{
        //free(curr);
        //curr = curr->next;
    //}
}


TransList::Desc* TransList::AllocateDesc(uint8_t size)
{
    //Desc* desc = (Desc*)malloc(sizeof(uint8_t) + sizeof(uint8_t) + sizeof(Operator) * size);
    Desc* desc = m_descAllocator->Alloc();
    desc->size = size;
    desc->status = INPROGRESS;
    
    return desc;
}

bool TransList::ExecuteOps(Desc* desc)
{
    bool ret = HelpOps(desc, 0);

    ASSERT_CODE
    (
        if(ret)
        {
            for(uint32_t i = 0; i < desc->size; ++i)
            {
                if(desc->ops[i].type == INSERT)
                {
                    __sync_fetch_and_add(&g_count, 1);
                }
                else if(desc->ops[i].type == DELETE)
                {
                    __sync_fetch_and_sub(&g_count, 1);
                }
                else
                {
                    __sync_fetch_and_add(&g_count_fnd, 1);
                }
            }
        }
    );

    return ret;
}


inline bool TransList::HelpOps(Desc* desc, uint8_t opid)
{
    bool ret = true;

    while(desc->status == INPROGRESS && ret && opid < desc->size)
    {
        const Operator& op = desc->ops[opid];

        if(op.type == INSERT)
        {
            ret = Insert(op.key, desc, opid);
        }
        else if(op.type == DELETE)
        {
            ret = Delete(op.key, desc, opid);
        }
        else
        {
            ret = Find(op.key, desc);
        }

        opid++;
    }

    if(desc->status == INPROGRESS)
    {
        if(ret == true)
        {
            __sync_bool_compare_and_swap(&desc->status, INPROGRESS, SUCCEED);
            return true;
        }
        else
        {
            __sync_bool_compare_and_swap(&desc->status, INPROGRESS, FAIL);
            return false;
        }
    }
}

inline void TransList::HelpAdopt(Node* node)
{
    Node* curr = node->adopt;

    //we skip adoption if the node itself has been logically deleted
    if(curr == NULL || IS_ADPINV(curr))
    {
        return;
    }

    if(curr->adopt == NULL)
    {
        __sync_bool_compare_and_swap(&curr->adopt, NULL, SET_ADPINV(node));
    }
    //TODO: ASSERT curr->adopt must equal to SET_ADPINV(node)

    Node* next = CLR_ADPINV(__sync_fetch_and_or(&curr->next, 0x1));

    if(node->next == NULL)
    {
        __sync_bool_compare_and_swap(&node->next, NULL, next);
    }

    __sync_bool_compare_and_swap(&node->adopt, curr, NULL);
}


inline bool TransList::Insert(uint32_t key, Desc* desc, uint8_t opid)
{
    Node* new_node = NULL;
    Node* pred = NULL;
    Node* curr = m_head;

    while(true)
    {
        LocatePred(pred, curr, key);

        if(curr)
        {
            if(IsKeyExist(curr, key, desc))
            {
                return curr->desc == desc;
            }

            if(new_node == NULL)
            {
                new_node = new(m_nodeAllocator->Alloc()) Node(key, NULL, desc, opid, NULL);
            }
            new_node->next = curr;

            //Key does not logically exisit, but physically 
            //a node containing the key exisit due to previous failed transactions
            if(curr->key == key)
            {
                new_node->adopt = curr;
                new_node->next = NULL;
                HelpAdopt(curr);
            }
        }
        else if(new_node == NULL)
        {
            new_node = new(m_nodeAllocator->Alloc()) Node(key, NULL, desc, opid, NULL);
        }

        Node* pred_next = pred->next;

        if(pred_next == curr)
        {
            pred_next = __sync_val_compare_and_swap(&pred->next, curr, new_node);

            if(pred_next == curr)
            {
                ASSERT_CODE
                (
                    __sync_fetch_and_add(&g_count_ins, 1);

                    if(new_node->adopt == NULL)
                    {
                        __sync_fetch_and_add(&g_count_ins_new, 1);
                    }
                );

                HelpAdopt(new_node);
                return true;
            }
        }

        if(IS_ADPINV(pred_next))
        {
            curr = CLR_ADPINV(pred->adopt);
            pred = NULL;
        }
        else
        {
            curr = pred;
            pred = NULL;
        }
    }
}

inline bool TransList::Delete(uint32_t key, Desc* desc, uint8_t opid)
{
    Node* new_node = new Node(key, NULL, desc, opid, NULL);
    Node* pred = NULL;
    Node* curr = m_head;

    while(true)
    {
        LocatePred(pred, curr, key);

        if(curr == NULL)
        {
            return false;
        }

        if(!IsKeyExist(curr, key, desc))
        {
            delete new_node;
            return curr->key == key && curr->desc == desc;
        }

        new_node->adopt = curr;
        HelpAdopt(curr);

        Node* pred_next = pred->next;

        if(pred_next == curr)
        {
            pred_next = __sync_val_compare_and_swap(&pred->next, curr, new_node);

            if(pred_next == curr)
            {
                HelpAdopt(new_node);
                return true;
            }
        }

        if(IS_ADPINV(pred_next))
        {
            curr = CLR_ADPINV(pred->adopt); 
            pred = NULL;
        }
        else
        {
            curr = pred;
            pred = NULL;
        }
    }
}


inline bool TransList::Find(uint32_t key, Desc* desc)
{
    Node* pred = NULL;
    Node* curr = m_head;

    LocatePred(pred, curr, key);

    return curr != NULL && IsKeyExist(curr, key, desc);
}


inline bool TransList::IsKeyExist(Node* node, uint32_t key, Desc* desc)
{
    if(node->key == key)
    {
        if(node->desc->status == INPROGRESS)
        {
            //We need to skip helping the transation if we are looking at a node from the same transaction
            //Basically, the node added by previous operations in the same transaction is visible to subsequent operaitons 
            //even if the transaction is INPROGRESS
            if(node->desc == desc)
            {
                //Without this condition check, we may run into infinite loop
                //if we have insert the same key twice within the same transaction
                return node->desc->ops[node->opid].type == INSERT;
            }
            else
            {
                HelpOps(node->desc, node->opid + 1);
            }
        }

        return  (node->desc->status == SUCCEED && node->desc->ops[node->opid].type == INSERT) ||
            (node->desc->status == FAIL && node->desc->ops[node->opid].type == DELETE);
    }
    else
    {
        return false;
    }
}


inline void TransList::LocatePred(Node*& pred, Node*& curr, uint32_t key)
{
    while(curr != NULL && curr->key < key)
    {
        pred = curr;
        HelpAdopt(curr);
        curr = CLR_ADPINV(curr->next);
    }

    ASSERT(pred, "pred must be valid");
}

inline void TransList::Print()
{
    Node* curr = m_head->next;

    while(curr)
    {
        printf("Node [%p] Key [%u] Status [%s]\n", curr, curr->key, IsKeyExist(curr, curr->key, NULL)? "active":"inactive");
        curr = curr->next;
    }
}
