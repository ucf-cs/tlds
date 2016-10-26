#include <cstdlib>
#include <cstdio>
#include <new>
#include "translink/map/transmap.h"


#define SET_MARK(_p)    ((Node *)(((uintptr_t)(_p)) | 1))
#define CLR_MARK(_p)    ((Node *)(((uintptr_t)(_p)) & ~1))
#define CLR_MARKD(_p)    ((NodeDesc *)(((uintptr_t)(_p)) & ~1))
#define IS_MARKED(_p)     (((uintptr_t)(_p)) & 1)

__thread TransMap::HelpStack helpStack;

TransMap::TransMap(/*Allocator<Node>* nodeAllocator,*/ Allocator<Desc>* descAllocator, Allocator<NodeDesc>* nodeDescAllocator, uint64_t initalPowerOfTwo, uint64_t numThreads)
    :  m_descAllocator(descAllocator) //m_nodeAllocator(nodeAllocator),
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

bool TransMap::ExecuteOps(Desc* desc, int threadId)
{
    helpStack.Init();

    HelpOps(desc, 0, threadId);

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
                    __sync_fetch_and_add(&g_count_upd, 1); //TODO: unsure about this, should be &g_count?
                }
                else
                	__sync_fetch_and_add(&g_count_fnd, 1);
            }
        }
    );

    return ret;
}

inline VALUE TransMap::GetValue(NodeDesc* oldCurrDesc)
{
	return oldCurrDesc->desc->ops[oldCurrDesc->opid].value;
}

inline std::vector<VALUE> TransMap::HelpOps(Desc* desc, uint8_t opid, int threadId)
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

    bool ret = true;
    std::vector<DataNode*> retVector;
    std::vector<VALUE> foundValues;

    helpStack.Push(desc);

    while(desc->status == ACTIVE && ret != FAIL && opid < desc->size)
    {
        const Operator& op = desc->ops[opid];

        if(op.type == INSERT)
        {
            ret = Insert(desc, opid, op.key, op.value, T);
        }
        else if(op.type == DELETE)
        {
            //ret = Delete(op.key, desc, opid);
            ret = Delete(desc, opid, op.key, T);
        }
        else if(op.type == UPDATE)
        {
        	// this pointer is passed by reference to the update function
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
        	if (retVal != 0)
        		foundValues.push_back(retVal);
        	// TODO: edit helpops to return an array of values?
        }
        
        opid++;
    }

    helpStack.Pop();

    if(ret != false)
    {
    	//requires c++11 TODO: make sure makefile compiles with that std
    	// any concurrent txn will see that ours is live and not use/modify our nodes
    	// there are now no more concurrent operations from our transaction to use/modify nodes we've previously updated
    	// before commit update the node values if our transaction owns them via their desc aliasing that of our txn
        for(auto x: retVector)
        {
        	// everything must have returned successfully to get here, so if the last operation was an update then we
        	// copy the new value in, if it was an insert we overwrite the value with itself, if it was a find 
        	if(x->nodeDesc->desc == desc)//&& x->nodeDesc->desc->ops[x->nodeDesc->opid].type == UPDATE)
        	{
        		if (x->nodeDesc->desc->ops[x->nodeDesc->opid].type == UPDATE || 
        			(x->nodeDesc->desc->ops[x->nodeDesc->opid].type == FIND && x->nodeDesc->desc->ops[nodeDesc->opid].value != 0) )//nodeDesc->value != 0) )
        		{
        			x->value = x->nodeDesc->desc->ops[nodeDesc->opid].value;//x->nodeDesc->value;
        		}
        	}
        }

        if(__sync_bool_compare_and_swap(&desc->status, ACTIVE, COMMITTED))
        {
            //MarkForDeletion(delNodes, delPredNodes, desc);

            __sync_fetch_and_add(&g_count_commit, 1);
        }
        else
        	exit(9999);
        	// NOTE: if this happens it means the updates need to be undone here
        return foundValues;
    }
    else
    {
    	// never updated node values, so don't have to undo those; they'll be interpreted correctly
        if(__sync_bool_compare_and_swap(&desc->status, ACTIVE, ABORTED))
        {
            //MarkForDeletion(insNodes, insPredNodes, desc);
            __sync_fetch_and_add(&g_count_abort, 1);
        }
        return NULL;
    }
}

//NOTE: update the logical status of the node (by updating the nodedesc) before any low-level operations on that node
// this refers only to data nodes, spine nodes are not included because once inserted they must remain inserted
// to make the underlying algorithms work without extensive refactoring of the code which did not originally permit
// spine nodes to be deallocated under any circumstances

//TODO: make sure that nodedesc is updated before all low-level data node manipulations

inline bool TransMap::Insert(Desc* desc, uint8_t opid, KEY k, VALUE v, int T)
{
    //inserted = NULL;

    NodeDesc* nodeDesc = new(m_nodeDescAllocator->Alloc()) NodeDesc(desc, opid);
    
	HASH hash=HASH_KEY(k);//reorders the bits in the key to more evenly distribute the bits
	#ifdef useThreadWatch
		Thread_watch[T]=hash;//Puts the hash in the watchlist
	#endif
	
	//Allocates a bucket, then stores the value, key and hash into it
	#ifdef USE_KEY
		DataNode *temp_bucket=Allocate_Node(v,k,hash,T, nodeDesc);
	#else
		DataNode *temp_bucket=Allocate_Node(v,hash,T, nodeDesc);
	#endif
	#ifdef DEBUG
	assert(temp_bucket!=NULL);
	#endif
	
	bool res=putIfAbsent_main(hash,temp_bucket,T, nodeDesc);
	if(!res){
		Free_Node_Stack(temp_bucket, T);
	}
		
	#ifdef useThreadWatch
		Thread_watch[T]=0;//Removes the hash from the watchlist
	#endif	
	return res;
}

inline bool putIfAbsent_main(HASH hash,DataNode *temp_bucket, int T, NodeDesc* nodedesc){
	
	//This count bounds the number of times the thread will loop as a result of CAS failure.
	int cas_fail_count=0;
insert_main:
	//Determines the position to insert at by examining the "M" right most bits
	int pos=getMAINPOS(hash);//&(MAIN_SIZE-1));
	#ifdef DEBUG
	assert(pos >=0 && pos <MAIN_SIZE);//Check to make sure position is valid
	#endif

	void *node=getNodeRaw(head,pos);//Gets the pointer value at the position of interest
	//Examining Main Spine First
	while(true){
		
		if(cas_fail_count>=MAX_CAS_FAILURE){//Checks to see if it failed the CAS to many times
			mark_data_node(head,pos);//If it has then it marks that node
				#ifdef DEBUGPRINTS_MARK
				printf("Marked a Node--Main\n");
				#endif
		}
		
		void *node2;
		if(node==NULL){//See Logic Above
			if( (node2=replace_node(head, pos, NULL, temp_bucket))==NULL){
				increment_size();//Increments the size of table
				return true;
			}	
			else{ //NOTE: don't need to check isKeyExist for a spine node, because they are never deleted under any circumstances
				if( /*node2!=NULL &&*/ isSpine(node2)){//If it is a Spine then continue, we don't know if the key was updated.
					return putIfAbsent_sub(unmark_spine(node2), temp_bucket, T, nodedesc);
				}
				else{
					cas_fail_count++;
					node=getNodeRaw(head,pos);//Gets the pointer value at the position of interest

					continue;
				}
				
			}
		}
		else if(isSpine(node)){//Check the Sub Spines
			return putIfAbsent_sub(unmark_spine(node), temp_bucket, T, nodedesc);
			
		}
		else if(isMarkedData(node)){//Force Expand The table because someone could not pass the cas
		    #ifdef DEBUGPRINTS
		    printf("marked found on main!\n");
		    #endif
		    node=forceExpandTable(T,head,pos,unmark_data(node), MAIN_POW);
			return putIfAbsent_sub(unmark_spine(node), temp_bucket, T, nodedesc);
		}
		else{//It is a Data Node
			#ifdef DEBUG
			if( ((DataNode *)node)->hash==0){
				printf("Zero Hash!");
			}
			#endif
			// if( ((DataNode *)node)->hash==temp_bucket->hash && IsKeyExist( ((DataNode *)node)->nodeDesc ) ){//It is a key match
			// 	//if(  ) //( ((DataNode *)node), temp_bucket->key, temp_bucket->value ) )

			// 		return false;
			// }
			if( ((DataNode *)node)->hash==temp_bucket->hash )
			{
				NodeDesc* oldCurrDesc = ((DataNode *)node)->nodeDesc;

				FinishPendingTxn(oldCurrDesc, desc);

	            if(IsSameOperation(oldCurrDesc, nodeDesc))
	            {
	                return true;
	            }

	            // the key that we wanted to insert is already in there, so fail
	            if(IsKeyExist(oldCurrDesc))
	            	return false;
	            else // the key they wanted to insert, isn't logically in the table so we can insert
            	{
            		//goto noMatch_putMain;
            		NodeDesc* currDesc = ((DataNode *)node)->nodeDesc;

	                if(desc->status != ACTIVE)
	                {
	                    return false;
	                }

	                //if(currDesc == oldCurrDesc)
	                {
	                    //Update desc to logically add the key to the table since it's already physically there
	                    currDesc = __sync_val_compare_and_swap(&((DataNode *)node)->nodeDesc, oldCurrDesc, nodeDesc);

	                    // If the CAS is successful, then the value before the CAS must have been oldCurrDesc which is returned
	                    // to currDesc leading to a successful comparison in the if statement below
	                    if(currDesc == oldCurrDesc)
	                    {
	                        ASSERT_CODE
	                            (
	                             __sync_fetch_and_add(&g_count_ins, 1);
	                            );

	                        //return true; 
	                        // NOTE: the following may break wait-freedom, but not lock-freedom
	                        // if the logically removed node has a different value, then CAS in our new value
	                        if ( ((DataNode *)node)->value != temp_bucket->value )
	                        	if(__sync_bool_compare_and_swap( &((DataNode *)node)->value, ((DataNode *)node)->value, ((DataNode* )temp_bucket))->value )
	                        		return true;
	                        	else // cas failed so retry
	                        		goto insert_main;
	                        else // had the same value so all we had to do was update the descriptor and return
	                        	return true;
	                    }
	                    else
	                    	goto insert_main;
	                }
            	}

			}
			else{//Create a Spine
				//Allocate Spine will return true if it succeded, and false if it failed.
				//See Below for functionality.
			//noMatch_putMain:

				bool res=Allocate_Spine(T, head,pos,(DataNode *)node,temp_bucket, MAIN_POW);
				if(res){
					increment_size();//Increments Size
					return true;
				}
				else{
					cas_fail_count++;
					node=getNodeRaw(head,pos);//Gets the pointer value at the position of interest
					continue;
				}
			}//End Else Create Spine
		}//End Else, Data Node
	}//End While Loop

	return false;
 }//End Put Main

/**
See Put_Main for logic Discription, this section is vary similar to put_main, but instead allows multiple level traversal
If you desire 3 or more different lengths of memory array then copy put_main, 
replace MAIN_POW/MAIN_SIZE with correct values, then change put_main's calls to put subs to the copied function

**DONT FORGET: to do get/delete as well
*/
inline bool putIfAbsent_sub(void* /* volatile  */* local, DataNode *temp_bucket, int T, NodeDesc* nodeDesc){
	insert_sub:
		HASH h=(temp_bucket->hash)>>MAIN_POW;//Shifts the hash to move the siginifcant bits to the right most position
	for(int right=MAIN_POW; right<KEY_SIZE; right+=SUB_POW){
		int pos=h&(SUB_SIZE-1);//Gets the sig bits from the hash
		#ifdef DEBUG
		assert(pos >=0 && pos <SUB_SIZE);//Check to make sure pos is valid
		#endif
		h=h>>SUB_POW;//Adjust the has for the next round
		
		int cas_fail_count=0;//CAS fail count, as described in put_main

		void *node=getNodeRaw(local,pos);
		do{
			if(cas_fail_count>=MAX_CAS_FAILURE){
					#ifdef DEBUGPRINTS_MARK
					printf("Marked a node sub\n");
					#endif
				mark_data_node(local,pos);
			}
			
			//Gets the siginifcant node
			
			
			if(node==NULL){//Same logic as Put Main
				void *node2;
				if( (node2=replace_node(local, pos, NULL, temp_bucket))==node){
					increment_size();//Increments size because an element was insert
					return true;
				}
				else{//CAS Failed
				//	void *node2=getNodeRaw(local,pos);
					if(isSpine(node2)){//If it is a Spine then continue, we don't know if the key was updated. 
						local=unmark_spine(node2);
						break;
					}
					else if(isMarkedData(node2)){
						//Local must be a spine node, so examine the next depth
							#ifdef DEBUGPRINTS_MARK
							printf("Failed as a result of mark sub\n");
							#endif
						node2=forceExpandTable(T,local,pos,unmark_data(node2), right+SUB_POW);
						local=unmark_spine(node2);
						break;
					}
					else if(((DataNode *)node2)->hash==temp_bucket->hash ){//HASH COMPARE
						//See Logic above on why
						NodeDesc* oldCurrDesc = ((DataNode *)node2)->nodeDesc;

						FinishPendingTxn(oldCurrDesc, desc);

			            if(IsSameOperation(oldCurrDesc, nodeDesc))
			            {
			                return true;
			            }

			            if( IsKeyExist( oldCurrDesc) )
			            	return false;
			            else // the key they wanted to insert, isn't logically in the table so we can insert
		            	{
		            		
		            		NodeDesc* currDesc = ((DataNode *)node2)->nodeDesc;

			                if(desc->status != ACTIVE)
			                {
			                    return false;
			                }

			                //if(currDesc == oldCurrDesc)
			                {
			                    //Update desc to logically add the key to the table since it's already physically there
			                    currDesc = __sync_val_compare_and_swap(&((DataNode *)node2)->nodeDesc, oldCurrDesc, nodeDesc);

			                    // If the CAS is successful, then the value before the CAS must have been oldCurrDesc which is returned
			                    // to currDesc leading to a successful comparison in the if statement below
			                    if(currDesc == oldCurrDesc)
			                    {
			                        ASSERT_CODE
			                            (
			                             __sync_fetch_and_add(&g_count_ins, 1);
			                            );

			                        // if the logically removed node has a different value, then CAS in our new value
			                        if ( ((DataNode *)node2)->value != temp_bucket->value )
			                        	if(__sync_bool_compare_and_swap( &((DataNode *)node2)->value, ((DataNode *)node2)->value, ((DataNode* )temp_bucket))->value )
			                        		return true;
			                        	else // cas failed so retry
			                        		goto insert_sub;
			                        else // had the same value so all we had to do was update the descriptor and return
			                        	return true;
			                    }
			                    else
			                    	goto insert_sub;
			                }
		            	}

			            	//goto noMatch_putSub;
					}
					else{
					//noMatch_putSub:
						cas_fail_count++;
						node=getNodeRaw(local,pos);
						continue;
					}
				}
			}
			else if(isSpine(node)){//If it is a spine break out of the while loop and continue
				//the for loop, then examine the enxt depth
				local=unmark_spine(node);
				break;
			}
			else if(isMarkedData(node)){
					#ifdef DEBUGPRINTS_MARK
					printf("Found marked sub\n");
					#endif
				//Local must be a spine node, so examine the next depth
				node=forceExpandTable(T,local,pos,unmark_data(node), right+SUB_POW);
				local=unmark_spine(node);
				break;
			}
			else{//is Data Node
				#ifdef DEBUG
				if( ((DataNode *)node)->hash==0){
					quick_print(local);
					printf("Zero Hash!");
					
				}
				#endif
				if( ((DataNode *)node)->hash==temp_bucket->hash  ){//It is a key match
					NodeDesc* oldCurrDesc = ((DataNode *)node)->nodeDesc;
					FinishPendingTxn(oldCurrDesc, desc);

		            if(IsSameOperation(oldCurrDesc, nodeDesc))
		            {
		                return true;
		            }

		            if( IsKeyExist( oldCurrDesc ) )
		            	return false;
	                else // the key they wanted to insert, isn't logically in the table so we can insert
	            	{
	            		
	            		NodeDesc* currDesc = ((DataNode *)node)->nodeDesc;

		                if(desc->status != ACTIVE)
		                {
		                    return false;
		                }

		                //if(currDesc == oldCurrDesc)
		                {
		                    //Update desc to logically add the key to the table since it's already physically there
		                    currDesc = __sync_val_compare_and_swap(&((DataNode *)node)->nodeDesc, oldCurrDesc, nodeDesc);

		                    // If the CAS is successful, then the value before the CAS must have been oldCurrDesc which is returned
		                    // to currDesc leading to a successful comparison in the if statement below
		                    if(currDesc == oldCurrDesc)
		                    {
		                        ASSERT_CODE
		                            (
		                             __sync_fetch_and_add(&g_count_ins, 1);
		                            );

		                        // if the logically removed node has a different value, then CAS in our new value
		                        if ( ((DataNode *)node)->value != temp_bucket->value )
		                        	if(__sync_bool_compare_and_swap( &((DataNode *)node)->value, ((DataNode *)node)->value, ((DataNode* )temp_bucket))->value )
		                        		return true;
		                        	else // cas failed so retry
		                        		goto insert_sub;
		                        else // had the same value so all we had to do was update the descriptor and return
		                        	return true;
		                    }
		                    else
		                    	goto insert_sub;
		                }
	            	}
		            //else
		            	//goto noMatch_putSub2;
				}
				else{//Create a Spine
				//noMatch_putSub2:
					bool res=Allocate_Spine(T, local,pos,(DataNode *)node,temp_bucket, right+SUB_POW);
					if(res){
						increment_size();
						return true;
					}
					else{
						cas_fail_count++;
						node=getNodeRaw(local,pos);
						continue;
					}
				}//End Else Create Spine
			}//End Else, Data Node
		}while(true);//End While Loop
	}//End For Loop


	return false;
}//End Sub Put

//TODO: threadid's passed from main.cc start at 1 per maptester's call to workthread
    //inline bool TransMap::Insert(Desc* desc, uint8_t opid, KEY k, VALUE v, int T)
	inline bool Update(Desc* desc, uint8_t opid, KEY k,/*VALUE e_value,*/ VALUE v, int T, DataNode*& toReturn){//T is the executing thread's ID
		/*if(e_value==v)
			return true;*/

		NodeDesc* nodeDesc = new(m_nodeDescAllocator->Alloc()) NodeDesc(desc, opid);
		
		HASH hash=HASH_KEY(k);//reorders the bits in the key to more evenly distribute the bits
#ifdef useThreadWatch
		Thread_watch[T]=hash;//Buts the hash in the watchlist
#endif

		//Allocates a bucket, then stores the value, key and hash into it
#ifdef USE_KEY
		DataNode *temp_bucket=Allocate_Node(v,k,hash,T, nodeDesc);
#else
		DataNode *temp_bucket=Allocate_Node(v,hash,T, nodeDesc);
#endif
#ifdef DEBUG
		assert(temp_bucket!=NULL);
#endif

		bool res=putUpdate_main(hash,/*e_value,*/ temp_bucket,T, nodeDesc, toReturn);
		if(!res){
			Free_Node_Stack(temp_bucket, T);
		}

#ifdef useThreadWatch
		Thread_watch[T]=0;//Removes the hash from the watchlist
#endif


		return res;
	}

inline bool putUpdate_main(HASH hash, /*VALUE e_value,*/ DataNode *temp_bucket, int T, NodeDesc* nodeDesc, DataNode*& toReturn){

	//This count bounds the number of times the thread will loop as a result of CAS failure.
	int cas_fail_count=0;
update_main:
	//Determines the position to insert at by examining the "M" right most bits
	int pos=getMAINPOS(hash);//&(MAIN_SIZE-1));
#ifdef DEBUG
	assert(pos >=0 && pos <MAIN_SIZE);//Check to make sure position is valid
#endif

	void *node=getNodeRaw(head,pos);//Gets the pointer value at the position of interest
	
	//Examining Main Spine First
	while(true){

		if(cas_fail_count>=MAX_CAS_FAILURE){//Checks to see if it failed the CAS to many times
			mark_data_node(head,pos);//If it has then it marks that node
#ifdef DEBUGPRINTS_MARK
			printf("Marked a Node--MAin\n");
#endif
		}

		if(node==NULL){//See Logic Above
			return false;
		}
		else if(isSpine(node)){//Check the Sub Spines
			return putUpdate_sub(unmark_spine(node),/*e_value,*/ temp_bucket, T, nodeDesc, toReturn);

		}
		else if(isMarkedData(node)){//Force Expand The table because someone could not pass the cas
#ifdef DEBUGPRINTS
		    printf("marked found on main!\n");
#endif
		    node=forceExpandTable(T,head,pos,unmark_data(node), MAIN_POW);
			return putUpdate_sub(unmark_spine(node),/*e_value,*/ temp_bucket, T, nodeDesc, toReturn);
		}
		else{//It is a Data Node
#ifdef DEBUG
			if( ((DataNode *)node)->hash==0){
				printf("Zero Hash!");
			}
#endif
			if( ((DataNode *)node)->hash==temp_bucket->hash ){//&& IsKeyExist( ((DataNode *)node)->nodeDesc ) ){//It is a key match
				NodeDesc* oldCurrDesc = ((DataNode *)node)->nodeDesc;
				FinishPendingTxn(oldCurrDesc, desc);

	            if(IsSameOperation(oldCurrDesc, nodeDesc))
	            {
	                return true;
	            }

	            if( IsKeyExist( oldCurrDesc ) )
	            {
					// we have a key with matching value to update, so update nodedesc
            		NodeDesc* currDesc = ((DataNode *)node)->nodeDesc;

	                if(desc->status != ACTIVE)
	                {
	                    return false;
	                }

	                //if(currDesc == oldCurrDesc)
	                {
	                    //Update desc to logically add the key to the table since it's already physically there
	                    currDesc = __sync_val_compare_and_swap(&((DataNode *)node)->nodeDesc, oldCurrDesc, nodeDesc);

	                    // If the CAS is successful, then the value before the CAS must have been oldCurrDesc which is returned
	                    // to currDesc leading to a successful comparison in the if statement below
	                    if(currDesc == oldCurrDesc)
	                    {
	                        ASSERT_CODE
	                            (
	                             __sync_fetch_and_add(&g_count_ins, 1);
	                            );

	                        toReturn = (DataNode *)node;//node;
	                        return true; 
	                    }
	                    else // weren't able to update the descriptor so retry
	                    {
	                    	goto update_main; // restart, preserving fail count and therefore wait-freedom
	                    	// there must be a concurrent transaction for our descriptor update to have failed
	                    	// update nodeinfo algorithm retries if we fail to update the descriptor
	                    }
	                }
				}
				else // the key they wanted to insert, isn't logically in the table so we can insert
            	{
            		return false; // key isn't in the table, so can't update (follows semantics of journal paper)
            	}
				//else
				//	goto noMatch_updateMain;
			}
			else{//Create a Spine
				//Allocate Spine will return true if it succeded, and false if it failed.
				//See Below for functionality.
			//noMatch_updateMain:
				bool res=Allocate_Spine(T, head,pos,(DataNode *)node,temp_bucket, MAIN_POW);
				if(res){
					increment_size();//Increments Size
					return true;
				}
				else{
					cas_fail_count++;
					node=getNodeRaw(head,pos);
					continue;
				}
			}//End Else Create Spine
		}//End Else, Data Node
	}//End While Loop
}//End Update Main

/**
 See Put_Main for logic Discription, this section is vary similar to put_main, but instead allows multiple level traversal
 If you desire 3 or more different lengths of memory array then copy put_main,
 replace MAIN_POW/MAIN_SIZE with correct values, then change put_main's calls to put subs to the copied function

 **DONT FORGET: to do get/delete as well
 */
inline bool putUpdate_sub(void* /* volatile  */* local, /*VALUE e_value,*/ DataNode *temp_bucket, int T, NodeDesc* nodeDesc, DataNode*& toReturn){
update_sub:
		HASH h=(temp_bucket->hash)>>MAIN_POW;//Shifts the hash to move the siginifcant bits to the right most position
	for(int right=MAIN_POW; right<KEY_SIZE; right+=SUB_POW){
		int pos=h&(SUB_SIZE-1);//Gets the sig bits from the hash
#ifdef DEBUG
		assert(pos >=0 && pos <SUB_SIZE);//Check to make sure pos is valid
#endif
		h=h>>SUB_POW;//Adjust the has for the next round
		void *node=getNodeRaw(local,pos);
		
		int cas_fail_count=0;//CAS fail count, as described in put_main
		do{
			if(cas_fail_count>=MAX_CAS_FAILURE){
#ifdef DEBUGPRINTS_MARK
				printf("Marked a node sub\n");
#endif
				mark_data_node(local,pos);
			}

			//Gets the siginifcant node
			

			if(node==NULL){//Same logic as Put Main
				return false;
			}
			else if(isSpine(node)){//If it is a spine break out of the while loop and continue
				//the for loop, then examine the enxt depth
				local=unmark_spine(node);
				break;
			}
			else if(isMarkedData(node)){
#ifdef DEBUGPRINTS_MARK
				printf("Found marked sub\n");
#endif
				//Local must be a spine node, so examine the next depth
				node=forceExpandTable(T,local,pos,unmark_data(node), right+SUB_POW);
				local=unmark_spine(node);
				break;
			}
			else{//is Data Node
#ifdef DEBUG
				if( ((DataNode *)node)->hash==0){
					quick_print(local);
					printf("Zero Hash!");

				}
#endif
				if( ((DataNode *)node)->hash==temp_bucket->hash  ){//It is a key match
					NodeDesc* oldCurrDesc = ((DataNode *)node)->nodeDesc;
					FinishPendingTxn(oldCurrDesc, desc);

		            if(IsSameOperation(oldCurrDesc, nodeDesc))
		            {
		                return true;
		            }

		            if( IsKeyExist( oldCurrDesc ) )
		            {
						/*if( ((DataNode *)node)->value != e_value){
							return false;
						}*/

						NodeDesc* currDesc = ((DataNode *)node)->nodeDesc;

		                if(desc->status != ACTIVE)
		                {
		                    return false;
		                }

		                //if(currDesc == oldCurrDesc)
		                {
		                    //Update desc to logically add the key to the table since it's already physically there
		                    currDesc = __sync_val_compare_and_swap(&((DataNode *)node)->nodeDesc, oldCurrDesc, nodeDesc);

		                    // If the CAS is successful, then the value before the CAS must have been oldCurrDesc which is returned
		                    // to currDesc leading to a successful comparison in the if statement below
		                    if(currDesc == oldCurrDesc)
		                    {
		                        ASSERT_CODE
		                            (
		                             __sync_fetch_and_add(&g_count_ins, 1);
		                            );

		                        toReturn = (DataNode *)node;//node;
		                        return true; 
		                    }
		                    else
		                    	goto update_sub;
		                }
					}
					else
					{
						return false; //key not there, can't update
					}
				}
				else{//Create a Spine
				//noMatch_updateSub:
					bool res=Allocate_Spine(T, local,pos,(DataNode *)node,temp_bucket, right+SUB_POW);
					if(res){
						increment_size();
						return true;
					}
					else{
						cas_fail_count++;
						node=getNodeRaw(local,pos);
						continue;
					}
				}//End Else Create Spine
			}//End Else, Data Node
		}while(true);//End While Loop
	}//End For Loop
}//End update sub

inline bool TransMap::IsSameOperation(NodeDesc* nodeDesc1, NodeDesc* nodeDesc2)
{
    return nodeDesc1->desc == nodeDesc2->desc && nodeDesc1->opid == nodeDesc2->opid;
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

// checks if the node exists
inline bool TransMap::IsKeyExist(NodeDesc* nodeDesc)
{
    bool isNodeActive = IsNodeActive(nodeDesc);
    uint8_t opType = nodeDesc->desc->ops[nodeDesc->opid].type;

    // NOTE: if the current descriptor is committed, or aborted (not live/active) and referencing an update, then the key exists
    // aborted update operations should have this method return that the key exists, then the old value
    // from the descriptor should be used
    return  (opType == FIND) || (isNodeActive && opType == INSERT) || (!isNodeActive && opType == DELETE) || (opType == UPDATE);
    // if the operation performed was an update then the key remains in the hash map regardless of return value
    	//((nodeDesc->desc->status == COMMITTED || nodeDesc->desc->status == ABORTED) && opType == UPDATE);
}

inline bool TransMap::IsLiveUpdate(NodeDesc* nodeDesc)
{
	if (nodeDesc->desc->ops[nodeDesc->opid].type == UPDATE || 
	(nodeDesc->desc->ops[nodeDesc->opid].type == FIND && nodeDesc->desc->ops[nodeDesc->opid].value != 0) )//nodeDesc->value != 0) )
		return true;
}