#include <cstdlib>
#include <cstdio>
#include <new>
#include "translink/map/transmap.h"


#define SET_MARK(_p)    ((Node *)(((uintptr_t)(_p)) | 1))
#define CLR_MARK(_p)    ((Node *)(((uintptr_t)(_p)) & ~1))
#define CLR_MARKD(_p)    ((NodeDesc *)(((uintptr_t)(_p)) & ~1))
#define IS_MARKED(_p)     (((uintptr_t)(_p)) & 1)

__thread TransMap::HelpStack helpStack;

TransMap::TransMap(Allocator<Node>* nodeAllocator, Allocator<Desc>* descAllocator, Allocator<NodeDesc>* nodeDescAllocator, uint64_t initalPowerOfTwo, uint64_t numThreads)
    : m_nodeAllocator(nodeAllocator)
    , m_descAllocator(descAllocator)
    , m_nodeDescAllocator(nodeDescAllocator)
	//WaitFreeHashTable(int initalPowerOfTwo, int numThreads)
	{
		MAIN_SIZE				=TransMap::POW(initalPowerOfTwo);
		MAIN_POW				=initalPowerOfTwo;
		Threads					=numThreads; assert(Threads>0);
		head					=(void* /* volatile  */ *)	calloc(MAIN_SIZE,sizeof(void */* volatile  */));

#ifdef useThreadWatch
		Thread_watch				=(HASH * /* volatile  */ )	calloc(Threads,sizeof(HASH));
#endif
		Thread_pool_stack			=(void **)			calloc(Threads,sizeof(void *));
#ifdef useVectorPool
		Thread_pool_vector			=(void ***)			calloc(Threads,sizeof(void **));
		Thread_pool_vector_size		=(int *)			calloc(Threads,sizeof(int));
		for(int i=0; i<Threads; i++){
			Thread_pool_vector[i]=(void **)		calloc(10,sizeof(void *));
			Thread_pool_vector_size[i]=10;
		}
#endif
		Thread_spines			=(void **)		calloc(Threads,sizeof(void *));
		
		elements				=0;
		#ifdef SPINE_COUNT
	 		spine_elements=0;
	 	#endif
		//v_capacity=MAIN_SIZE;
		#ifdef DEBUGPRINTS_MARK
		printf("DEBUGPRINTS_MARK output enabled\n");
		#endif
	}

inline int TransMap::POW(int pow){
	int res=1;
	if(pow==0) return 1;
	else{
		while(pow!=0){
			res=res*2;
			pow--;
		}
	}
	return res;
}

TransMap::Desc* TransMap::AllocateDesc(uint8_t size)
{
    Desc* desc = m_descAllocator->Alloc();
    desc->size = size;
    desc->status = ACTIVE;
    
    return desc;
}

bool TransMap::ExecuteOps(Desc* desc)
{
    helpStack.Init();

    HelpOps(desc, 0);

    bool ret = desc->status != ABORTED;

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
                else if(desc->ops[i].type == UPDATE)
                {
                    __sync_fetch_and_add(&g_count_upd, 1); //TODO: unsure about this
                }
                else
                	__sync_fetch_and_add(&g_count_fnd, 1);
            }
        }
    );

    return ret;
}

inline void TransMap::MarkForDeletion(const std::vector<Node*>& nodes, const std::vector<Node*>& preds, Desc* desc)
{
    // Mark nodes for logical deletion
    for(uint32_t i = 0; i < nodes.size(); ++i)
    {
        Node* n = nodes[i];
        if(n != NULL)
        {
            NodeDesc* nodeDesc = n->nodeDesc;

            if(nodeDesc->desc == desc)
            {
                if(__sync_bool_compare_and_swap(&n->nodeDesc, nodeDesc, SET_MARK(nodeDesc)))
                {
                    Node* pred = preds[i];
                    Node* succ = CLR_MARK(__sync_fetch_and_or(&n->next, 0x1));
                    __sync_bool_compare_and_swap(&pred->next, n, succ);
                }
            }
        }
    }
}

inline void TransMap::HelpOps(Desc* desc, uint8_t opid)
{
    if(desc->status != ACTIVE)
    {
        return;
    }

    //Cyclic dependcy check
    if(helpStack.Contain(desc))
    {
        if(__sync_bool_compare_and_swap(&desc->status, ACTIVE, ABORTED))
        {
            __sync_fetch_and_add(&g_count_abort, 1);
            __sync_fetch_and_add(&g_count_fake_abort, 1);
        }

        return;
    }

    ReturnCode ret = OK;
    std::vector<Node*> delNodes;
    std::vector<Node*> delPredNodes;
    std::vector<Node*> insNodes;
    std::vector<Node*> insPredNodes;

    helpStack.Push(desc);

    while(desc->status == ACTIVE && ret != FAIL && opid < desc->size)
    {
        const Operator& op = desc->ops[opid];

        if(op.type == INSERT)
        {
            Node* inserted;
            Node* pred;
            ret = Insert(op.key, desc, opid, inserted, pred);

            insNodes.push_back(inserted);
            insPredNodes.push_back(pred);
        }
        else if(op.type == DELETE)
        {
            Node* deleted;
            Node* pred;
            ret = Delete(op.key, desc, opid, deleted, pred);            

            delNodes.push_back(deleted);
            delPredNodes.push_back(pred);
        }
        else
        {
            ret = Find(op.key, desc, opid);
        }
        
        opid++;
    }

    helpStack.Pop();

    if(ret != FAIL)
    {
        if(__sync_bool_compare_and_swap(&desc->status, ACTIVE, COMMITTED))
        {
            MarkForDeletion(delNodes, delPredNodes, desc);
            __sync_fetch_and_add(&g_count_commit, 1);
        }
    }
    else
    {
        if(__sync_bool_compare_and_swap(&desc->status, ACTIVE, ABORTED))
        {
            MarkForDeletion(insNodes, insPredNodes, desc);
            __sync_fetch_and_add(&g_count_abort, 1);
        }     
    }
}

inline TransMap::ReturnCode TransMap::Insert(uint32_t key, Desc* desc, uint8_t opid, Node*& inserted, Node*& pred)
{
    inserted = NULL;
    NodeDesc* nodeDesc = new(m_nodeDescAllocator->Alloc()) NodeDesc(desc, opid);
    Node* new_node = NULL;
    Node* curr = m_head;

    while(true)
    {
        // start searching for where to put our new node, it'll be between pred and curr
        LocatePred(pred, curr, key); 

        // IsNodeExist(Node* node, uint32_t key){ return node != NULL && node->key == key; }
        if(!IsNodeExist(curr, key))
        {
            //Node* pred_next = pred->next;

            if(desc->status != ACTIVE)
            {
                return FAIL;
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

                    inserted = new_node;
                    return OK;
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
                    (__sync_fetch_and_or(&curr->next, 0x1));
                }
                curr = m_head;
                continue;
            }

            FinishPendingTxn(oldCurrDesc, desc);

            if(IsSameOperation(oldCurrDesc, nodeDesc))
            {
                return SKIP;
            }

            if(!IsKeyExist(oldCurrDesc))
            {
                NodeDesc* currDesc = curr->nodeDesc;

                if(desc->status != ACTIVE)
                {
                    return FAIL;
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

                        inserted = curr;
                        return OK; 
                    }
                }
            }
            else
            {
                return FAIL;
            }
        }
    }
}

inline TransMap::ReturnCode TransMap::Delete(uint32_t key, Desc* desc, uint8_t opid, Node*& deleted, Node*& pred)
{
    deleted = NULL;
    NodeDesc* nodeDesc = new(m_nodeDescAllocator->Alloc()) NodeDesc(desc, opid);
    Node* curr = m_head;

    while(true)
    {
        LocatePred(pred, curr, key);

        if(IsNodeExist(curr, key))
        {
            NodeDesc* oldCurrDesc = curr->nodeDesc;

            if(IS_MARKED(oldCurrDesc))
            {
                return FAIL;
                //Help removed deleted nodes
                //if(!IS_MARKED(curr->next))
                //{
                    //__sync_fetch_and_or(&curr->next, 0x1);
                //}
                //curr = m_head;
                //continue;
            }

            FinishPendingTxn(oldCurrDesc, desc);

            if(IsSameOperation(oldCurrDesc, nodeDesc))
            {
                return SKIP;
            }

            if(IsKeyExist(oldCurrDesc))
            {
                NodeDesc* currDesc = curr->nodeDesc;

                if(desc->status != ACTIVE)
                {
                    return FAIL;
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

                        deleted = curr;
                        return OK; 
                    }
                }
            }
            else
            {
                return FAIL;
            }  
        }
        else 
        {
            return FAIL;      
        }
    }
}


inline bool TransMap::IsSameOperation(NodeDesc* nodeDesc1, NodeDesc* nodeDesc2)
{
    return nodeDesc1->desc == nodeDesc2->desc && nodeDesc1->opid == nodeDesc2->opid;
}


inline TransMap::ReturnCode TransMap::Find(uint32_t key, Desc* desc, uint8_t opid)
{
    NodeDesc* nodeDesc = NULL;
    Node* pred;
    Node* curr = m_head;

    while(true)
    {
        LocatePred(pred, curr, key);

        if(IsNodeExist(curr, key))
        {
            NodeDesc* oldCurrDesc = curr->nodeDesc;

            if(IS_MARKED(oldCurrDesc))
            {
                if(!IS_MARKED(curr->next))
                {
                    (__sync_fetch_and_or(&curr->next, 0x1));
                }
                curr = m_head;
                continue;
            }

            FinishPendingTxn(oldCurrDesc, desc);

            if(nodeDesc == NULL) nodeDesc = new(m_nodeDescAllocator->Alloc()) NodeDesc(desc, opid);

            if(IsSameOperation(oldCurrDesc, nodeDesc))
            {
                return SKIP;
            }

            if(IsKeyExist(oldCurrDesc))
            {
                NodeDesc* currDesc = curr->nodeDesc;

                if(desc->status != ACTIVE)
                {
                    return FAIL;
                }

                //if(currDesc == oldCurrDesc)
                {
                    //Update desc 
                    currDesc = __sync_val_compare_and_swap(&curr->nodeDesc, oldCurrDesc, nodeDesc);

                    if(currDesc == oldCurrDesc)
                    {
                        return OK; 
                    }
                }
            }
            else
            {
                return FAIL;
            }
        }
        else 
        {
            return FAIL;
        }
    }
}

inline bool TransMap::IsNodeExist(Node* node, uint32_t key)
{
    return node != NULL && node->key == key;
}

inline void TransMap::FinishPendingTxn(NodeDesc* nodeDesc, Desc* desc)
{
    // The node accessed by the operations in same transaction is always active 
    if(nodeDesc->desc == desc)
    {
        return;
    }

    HelpOps(nodeDesc->desc, nodeDesc->opid + 1);
}

inline bool TransMap::IsNodeActive(NodeDesc* nodeDesc)
{
    return nodeDesc->desc->status == COMMITTED;
}

inline bool TransMap::IsKeyExist(NodeDesc* nodeDesc)
{
    bool isNodeActive = IsNodeActive(nodeDesc);
    uint8_t opType = nodeDesc->desc->ops[nodeDesc->opid].type;

    return  (opType == FIND) || (isNodeActive && opType == INSERT) || (!isNodeActive && opType == DELETE);
}

// inline void TransMap::LocatePred(Node*& pred, Node*& curr, uint32_t key)
// {
//     Node* pred_next;

//     while(curr->key < key)
//     {
//         pred = curr;
//         pred_next = CLR_MARK(pred->next);
//         curr = pred_next;

//         while(IS_MARKED(curr->next))
//         {
//             curr = CLR_MARK(curr->next);
//         }

//         if(curr != pred_next)
//         {
//             //Failed to remove deleted nodes, start over from pred
//             if(!__sync_bool_compare_and_swap(&pred->next, pred_next, curr))
//             {
//                 curr = m_head;
//             }

//             //__sync_bool_compare_and_swap(&pred->next, pred_next, curr);
//         }
//     }

//     ASSERT(pred, "pred must be valid");
// }

inline void TransMap::Print()
{
    Node* curr = m_head->next;

    while(curr != m_tail)
    {
        printf("Node [%p] Key [%u] Status [%s]\n", curr, curr->key, IsKeyExist(CLR_MARKD(curr->nodeDesc))? "Exist":"Inexist");
        curr = CLR_MARK(curr->next);
    }
}
