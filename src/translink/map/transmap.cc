#include <cstdlib>
#include <cstdio>
#include <new>
#include "translink/map/transmap.h"


#define SET_MARK(_p)    ((Node *)(((uintptr_t)(_p)) | 1))
#define CLR_MARK(_p)    ((Node *)(((uintptr_t)(_p)) & ~1))
#define CLR_MARKD(_p)    ((NodeDesc *)(((uintptr_t)(_p)) & ~1))
#define IS_MARKED(_p)     (((uintptr_t)(_p)) & 1)

__thread TransMap::HelpStack helpStack;

TransMap::TransMap(/*Allocator<Node>* nodeAllocator,*/ Allocator<TransMap::Desc>* descAllocator, Allocator<TransMap::NodeDesc>* nodeDescAllocator, uint64_t initalPowerOfTwo, uint64_t numThreads)
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

TransMap::~TransMap()
{
	printf("Total commit %u, abort (total/fake) %u/%u\n", g_count_commit, g_count_abort, g_count_fake_abort);
    //Print();

//NOTE: counts are not incremented with correct semantics in any method
    // ASSERT_CODE
    // (
    //     printf("Total node count %u, Inserts (total/new) %u/%u, Deletes (total/new) %u/%u, Finds %u\n", g_count, g_count_ins, g_count_ins_new, g_count_del , g_count_del_new, g_count_fnd);
    // );

    //Node* curr = m_head;
    //while(curr != NULL)
    //{
        //free(curr);
        //curr = curr->next;
    //}
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

inline VALUE TransMap::GetValue(NodeDesc* oldCurrDesc)
{
	return oldCurrDesc->desc->ops[oldCurrDesc->opid].value;
}

inline std::vector<VALUE> TransMap::HelpOps(Desc* desc, uint8_t opid, int threadId)
{
    if(desc->status != MAP_ACTIVE)
    {
        return;
    }

    //Cyclic dependcy check
    if(helpStack.Contain(desc))
    {
        if(__sync_bool_compare_and_swap(&desc->status, MAP_ACTIVE, MAP_ABORTED))
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

    while(desc->status == MAP_ACTIVE && ret != FAIL && opid < desc->size)
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
        	if(x->nodeDesc->desc == desc)//&& x->nodeDesc->desc->ops[x->nodeDesc->opid].type == MAP_UPDATE)
        	{
        		if (x->nodeDesc->desc->ops[x->nodeDesc->opid].type == MAP_UPDATE || 
        			(x->nodeDesc->desc->ops[x->nodeDesc->opid].type == MAP_FIND && x->nodeDesc->desc->ops[nodeDesc->opid].value != 0) )//nodeDesc->value != 0) )
        		{
        			x->value = x->nodeDesc->desc->ops[nodeDesc->opid].value;//x->nodeDesc->value;
        		}
        	}
        }

        if(__sync_bool_compare_and_swap(&desc->status, MAP_ACTIVE, MAP_COMMITTED))
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
        if(__sync_bool_compare_and_swap(&desc->status, MAP_ACTIVE, MAP_ABORTED))
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

	                if(desc->status != MAP_ACTIVE)
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

			                if(desc->status != MAP_ACTIVE)
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

		                if(desc->status != MAP_ACTIVE)
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

	                if(desc->status != MAP_ACTIVE)
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

		                if(desc->status != MAP_ACTIVE)
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

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////Get Functions///////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
/**
The get functions retun VALUE if the key is in the table and NULL if it is not.
They don't modify the table and if a data node is marked they ignore the marking

*/

//NOTE: for the map interface, can't return true or false for the first time, have to return a non-null value or null
	// this has to be caught and interpreted by the helpops function when assigning transaction status back to the ret value
	// for the while loop and needs to be done for map interface in general in the update template pseudocode
	//inline VALUE get_first(KEY k, int T){
	inline VALUE Find(Desc* desc, uint8_t opid, KEY k, int T){
		NodeDesc* nodeDesc = new(m_nodeDescAllocator->Alloc()) NodeDesc(desc, opid);
		HASH h=HASH_KEY(k);//Reorders the bits for more even distribution
#ifdef useThreadWatch
		Thread_watch[T]=h;//Adds the hash to the watchlist
#endif
		VALUE v=get_main(h, nodeDesc);//Calls the get main function							//TODO: MemCopy?
#ifdef useThreadWatch
		Thread_watch[T]=0;//Removes the hash from the watchlist
#endif
		return v;
	}
	
	inline VALUE get_main(HASH hash, NodeDesc* nodeDesc) {
	find_main:
		int pos=getMAINPOS(hash);//&(MAIN_SIZE-1));
	#ifdef DEBUG	
		assert(pos >=0 && pos <MAIN_SIZE);
	#endif
		
		void *node= getNode(head,pos);//Gets the pointer value at the sig node, stripping bit mark if it had it

		if (node == NULL)
			return (VALUE)NULL;//Returns NULL because key is not in the table
		else if(isSpine(node))
			return get_sub(hash,unmark_spine(node), nodeDesc);//Checks the Sub_Spine
		else{//Is Data Node//Found a Data if it is a key match then it returns the value
			if ( ((DataNode *)node)->hash == hash)									//HASH COMPARE
			{
				NodeDesc* oldCurrDesc = ((DataNode *)node)->nodeDesc;
				FinishPendingTxn(oldCurrDesc, desc);

	            if(IsSameOperation(oldCurrDesc, nodeDesc))
	            {
	                return 0; //TODO: need to return some value here, maybe make up a sentinel
	            }

	            // NOTE: because we use a perfect hash function with our wfhm, if the key
	            // doesn't exist here, then it doesn't logically exist anywhere in the table 
	            if( !IsKeyExist( oldCurrDesc ) )
					return (VALUE)NULL;
	            else // the key they wanted to insert, isn't logically in the table so we can insert
            	{
            		
            		NodeDesc* currDesc = ((DataNode *)node)->nodeDesc;

	                if(desc->status != MAP_ACTIVE)
	                {
	                    return (VALUE)NULL;
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

	                        // if this txn did an update here, then use that value
	                        if (IsLiveUpdate(oldCurrDesc))
	                        {
	                        	nodeDesc->desc->ops[nodeDesc->opid].value = GetValue(oldCurrDesc);//oldCurrDesc->desc->ops[oldCurrDesc->opid].value;	
	                        	return nodeDesc->desc->ops[nodeDesc->opid].value;//nodeDesc->value;
	                        }
	                		else
	                		{
	                			// if it's not a live update then use the value at the node
	                			return ((DataNode *)node)->value;
	                		}
	                    }
	                    else
	                    	goto find_main;
	                }

            	}
				//else
					//goto noMatch_getMain;
			}
			else//otherwise return NULL because there is no key match
			{
			//noMatch_getMain:
				return (VALUE)NULL;
			}
		}//End Is Data Node
	}//End Get Main
	
	inline VALUE get_sub(HASH hash, void* /* volatile  */* local, NodeDesc* nodeDesc){
	find_sub:
		HASH h=hash>>MAIN_POW; //Adjusts the hash bits
		int pos=h&(SUB_SIZE-1);//determines the position to check
		#ifdef DEBUG
		assert(pos >=0 && pos <SUB_SIZE);
		#endif
		
		//This loop will run until the max depth is reached, the spine at the max depth will be examined by 
		//the code block below
		for(int right=MAIN_POW; right<KEY_SIZE-SUB_POW; right+=SUB_POW){
			h=h>>SUB_POW;//adjusts the hash bits for the next time
			void *node=getNode(local,pos);
			if (node == NULL)//See Logic above
				return (VALUE)NULL;
			else if(!isSpine(node)){
				if (((DataNode *)node)->hash == hash)//HASH COMPARE
				{
					NodeDesc* oldCurrDesc = ((DataNode *)node)->nodeDesc;
					FinishPendingTxn(oldCurrDesc, desc);

		            if(IsSameOperation(oldCurrDesc, nodeDesc))
		            {
		                return true; //TODO: use sentinel
		            }

		            if( !IsKeyExist( oldCurrDesc ) )
						return (VALUE)NULL;
		            else // the key they wanted to insert, isn't logically in the table so we can insert
	            	{
	            		
	            		NodeDesc* currDesc = ((DataNode *)node)->nodeDesc;

		                if(desc->status != MAP_ACTIVE)
		                {
		                    return (VALUE)NULL;
		                }

		                //if(currDesc == oldCurrDesc)
		                {
		                	//if(IsAbortedUpdate(oldCurrDesc))
		                	//	return oldCurrDesc->desc->ops[oldCurrDesc->opid].value;

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
		                        
		                        // if this txn did an update here, then use that value
		                        if (IsLiveUpdate(oldCurrDesc))
		                        {
		                        	nodeDesc->desc->ops[nodeDesc->opid].value = GetValue(oldCurrDesc);//nodeDesc->value = oldCurrDesc->desc->ops[oldCurrDesc->opid].value;	
		                        	return nodeDesc->desc->ops[nodeDesc->opid].value;//nodeDesc->value;
		                        }
		                		else
		                		{
		                			// if it's not a live update then use the value at the node
		                			return ((DataNode *)node)->value;
		                		}
		                    }
		                    else
		                    	goto find_sub;
		                }
	            	}
					//else
						//goto noMatch_getSub;
				}
				else
				{
				//noMatch_getSub:
					return (VALUE)NULL;
				}
			}//End Is Data Node
			local=unmark_spine(node);
			pos=h&(SUB_SIZE-1);
		}//End For loop
		
	find_final:
		//Since this is the final depth we dont need to check the hash, as only hash matches can be placed here
		void *node=getNode(local,pos);
		#ifdef DEBUG
		assert(!isSpine(node));
		#endif
		if (node == NULL)
			return (VALUE)NULL;
		else{/* if (((DataNode *)node)->hash == hash)*/ 							//HASH COMPARE
			#ifdef DEBUG
			assert(((DataNode *)node)->hash == hash);
			#endif

			NodeDesc* oldCurrDesc = ((DataNode *)node)->nodeDesc;
			FinishPendingTxn(oldCurrDesc, desc);

            if(IsSameOperation(oldCurrDesc, nodeDesc))
            {
                return true; //TODO: use sentinel
            }

            if( !IsKeyExist( oldCurrDesc ) )
				return (VALUE)NULL;
            else // the key they wanted to insert, isn't logically in the table so we can insert
        	{
        		
        		NodeDesc* currDesc = ((DataNode *)node)->nodeDesc;

                if(desc->status != MAP_ACTIVE)
                {
                    return (VALUE)NULL;
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

	                        // if this txn did an update here, then use that value
	                        if (IsLiveUpdate(oldCurrDesc))
	                        {
	                        	nodeDesc->desc->ops[nodeDesc->opid].value = GetValue(oldCurrDesc);//nodeDesc->value = //oldCurrDesc->desc->ops[oldCurrDesc->opid].value;	
	                        	return nodeDesc->desc->ops[nodeDesc->opid].value;//nodeDesc->value;
	                        }
	                		else
	                		{
	                			// if it's not a live update then use the value at the node
	                			return ((DataNode *)node)->value;
	                		}
                    }
                    else
                    	goto find_final; // keep retrying until we're aborted by a concurrent txn, or we succeed
                }
        	}

			//return ((DataNode *)node)->value;
		}
		/*else
			return NULL;*/
	}//end get sub


//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////Remove Functions///////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

/**
The Remove functions will remove an element from the hash table if it has a key/hash match
If it removes the element from the table then it will decrement the size of the table.

It returns true if it removed the element, and false if it didn't

If it failes to remove an element, and the current node is now...
	Data Node that is not a key match OR NULL then another thread removed the element from the table, and we can return false, because another thread already removed it
	Spine: then we must examine that spine
	Marked Data Node that is equal to the node we originally tried to CAS
		Then we force create the spine, then try to remove again
	Data Node that is a key match, then we return true and we linearize it that the key was deleted then immeditely inserted
		
**/
	//inline bool remove_first(KEY k, int T){
	//NOTE: nodeDesc is pass by value
	inline bool Delete(Desc* desc, uint8_t opid, KEY k, int T){
		NodeDesc* nodeDesc = new(m_nodeDescAllocator->Alloc()) NodeDesc(desc, opid);
		HASH h=HASH_KEY(k);//Reorders the bits for more even distribution
#ifdef useThreadWatch
		Thread_watch[T]=h;//Adds the key to the watchlist
#endif

		bool res=remove_main(h, T, nodeDesc);//Checks For the key and removes it if found
#ifdef useThreadWatch
		Thread_watch[T]=0;//Removes the key from the watch list
#endif
		return res;//Returns the result.
	}
	
	inline bool remove_main(HASH hash, int T, NodeDesc* nodeDesc) {
	delete_main:
		int pos=getMAINPOS(hash);//&(MAIN_SIZE-1));
		#ifdef DEBUG
		assert(pos >=0 && pos <MAIN_SIZE);//Verify the position is valid
		#endif
		
		void *node= getNodeRaw(head,pos);//Gets a Reference to the sig node
		if (node == NULL)
			return false;//Key is not in the table so return false
		if(isMarkedData(node)){//If it is marked the force expand, and examine the sub spine
			node=forceExpandTable(T,head,pos,unmark_data(node), MAIN_POW);
			return remove_sub(hash,unmark_spine(node), T, nodeDesc);
		} 
		if(isSpine(node))//examine the subspine
			return remove_sub(hash,unmark_spine(node), T, nodeDesc);
		
		//Check if it is a Key Match
		if ( ((DataNode *)node)->hash == hash) {//HASH COMPARE
			NodeDesc* oldCurrDesc = ((DataNode *)node)->nodeDesc;
			FinishPendingTxn(oldCurrDesc, desc);

            if(IsSameOperation(oldCurrDesc, nodeDesc))
            {
                return true;
            }

            if( IsKeyExist( oldCurrDesc ) )
            {
        		NodeDesc* currDesc = ((DataNode *)node)->nodeDesc;

                if(desc->status != MAP_ACTIVE)
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

                        return true; 
                    }
                    else
                    	goto delete_main;
                }
			}
            else // the key they wanted to remove, isn't logically in the table so we can't remove
        	{
        		return false;
        	}
			//else
				//goto noMatch_removeMain;
		}//End is Hash match
		
	//noMatch_removeMain:
		return false;
	}//End Remove Main
	
	inline bool remove_sub(HASH hash, void* /* volatile  */ * local, int T, NodeDesc* nodeDesc) {
	delete_sub:
		HASH h=hash>>MAIN_POW;//Adjusts the hash
		int pos=h&(SUB_SIZE-1);//Gets the position of the sig node
		#ifdef DEBUG
		assert(pos >=0 && pos <SUB_SIZE);//verifies position is valid
		#endif
		
		
		for(int right=MAIN_POW; right<KEY_SIZE; right+=SUB_POW){//Traverses the sub spines
			h= h>>SUB_POW;//Adjusts the hash
			void *node = getNodeRaw(local,pos);//gets the node
			if (node == NULL)//Key is not in the table so return false
				return false;
			if(isMarkedData(node)){//If it is marked then force expand, then check the subspine
				node=forceExpandTable(T,local,pos,unmark_data(node), right+SUB_POW);//Examine the subspine
			}
			else if(!isSpine(node)){//It is a Data Node
				//If it is a key/hash match
				if (( (DataNode *)node)->hash == hash) {//HASH COMPARE
					NodeDesc* oldCurrDesc = ((DataNode *)node)->nodeDesc;
					FinishPendingTxn(oldCurrDesc, desc);

		            if(IsSameOperation(oldCurrDesc, nodeDesc))
		            {
		                return true;
		            }

		            if( IsKeyExist( oldCurrDesc ) )
		            {
	            		NodeDesc* currDesc = ((DataNode *)node)->nodeDesc;

		                if(desc->status != MAP_ACTIVE)
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

		                        return true;
		                    }
		                    else
		                    	goto delete_sub;
		                }

					}
		            else // the key they wanted to insert, isn't logically in the table so we can insert
	            	{
	            		return false;
	            	}
					//else
						//goto noMatch_removeSub;
				} // end hash compare
				else
				{
				//noMatch_removeSub:
					return false;
				}
			}//End Is Data Node
			
			//Sets the current spine, and gets the new position of interst
			local  = unmark_spine(node);
			pos= h&(SUB_SIZE-1);
		}//End For Loop
		
		return false;
	}//End Remove Sub


	/**
	This function is used to expand the table at a position that is under heavy CAS contention.
	IF a thread fails to pass a CAS on a position in X tries, and the current values after those
	CAS operations don't allow the thread to return, the thread will mark the node.
	
	All threads that want to change this node must first expand the table at that position.
	Marking the node is blind, the node can be NULL, a Spine Node, or a Data Node.
	
	If we mark a spine node by mistake there is no issues because the check isSpine only considers a single bit
	that is a different bit then the is marked data. Further the unmarkspine function clears both bits.
	
	If it is null then an empty spine is created at the position.
	
	If it is data then a spine containing an unmarked node is created at the position
	*/
	void * forceExpandTable(int T,void * /* volatile  */ *local,int pos, void *n, int right){
		
		//Gets a Spine node from the Spine pool or allocates a new one
		//See Allocate_Spine for more details on the Spine Pool
		void ** s_head;
		if(Thread_spines[T]==NULL){
			s_head=(void**)getSpine();
		}
		else{
			s_head=(void**)Thread_spines[T];
			Thread_spines[T]=s_head[0];
		}
		
		//Determines the location that the current node belongs in the spine
		int i_pos=0;
		if(n!=NULL){
			DataNode *D=(DataNode *)n;
			i_pos=(	(	(D->hash)>>right	)	&	(SUB_SIZE-1)	);
		}
		
		//Places the node in the spine
		s_head[i_pos]=n;
		//Attempts to CAS the current node for the spine

		void *marked_n=(void *)(mark_data((void *)n));
		void *cas_res;

		if(	(cas_res=replace_node(local, pos, marked_n, (void *)mark_spine(s_head) ) )!=marked_n	){
			//If it fails, remove the node from the spine, place the spine  in the pool, and return the current node
			//The Current Node must be a spine
			s_head[i_pos]=NULL;
			s_head[0]=Thread_spines[T];
			Thread_spines[T]=s_head;
			#ifdef DEBUG
			assert(isSpine(getNode(local,pos)));
			#endif
			return cas_res;
		}
		else{//If it passes, return the spine that was inserted
			assert(isSpine(getNode(local,pos)));
			#ifdef SPINE_COUNT
	 			__sync_fetch_and_add(&spine_elements, 1);
	  		#endif
			return mark_spine(s_head);;
		}
		
	}

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////Memory Management Functions/////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////	
	
	/**
	Allocating a Node for reuse:
	
	If a node is removed from the hashtable, then it will be placed in that threads reuse stack or reuse vector
	Each thread has its own stack and a vector of length N-1. A node is placed in the stack if no other thread
	is operating on that node's hash, and placed in the vector if a thread is operating on the node's hash/key.
	
	The length of the vector for each thread is N, since a thread can only be examining one key at a time.
	This means that if the vector is full, and a thread wants to place an element into the array, 
	then there must be an element in the array that is no longer in use, and that element can be moved to the stack
	
	Nodes in the stack, which is a linked list using the the key as a next pointer can be used without examining 
	what the other threads are doing, while node's in the vector can only be used if no other thread is using the same hash.
	
	If there are no nodes in the stack or vector, then the thread will allocate a new node
	
	**/
	#ifdef USE_KEY
		inline DataNode * Allocate_Node(VALUE v, KEY k, HASH h, int T, NodeDesc* nodeDesc){
	#else
		inline DataNode * Allocate_Node(VALUE v, HASH h, int T, NodeDesc* nodeDesc){
	#endif
	
	    DataNode *new_temp_node=(DataNode *)Thread_pool_stack[T];
		
	    if(new_temp_node!=NULL){//Check to see if the stack was empty
			Thread_pool_stack[T]=new_temp_node->next;//If it wasn't set the stack equal to the next pointer
            #ifdef DEBUGPRINTS_RECYCLE
				printf("Removed From the Stack %p by %d\n",new_temp_node,T);
			#endif 
			//The Next pointer can be NULL or a data node memory address
	    }
	    else{//Check the Vector
#ifdef useVectorPool
			int size=Thread_pool_vector_size[T];
			for(int i=0; i<size; i++){//Goes through each position in the reuse vector
				DataNode *D=(DataNode *)(Thread_pool_vector[T][i]);//Gets the reference
				if( (D!=NULL) && (!inUse(D->hash,T)) ){//If it is not null and not inuse, then
					Thread_pool_vector[T][i]=NULL;//Set the Position to NULL

					#ifdef DEBUGPRINTS_RECYCLE
					printf("Removed From the vector %p by %d\n",D,T);
					#endif 
				
					D->value = v;//Assign the value
					D->hash = h;//Assign the hash
					#ifdef USE_KEY
					D->key = k;//Assign the key
					#endif
					return D;//Return
				}//End if
			}//End For Loop on Vector
#endif
			//No valid nodes, then malloc
			new_temp_node= (DataNode *)calloc(1,sizeof(DataNode));
			#ifdef DEBUG
			assert(new_temp_node!=NULL);//Insures a node was allocated
			#endif
	    }//End Else
	    
	    new_temp_node->value =v;//Assign the value
	    new_temp_node->hash = h;//Assign the hash
	#ifdef USE_KEY
	    new_temp_node->key = k;//Assign the key
	#endif
	
		new_temp_node->nodeDesc = nodeDesc;

	    return new_temp_node;//Return
	}
	

	
	/*
	Adds a node to the stack, used for when a thread fails to insert its node and chooses not to try again.
	See logic above put main for scenarios when a threads operation is immeditly replaced
	*/
	inline void Free_Node_Stack(void *node, int T){
		
		//Add to Stack
		/*if(Thread_pool_stack[T]==NULL){//If null set the next pointer to NULL
			Thread_pool_stack[T]=node;
			((DataNode *)node)->next=NULL;
			return;
		}
		else{*/
			((DataNode *)node)->next=Thread_pool_stack[T];//Sets the next pointer to the stack
			Thread_pool_stack[T]=node;//Then set the stack to the node
			#ifdef DEBUGPRINTS_RECYCLE
			printf("Placed on Stack(4) %p by %d\n",node,T);
			#endif
			return;
	//	}
	}
	
	
	
	/**
	This function  allocates the spine nodes used for expansion
	
	IF two nodes collide more than once, then the collision is fully resolved.
	If the thread fails to CAS the spine for the current node, then the allocated spines are placed in a stack
	to be reused later on.
	
	*/
	inline bool Allocate_Spine(int T, void* /* volatile  */* s, int pos, DataNode *n1/*current node*/, DataNode *n2/*colliding node*/, int right){
		//Gets the hash value of the node to replace
		
		//Change Watch Value
#ifdef useThreadWatch
		Thread_watch[T]=n1->hash;
#endif
		
		/**
		
		This prevents the case where the value is freed, the reallocated and placed in the same position with a different key that was read.
		The current key is read and placed in the watch list. Then It is checked to make sure the node is still at that position. If it is then it checks to make sure the key hasn't changed.
		If the key has changed, which means between the time it was load then stored into the watch list it was freed then reallocated, then it retries to get the key. Otherwise it breaks.
		**/
		int count=0;
#ifdef useThreadWatch
		for(count=0; count<MAX_CAS_FAILURE; count++){
			if( s[pos]!=n1){
				Thread_watch[T]=n2->hash;
				return false;
			}
			else if(n1->hash == Thread_watch[T] ){
				break;
			}
			else{
				Thread_watch[T]=n1->hash;
				continue;
			}
		}
#endif
		if (count==MAX_CAS_FAILURE){
				#ifdef DEBUGPRINTS_MARK
				printf("Marked a node sub--SPINE EXPANSION \n");
				#endif
			mark_data_node(s,pos);
#ifdef useThreadWatch
			Thread_watch[T]=n2->hash;
#endif
			return false;
		}
		
		
		
		//No Sense in going any further if the node is no longer there
		if(s[pos]!=n1 )
			return false;
		
		//Gets the current hash value
		HASH n1_hash	=((n1->hash)>>right);
		HASH n2_hash	=((n2->hash)>>right);
		
		#ifdef SPINE_COUNT
	 	 int spine_count=1;
	  	#endif
		void**s_head;//Gets a spine by either allocting or from the stack
		if(Thread_spines[T]==NULL){
			s_head=(void**)getSpine();
		}
		else{
			s_head=(void**)Thread_spines[T];
		}
	
		
		//Get the sig positions
		int  n1_pos	=n1_hash&(SUB_SIZE-1);
		int  n2_pos	=n2_hash&(SUB_SIZE-1);

		#ifdef DEBUG
		if(n1_hash==n2_hash){
			printf("Hash Equal Error R:%d\n",right);
			print_key(n1->hash); 
			print_key(n2->hash);
			s[pos]=NULL;
			#ifdef USE_KEY
			printf("Check get For node: %lu\n",get(n1->key,T));
			#endif
			check_table_state_p();
			s[pos]=n1;
#ifdef useThreadWatch
			Thread_watch[T]=n2->hash;
#endif
			return false;
		}
		assert(n1_hash!=n2_hash);
		#endif
	  
	 
		void* /* volatile  */ *s_temp=s_head;
	//	int count=1;
		while(n1_pos==n2_pos){//Loop until they no longer collide
	
	#ifdef DEBUG
			if(n1_hash==n2_hash){
				printf("Hash Equal Error:2\n"); 
				print_key(n1->hash); 
				print_key(n2->hash);
				check_table_state_p();
				assert(false);  
#ifdef useThreadWatch
				Thread_Watch[T]=n2->hash;
#endif
				return false;
			}
			assert(n1_hash!=n2_hash);
	#endif	
			void *s_temp2;

			if(s_temp[0]==NULL){//Allocate more spines
				s_temp2=getSpine();//Returns marked spine
			}
			else{//Get a spine from the stack
				s_temp2=s_temp[0];
				s_temp[0]=NULL;
			//	assert(isEmptyArray(s_temp,SUB_SIZE));
			}
		 #ifdef SPINE_COUNT
	  		spine_count++;
		 #endif
			//Add the spine to the privous spine
			s_temp[n1_pos]=mark_spine((void */* volatile  */*)s_temp2);
			s_temp=(void */* volatile  */*)s_temp2;

			//Adjust the hash and sig positions
			n1_hash	=n1_hash>>SUB_POW;
			n2_hash	=n2_hash>>SUB_POW;
			n1_pos=n1_hash&(SUB_SIZE-1);
			n2_pos=n2_hash&(SUB_SIZE-1);
		}
		//Mem checks
		#ifdef DEBUG
		assert(s_temp!=NULL);
		assert(s_head!=NULL);
		#endif
		
		//Sets the spine stack to the correct value
		Thread_spines[T]=(s_temp[0]);
		
		//Adds the nodes to the end spine
		s_temp[0]=NULL;
		s_temp[n1_pos]=n1;
		s_temp[n2_pos]=n2;
	  
	//attemp to add the spine to the table
		void *car_res;
		if( (car_res=replace_node(s, pos, (void *)n1,mark_spine((void */* volatile  */*)s_head) ))==n1){
		  //  __sync_fetch_and_add(&v_capacity, count*SUB_SIZE);
	//		printf("Added Spine %p replacing %p (1)\n",s_head,n1);
	  #ifdef SPINE_COUNT
	 	__sync_fetch_and_add(&spine_elements, spine_count);
	  #endif
			return true;//return true if passes
			
		}
		else{//If failed then add the allocated spines to the spine stack
			n2_hash	=(n2->hash>>right);
			
			
			void**s_temp=(void **)s_head;
			do{//Loops through, moving the spines to position 0, and removing the  node references
				n2_pos	=n2_hash&(SUB_SIZE-1);
				if(s_temp[n2_pos]==n2){//Break case
					s_temp[n1_pos]=NULL;//Remove node n1
					s_temp[n2_pos]=NULL;//remove node n2
					break;
				}
				else{//move spine to position 0
					s_temp[0]=(void *)unmark_spine(s_temp[n2_pos]);
					if(n2_pos!=0){
						s_temp[n2_pos]=NULL;
					}
					s_temp=(void **)s_temp[0];
				}
				n2_hash= n2_hash>>SUB_POW;
				
			}while(true);
			s_temp[0]=Thread_spines[T];
			
			//Adjust spine stack
			Thread_spines[T]=s_head;
#ifdef useThreadWatch
			Thread_watch[T]=n2->hash;
#endif
			return false;
		}
	}//End Spine Allocator
	
	//Allocate spine from memory
	inline void * getSpine(){
		//Could add Code to allocate more than one spine
		void* s=(void*)calloc(SUB_SIZE,(sizeof(void */* volatile  */)));
		#ifdef DEBUG
		assert(s!=NULL);
	//	assert(isEmptyArray(s,SUB_SIZE));
		#endif
		return s;
	}
   
	/*Loop through the thread watch list and if any thread is watching the hash value
		Then return true, otherwise return false.
		We can ignore our own thread
	*/
#ifdef useThreadWatch
	inline bool inUse(HASH h, int T){
		//Thread_pool_stack		Thread_pool_vector
        for(int i=0; i<Threads; i++){
            if(h==Thread_watch[i] && T!=i)//HASH Compare
                return true;
        }
        return false;
	}; //TODO: pretty sure this semicolon is just ignored
#endif
	
// //////Atomic CAS/Writes////
// //SWAPS NULL OR DATA NODE FOR DATA NODE
	inline void * replace_node(void* /* volatile  */ *s , int pos, void *current_node, DataNode *new_node){
		return replace_node(s,pos, (void *) current_node, (void *)new_node);
	}
//SWAPS A DATA NODE FOR SPINE NODE
	inline void * replace_node(void* /* volatile  */ *s, int pos, DataNode *current_node, void* new_node){
		return replace_node(s,pos, (void *) current_node, (void *)new_node);
	}
// //Replaces two pointers
	inline void * replace_node(void* /* volatile  */ *s, int pos, void *current_node, void *new_node) {
		
		if (current_node == s[pos]) {
			return __sync_val_compare_and_swap(&(s[pos]), current_node, new_node);
		}
		return  s[pos];
	}

// //DELETE NODE!
	inline bool replace_node(void* /* volatile  */ *s, int pos, void *current_node /*, new node=NULL*/) {
		if (current_node == s[pos]) {
			return __sync_bool_compare_and_swap(&(s[pos]), current_node, NULL);
		}
		return false;
	}

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////Size Calculating Functions//////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
	
inline void increment_size(){
	 __sync_fetch_and_add(&elements, 1);
}
inline void decrement_size(){
	__sync_fetch_and_add(&elements, -1);
}

int size(){ return elements; }


//Debug Interfaces//
int capacity(){
	#ifdef SPINE_COUNT
 		return (SUB_SIZE*spine_elements + MAIN_SIZE);
  	#else
		return -1;
	#endif
}

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
///////////////////////////////Bit Marking Functions//////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////	
	
//SPINE NODE BIT MARK
inline bool isSpine(void *p){
	/*if(p==NULL)
		return false;
	else*/
		return  (((unsigned long)(p)) & 1);
}
inline void * mark_spine(void * /* volatile  */*s){
	if(s==NULL)
		return NULL;
	else
		return (void *) ( ( (unsigned long)((void *)s)) |1);
}
inline void* /* volatile  */ *unmark_spine(void *s){
	return  (void* /* volatile  */ *)( ( (unsigned long)s) & ~3);//3 instead of one in case a spine was marked, less overhead then checking for a mark on a spine
}  

//DATA NODE BIT MARK
inline DataNode* unmark_data(void *s){
	return (DataNode *) ( ( (unsigned long)((void *)s)) & ~2);
}
inline void* mark_data(void *s){
	return  (void *)( ( (unsigned long)s) | 2);
}
inline bool isMarkedData(void *p){
	return  (((unsigned long)(p)) & 2);
}

inline void mark_data_node(void * /* volatile  */*s, int pos){
	#ifdef DEBUGPRINTS_MARK
	printf("Node was marked!\n");
	#endif
	__sync_fetch_and_or(&(s[pos]),2);
}

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
///////////////////////////////Atomic Wrapper Functions////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////	

///////Atomic Read/////
inline void * getNode( void* /* volatile  */ *s, int pos){
	return (void *)( ( (unsigned long)(s[pos])) & ~2);
}
inline void * getNodeRaw( void* /* volatile  */ *s, int pos){
	return (void *)(s[pos]);
}

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
    return nodeDesc->desc->status == MAP_COMMITTED;
}

// checks if the node exists
inline bool TransMap::IsKeyExist(NodeDesc* nodeDesc)
{
    bool isNodeActive = IsNodeActive(nodeDesc);
    uint8_t opType = nodeDesc->desc->ops[nodeDesc->opid].type;

    // NOTE: if the current descriptor is committed, or aborted (not live/active) and referencing an update, then the key exists
    // aborted update operations should have this method return that the key exists, then the old value
    // from the descriptor should be used
    return  (opType == MAP_FIND) || (isNodeActive && opType == MAP_INSERT) || (!isNodeActive && opType == MAP_DELETE) || (opType == MAP_UPDATE);
    // if the operation performed was an update then the key remains in the hash map regardless of return value
    	//((nodeDesc->desc->status == MAP_COMMITTED || nodeDesc->desc->status == MAP_ABORTED) && opType == MAP_UPDATE);
}

inline bool TransMap::IsLiveUpdate(NodeDesc* nodeDesc)
{
	if (nodeDesc->desc->ops[nodeDesc->opid].type == MAP_UPDATE || 
	(nodeDesc->desc->ops[nodeDesc->opid].type == MAP_FIND && nodeDesc->desc->ops[nodeDesc->opid].value != 0) )//nodeDesc->value != 0) )
		return true;
}