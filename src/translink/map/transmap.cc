#include <cstdlib>
#include <cstdio>
#include <new>
#include "translink/map/transmap.h"


#define SET_MARK(_p)    ((Node *)(((uintptr_t)(_p)) | 1))
#define CLR_MARK(_p)    ((Node *)(((uintptr_t)(_p)) & ~1))
#define CLR_MARKD(_p)    ((NodeDesc *)(((uintptr_t)(_p)) & ~1))
#define IS_MARKED(_p)     (((uintptr_t)(_p)) & 1)


__thread TransMap::HelpStack mapHelpStack;
//TODO: make __thread std::vector<VALUE> toR; where each thread can put the results of its find operations and return them to the user

	bool TransMap::ExecuteOps(Desc* desc, int threadId)//, std::vector<VALUE> &toR)
	{
	    mapHelpStack.Init();

	    HelpOps(desc, 0, threadId);//, toR);

	    bool ret = desc->status != MAP_ABORTED;

	    ASSERT_CODE
	    (
	        if(ret)
	        {
	            for(uint32_t i = 0; i < desc->size; ++i)
	            {
	                if(desc->ops[i].type == MAP_INSERT)
	                {
	                    __sync_fetch_and_add(&g_count, 1);
	                }
	                else if(desc->ops[i].type == MAP_DELETE)
	                {
	                    __sync_fetch_and_sub(&g_count, 1);
	                }
	                else if(desc->ops[i].type == MAP_UPDATE)
	                {
	                    __sync_fetch_and_add(&g_count_upd, 1); //TODO: unsure about this, should be &g_count?
	                }
	                else
	                	__sync_fetch_and_add(&g_count_fnd, 1);
	            }
	        }
	    );

	    return ret;
	}

	inline void TransMap::HelpOps(Desc* desc, uint8_t opid, int T)//, std::vector<VALUE> &toR)
	{
	    if(desc->status != MAP_ACTIVE)
	    {
	        return;//return NULL;
	    }

	    //Cyclic dependcy check
	    if(mapHelpStack.Contain(desc))
	    {
	        if(__sync_bool_compare_and_swap(&desc->status, MAP_ACTIVE, MAP_ABORTED))
	        {
	            __sync_fetch_and_add(&g_count_abort, 1);
	            __sync_fetch_and_add(&g_count_fake_abort, 1);
	        }

	        return;//return NULL;
	    }

	    bool ret = true;
	    std::vector<DataNode*> retVector;
	    //std::vector<VALUE> foundValues;

	    mapHelpStack.Push(desc);

	    while(desc->status == MAP_ACTIVE && ret != false && opid < desc->size)
	    {
	        const Operator& op = desc->ops[opid];

	        if(op.type == MAP_INSERT)
	        {
	            ret = Insert(desc, opid, op.key, op.value, T);
	        }
	        else if(op.type == MAP_DELETE)
	        {
	            //ret = Delete(op.key, desc, opid);
	            ret = Delete(desc, opid, op.key, T);
	        }
	        else if(op.type == MAP_UPDATE)
	        {
	        	//TODO: leftoff here, should i change toRet to use pointers? pass pointers instead of refs?
	        	// TODO: or each thread should have a threadlocal retVector vector like their helpstack?
	        	// TODO: how does helping affect the use of toRet? if transaction B helps transaction A do its update, does transaction A get a valid toRet pointer?
	        	// this pointer is passed by reference to the update function
	        	//TODO: instead of making it complicated just add an oldvalue field to the nodeDesc and use that to do logical interpretation
	        	//TODO: if i do this how do i keep track if a find modifies the node after the update, how do i know to write the old value if i'm looking at a node pointing to a find
	        		// i could iterate through the transaction descriptor and see where it wanted to do write operations, but i only get keys there, so i'd have to save the corresponding nodes for all update operations
	        		// but that brings me back to my first problem unless i store a pointer to the node in the transaction descriptor with the operation information (next to the key, value, opid, old value)
	        			// would old value still be necessary in this case?
	        	DataNode* toRet;
	        	ret = Update(desc, opid, op.key, op.value, T, toRet);
	        	// the pointer is copied into the vector
	        	retVector.push_back(toRet);
	        }
	        else
	        {
	            // ret = Find(op.key, desc, opid);
	            // if find is successful it returns a non-null value
	            VALUE retVal = Find(desc, opid, op.key, T);

	        	if (retVal == (VALUE)NULL)
	        		ret = false;
	        	ret = true;
	        	//if (retVal != 0)
	        		//toR.push_back(retVal);//foundValues.push_back(retVal);
	        	// TODO: edit helpops to return an array of values?
	        }
	        
	        opid++;
	    }

	    mapHelpStack.Pop();

	    if(ret != false)
	    {
	    	//requires c++11 TODO: make sure makefile compiles with that std
	    	// any concurrent txn will see that ours is live and not use/modify our nodes
	    	// there are now no more concurrent operations from our transaction to use/modify nodes we've previously updated
	    	// before commit update the node values if our transaction owns them via their desc aliasing that of our txn
	        for(DataNode* x: retVector)
	        {
	        	// everything must have returned successfully to get here, so if the last operation was an update then we
	        	// copy the new value in, if it was an insert we overwrite the value with itself, if it was a find
	        	// NOTE: sometimes x->nodeDesc is NULL, should this be happening? is this a result of problems with the m_desc and m_nodedesc allocators? because x->nodedesc->desc can apparently also be null
	        	// get rid of m_desc allocator and check for && x->nodeDesc->desc != NULL which was not hit before problem with not accessing memory arose
	        	if (x != NULL && x->nodeDesc != NULL ) 
	        	{
		        	if(x->nodeDesc->desc == desc)//&& x->nodeDesc->desc->ops[x->nodeDesc->opid].type == MAP_UPDATE)
		        	{
		        		if (x->nodeDesc->desc->ops[x->nodeDesc->opid].type == MAP_UPDATE || 
		        			(x->nodeDesc->desc->ops[x->nodeDesc->opid].type == MAP_FIND && x->nodeDesc->desc->ops[x->nodeDesc->opid].value != 0) )//nodeDesc->value != 0) )
		        		{
		        			x->value = x->nodeDesc->desc->ops[x->nodeDesc->opid].value;//x->nodeDesc->value;
		        		}
		        	}
		        }
	        }

	        if(__sync_bool_compare_and_swap(&desc->status, MAP_ACTIVE, MAP_COMMITTED))
	        {
	            //MarkForDeletion(delNodes, delPredNodes, desc);

	            __sync_fetch_and_add(&g_count_commit, 1);
	        }
	        // else
        	// {
        	// 	printf("999999999999999999999999\n");
        	// 	exit(9999);
        	// }
	        	// NOTE: if this happens it means the updates need to be undone here
	        //note: i think this might just mean that some other thread committed the transaction
	        return;//return foundValues;
	    }
	    else
	    {
	    	// never updated node values, so don't have to undo those; they'll be interpreted correctly
	        if(__sync_bool_compare_and_swap(&desc->status, MAP_ACTIVE, MAP_ABORTED))
	        {
	            //MarkForDeletion(insNodes, insPredNodes, desc);
	            __sync_fetch_and_add(&g_count_abort, 1);
	        }
	        return;//return NULL;
	    }
	}





