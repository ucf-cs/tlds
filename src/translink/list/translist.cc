//------------------------------------------------------------------------------
// 
//     
//
//------------------------------------------------------------------------------


#include <cstdlib>
#include <cstdio>
#include <new>
#include "translink/list/translist.h"


#define SET_MARK(_p)    ((Node *)(((uintptr_t)(_p)) | 1))
#define CLR_MARK(_p)    ((Node *)(((uintptr_t)(_p)) & ~1))
#define CLR_MARKD(_p)    ((NodeDesc *)(((uintptr_t)(_p)) & ~1))
#define IS_MARKED(_p)     (((uintptr_t)(_p)) & 1)

__thread TransList::HelpStack helpStack;

TransList::TransList(Allocator<Node>* nodeAllocator, Allocator<Desc>* descAllocator, Allocator<NodeDesc>* nodeDescAllocator)
    : m_tail(new Node(0xffffffff, NULL, NULL))
    , m_head(new Node(0, m_tail, NULL))
    , m_nodeAllocator(nodeAllocator)
    , m_descAllocator(descAllocator)
    , m_nodeDescAllocator(nodeDescAllocator)
{}

TransList::~TransList()
{
    printf("Total commit %u, abort %u\n", g_count_commit, g_count_abort);
    Print();

    ASSERT_CODE
    (
        printf("Total node count %u, Inserts (total/new) %u/%u, Deletes (total/new) %u/%u, Finds %u\n", g_count, g_count_ins, g_count_ins_new, g_count_del , g_count_del_new, g_count_fnd);
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
    Desc* desc = m_descAllocator->Alloc();
    desc->size = size;
    desc->status = LIVE;
    
    return desc;
}

bool TransList::ExecuteOps(Desc* desc)
{
    helpStack.Init();

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
    std::vector<Node*> deletedNodes;

    //Cyclic dependcy check
    if(helpStack.Contain(desc))
    {
        if(__sync_bool_compare_and_swap(&desc->status, LIVE, ABORTED))
        {
            __sync_fetch_and_add(&g_count_abort, 1);
        }
        return false;
    }

    helpStack.Push(desc);

    while(desc->status == LIVE && ret && opid < desc->size)
    {
        const Operator& op = desc->ops[opid];

        if(op.type == INSERT)
        {
            ret = Insert(op.key, desc, opid);
        }
        else if(op.type == DELETE)
        {
            Node* deleted = Delete(op.key, desc, opid);
            ret = deleted != NULL;
            deletedNodes.push_back(deleted);
        }
        else
        {
            ret = Find(op.key, desc);
        }

        opid++;
    }

    helpStack.Pop();

    //if(desc->status == LIVE)
    //{
    if(ret == true)
    {
        if(__sync_bool_compare_and_swap(&desc->status, LIVE, COMMITTED))
        {
            // Mark nodes for physical deletion
            for(uint32_t i = 0; i < deletedNodes.size(); ++i)
            {
                Node* deleted = deletedNodes[i];
                if(deleted != NULL)
                {
                    NodeDesc* nodeDesc = deleted->nodeDesc;

                    if(nodeDesc->desc == desc)
                    {
                        if(__sync_bool_compare_and_swap(&deleted->nodeDesc, nodeDesc, SET_MARK(nodeDesc)))
                        {
                            __sync_fetch_and_or(&deleted->next, 0x1);
                        }
                    }
                }
            }

            __sync_fetch_and_add(&g_count_commit, 1);
        }
        return true;
    }
    else
    {
        if(__sync_bool_compare_and_swap(&desc->status, LIVE, ABORTED))
        {
            __sync_fetch_and_add(&g_count_abort, 1);
        }
        return false;
    }
    //}
}

inline bool TransList::Insert(uint32_t key, Desc* desc, uint8_t opid)
{
    NodeDesc* nodeDesc = new(m_nodeDescAllocator->Alloc()) NodeDesc(desc, opid);
    Node* new_node = NULL;
    Node* pred;
    Node* curr = m_head;

    while(true)
    {
        LocatePred(pred, curr, key);

        if(!IsNodeExist(curr, key))
        {
            //Node* pred_next = pred->next;

            if(desc->status != LIVE)
            {
                return false;
            }

            //if(pred_next == curr)
            //{
                if(new_node == NULL)
                {
                    new_node = new(m_nodeAllocator->Alloc()) Node(key, NULL, nodeDesc);
                }
                new_node->next = curr;

                Node* pred_next = __sync_val_compare_and_swap(&pred->next, curr, new_node);

                if(pred_next == curr)
                {
                    ASSERT_CODE
                        (
                         __sync_fetch_and_add(&g_count_ins, 1);
                         __sync_fetch_and_add(&g_count_ins_new, 1);
                        );

                    return true;
                }
            //}

            // Restart
            curr = IS_MARKED(pred_next) ? m_head : pred;
        }
        else 
        {
            NodeDesc* oldCurrDesc = curr->nodeDesc;

            if(IS_MARKED(oldCurrDesc))
            {
                if(!IS_MARKED(curr->next))
                {
                    __sync_fetch_and_or(&curr->next, 0x1);
                }
                curr = m_head;
                continue;
            }

            if(!FinishPendingTxn(oldCurrDesc, desc))
            {
                return false;
            }

            if(!IsSameOperation(oldCurrDesc, nodeDesc) && !IsKeyExist(oldCurrDesc, desc))
            {
                NodeDesc* currDesc = curr->nodeDesc;

                if(desc->status != LIVE)
                {
                    return false;
                }

                //if(currDesc == oldCurrDesc)
                {
                    //Update desc 
                    currDesc = __sync_val_compare_and_swap(&curr->nodeDesc, oldCurrDesc, nodeDesc);

                    if(currDesc == oldCurrDesc)
                    {
                        ASSERT_CODE
                            (
                             __sync_fetch_and_add(&g_count_ins, 1);
                            );

                        return true; 
                    }
                }
            }
            else
            {
                return false;
            }
        }
    }
}

inline TransList::Node* TransList::Delete(uint32_t key, Desc* desc, uint8_t opid)
{
    NodeDesc* nodeDesc = new(m_nodeDescAllocator->Alloc()) NodeDesc(desc, opid);
    Node* new_node = NULL;
    Node* pred;
    Node* curr = m_head;

    while(true)
    {
        LocatePred(pred, curr, key);

        if(!IsNodeExist(curr, key))
        {
            Node* pred_next = pred->next;
            
            // Need this check to make sure txn is still live after we read pred_next
            if(desc->status != LIVE)
            {
                return NULL;
            }

            //if(pred_next == curr)
            //{
            if(new_node == NULL)
            {
                new_node = new(m_nodeAllocator->Alloc()) Node(key, NULL, nodeDesc);
            }
            new_node->next = curr;

            pred_next = __sync_val_compare_and_swap(&pred->next, curr, new_node);

            if(pred_next == curr)
            {
                ASSERT_CODE
                    (
                     __sync_fetch_and_add(&g_count_del, 1);
                     __sync_fetch_and_add(&g_count_del_new, 1);
                    );

                return NULL;
            }
            //}

            // Restart
            curr = IS_MARKED(pred_next) ? m_head : pred;
        }
        else 
        {
            NodeDesc* oldCurrDesc = curr->nodeDesc;

            if(IS_MARKED(oldCurrDesc))
            {
                if(!IS_MARKED(curr->next))
                {
                    __sync_fetch_and_or(&curr->next, 0x1);
                }
                curr = m_head;
                continue;
            }

            if(!FinishPendingTxn(oldCurrDesc, desc))
            {
                return NULL;
            }

            if(!IsSameOperation(oldCurrDesc, nodeDesc) && IsKeyExist(oldCurrDesc, desc))
            {
                NodeDesc* currDesc = curr->nodeDesc;

                if(desc->status != LIVE)
                {
                    return NULL;
                }

                //if(currDesc == oldCurrDesc)
                {
                    //Update desc 
                    currDesc = __sync_val_compare_and_swap(&curr->nodeDesc, oldCurrDesc, nodeDesc);

                    if(currDesc == oldCurrDesc)
                    {
                        ASSERT_CODE
                            (
                             __sync_fetch_and_add(&g_count_del, 1);
                            );

                        return curr; 
                    }
                }
            }
            else
            {
                return NULL;
            }    
        }
    }
}


inline bool TransList::IsSameOperation(NodeDesc* nodeDesc1, NodeDesc* nodeDesc2)
{
    return nodeDesc1->desc == nodeDesc2->desc && nodeDesc1->opid == nodeDesc2->opid;
}


inline bool TransList::Find(uint32_t key, Desc* desc)
{
    Node* pred = NULL;
    Node* curr = m_head;

    LocatePred(pred, curr, key);

    return false;
}

inline bool TransList::IsNodeExist(Node* node, uint32_t key)
{
    return node != NULL && node->key == key;
}

inline bool TransList::FinishPendingTxn(NodeDesc* nodeDesc, Desc* desc)
{
    // The node accessed by the operations in same transaction is always active 
    if(nodeDesc->desc == desc)
    {
        return true;
    }

    if(nodeDesc->desc->status == LIVE)
    {
        HelpOps(nodeDesc->desc, nodeDesc->opid + 1);
    }

    return true;
}

inline bool TransList::IsNodeActive(NodeDesc* nodeDesc, Desc* desc)
{
    return nodeDesc->desc->status == COMMITTED;
}

inline bool TransList::IsKeyExist(NodeDesc* nodeDesc, Desc* desc)
{
    bool isNodeActive = IsNodeActive(nodeDesc, desc);
    uint8_t opType = nodeDesc->desc->ops[nodeDesc->opid].type;

    return  (isNodeActive && opType == INSERT) || (!isNodeActive && opType == DELETE);
}

inline void TransList::LocatePred(Node*& pred, Node*& curr, uint32_t key)
{
    Node* pred_next;

    while(curr->key < key)
    {
        pred = curr;
        pred_next = CLR_MARK(pred->next);
        curr = pred_next;

        while(IS_MARKED(curr->next))
        {
            curr = CLR_MARK(curr->next);
        }

        if(curr != pred_next)
        {
            //Failed to remove deleted nodes, start over from pred
            //if(!__sync_bool_compare_and_swap(&pred->next, pred_next, curr))
            //{
                //curr = pred;
            //}

            __sync_bool_compare_and_swap(&pred->next, pred_next, curr);
        }
    }

    ASSERT(pred, "pred must be valid");
}

inline void TransList::Print()
{
    Node* curr = m_head->next;

    while(curr != m_tail)
    {
        printf("Node [%p] Key [%u] Status [%s]\n", curr, curr->key, IsKeyExist(CLR_MARKD(curr->nodeDesc), NULL)? "Exist":"Inexist");
        curr = CLR_MARK(curr->next);
    }
}
