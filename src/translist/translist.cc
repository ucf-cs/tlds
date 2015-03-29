//------------------------------------------------------------------------------
// 
//     
//
//------------------------------------------------------------------------------


#include <cstdlib>
#include <cstdio>
#include "translist/translist.h"
#include "common/assert.h"


#define SET_ADPINV(_p)    ((Node *)(((uintptr_t)(_p)) | 1))
#define CLR_ADPINV(_p)    ((Node *)(((uintptr_t)(_p)) & ~1))
#define IS_ADPINV(_p)     (((uintptr_t)(_p)) & 1)


TransList::TransList()
    : m_head(new Node)
{
}

TransList::~TransList()
{
    ASSERT_CODE(Print(););

    Node* curr = m_head;
    while(curr != NULL)
    {
        free(curr);
        curr = curr->next;
    }
}


TransList::Desc* TransList::AllocateDesc(uint8_t size)
{
    Desc* desc = (Desc*)malloc(sizeof(uint8_t) + sizeof(uint8_t) + sizeof(Operator) * size);
    desc->size = size;
    desc->status = INPROGRESS;
    
    return desc;
}


bool TransList::ExecuteOps(Desc* desc)
{
    return HelpOps(desc, 0);
}


inline bool TransList::HelpOps(Desc* desc, uint8_t opid)
{
    bool ret = true;

    while(ret && opid < desc->size)
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
            ret = Find(op.key);
        }

        opid++;
    }

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

inline void TransList::HelpAdopt(Node* node)
{
    Node* curr = node->adopt;
    if(curr == NULL)
    {
        return;
    }

    Node* next = CLR_ADPINV(__sync_fetch_and_or(&curr->next, 0x1));

    if(node->next == NULL)
    {
        __sync_bool_compare_and_swap(&node->next, NULL, next);
    }

    node->adopt = NULL;
}


inline bool TransList::Insert(uint32_t key, Desc* desc, uint8_t opid)
{

    Node* new_node = new Node(key, NULL, desc, opid, NULL);
    Node* pred = NULL;
    Node* curr = m_head;

    while(true)
    {
        LocatePred(pred, curr, key);

        if(curr)
        {
            new_node->next = curr;

            if(IsKeyExist(curr, key))
            {
                delete new_node;
                return curr->desc == desc;
            }

            //Key does not logically exisit, but physically 
            //a node containing the key exisit due to previous failed transactions
            if(curr->key == key)
            {
                new_node->adopt = curr;
                new_node->next = NULL;
                HelpAdopt(curr);
            }
        }

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
            pred = NULL;
            curr = CLR_ADPINV(pred_next);
            //TODO: we are duplexing next field for backtracking
            //Need modify HelpAdopt to support this
        }
        else
        {
            pred = NULL;
            curr = pred;
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

        if(curr == NULL || !IsKeyExist(curr, key))
        {
            delete new_node;
            return curr == NULL || (curr->key == key && curr->desc == desc);
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
            pred = NULL;
            curr = CLR_ADPINV(pred_next); 
            //TODO: we are duplexing next field for backtracking
            //Need modify HelpAdopt to support this
        }
        else
        {
            pred = NULL;
            curr = pred;
        }
    }
}


inline bool TransList::Find(uint32_t key)
{
    Node* pred = NULL;
    Node* curr = m_head;

    LocatePred(pred, curr, key);

    return curr != NULL && IsKeyExist(curr, key);
}


inline bool TransList::IsKeyExist(Node* node, uint32_t key)
{
    if(node->key == key)
    {
        if(node->desc->status == INPROGRESS)
        {
            HelpOps(node->desc, node->opid + 1);
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
        curr = curr->next;
    }
}

inline void TransList::Print()
{
    Node* curr = m_head->next;

    while(curr)
    {
        printf("Node [%p] Key [%u] Status [%s]\n", curr, curr->key, IsKeyExist(curr, curr->key)? "active":"inactive");
        curr = curr->next;
    }
}
