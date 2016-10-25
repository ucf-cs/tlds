#ifndef TRANSMAP_H
#define TRANSMAP_H

#include <cstdint>
#include <vector>
#include "common/assert.h"
#include "common/allocator.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h> //note: change wfhm assert(cond) to transmap Assert(cond, message)?
#include <iostream>
#include <iomanip>

template <class KEY, class VALUE>//, typename _tMemory>
class TransMap
{
public:
	enum MapOpStatus
	{
	    ACTIVE = 0,
	    COMMITTED,
	    ABORTED
	};

	enum ReturnCode
    {
        OK = 0,
        SKIP,
        FAIL
    };

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

    struct Desc
    {
        static size_t SizeOf(uint8_t size)
        {
            return sizeof(uint8_t) + sizeof(uint8_t) + sizeof(MapOperator) * size;
        }

        // Status of the transaction: values in [0, size] means live txn, values -1 means aborted, value -2 means committed.
        volatile uint8_t status;
        uint8_t size;
        MapOperator ops[];
    };
    
    struct NodeDesc
    {
        NodeDesc(Desc* _desc, uint8_t _opid)
            : desc(_desc), opid(_opid){}

        Desc* desc;
        uint8_t opid;
    };

    typedef struct {
		union{
			HASH hash;
			void* next;
		};
		#ifdef USE_KEY
		KEY key;/*Remove after Debugging */
		#endif
		
		VALUE value;

		NodeDesc* nodeDesc;
	}DataNode;

  //   struct Node
  //   {
  //       Node(): key(0), next(NULL), nodeDesc(NULL){}
  //       Node(uint32_t _key, Node* _next, NodeDesc* _nodeDesc)
  //           : key(_key), next(_next), nodeDesc(_nodeDesc){}

  //       // uint32_t key;
  //       // Node* next;
  //       union{
		// 	HASH hash;
		// 	void* next;
		// };
		// #ifdef USE_KEY
		// KEY key;/*Remove after Debugging */
		// #endif
		
		// VALUE value;
		// ///////////
        
  //       NodeDesc* nodeDesc;
  //   };

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

#define HASH unsigned int
#define KEY_SIZE 32
#define toHash 5
#ifndef SUB_POW
	#define SUB_POW 6
#endif
#define SUB_SIZE POW(SUB_POW)
#define MAX_CAS_FAILURE 10

#ifdef useThreadWatch
	HASH * /* volatile  */ Thread_watch;
#endif
#ifdef useVectorPool

    void ***Thread_pool_vector;
	int *Thread_pool_vector_size;
#endif
	
	void **Thread_pool_stack;
	void **Thread_spines;
	
	int MAIN_SIZE;
	int MAIN_POW;
		
	int Threads; 
	
	void* /* volatile  */*head;
	/* volatile  */ unsigned int elements;
	#ifdef SPINE_COUNT
		/* volatile  */ unsigned int spine_elements;
	#endif

	TransMap::TransMap(/*Allocator<Node>* nodeAllocator,*/ Allocator<Desc>* descAllocator, Allocator<NodeDesc>* nodeDescAllocator, uint64_t initalPowerOfTwo, uint64_t numThreads)
	~TransMap();

	bool ExecuteOps(Desc* desc, int threadId);

    //Desc* AllocateDesc(uint8_t size);

    VALUE get(KEY k, int T){return get_first(k,T);};
	bool remove(KEY k,int T){return remove_first(k,T);};
	bool putUpdate(KEY k, VALUE e_value, VALUE n_value, int T){ return putUpdate_first(k,e_value,n_value,T);};
	bool putIfAbsent(KEY k, VALUE v, int T){ return putIfAbsent_first(k,v,T);};

	int size(){ return elements; }


	//Debug Interfaces//
	int capacity(){
		#ifdef SPINE_COUNT
	 		return (SUB_SIZE*spine_elements + MAIN_SIZE);
	  	#else
			return -1;
			#endif
	}
	

	//TODO UPDATE HASH FUNCTION
	/**
	 Hash function must only reorder bits! it must be 1:1
	 **/
	inline HASH HASH_KEY(KEY k) {

#if toHash==1
		HASH x=k;
		//XY
		register unsigned int y = 0x55555555;

		x = (((x >> 1) & y) | ((x & y) << 1));
		y = 0x33333333;
		x = (((x >> 2) & y) | ((x & y) << 2));
		y = 0x0f0f0f0f;
		x = (((x >> 4) & y) | ((x & y) << 4));
		y = 0x00ff00ff;
		x = (((x >> 8) & y) | ((x & y) << 8));
		return x;
#elif toHash==2

		unsigned int key=(unsigned int)k;
		key = ~key + (key << 15); // key = (key << 15) - key - 1;
		key = key ^ (key >> 12);
		key = key + (key << 2);
		key = key ^ (key >> 4);
		key = key * 2057; // key = (key + (key << 3)) + (key << 11);
		key = key ^ (key >> 16);
		return key;
#elif toHash==3
		return (((k & 0xF0000000) >> 20)+((k & 0x0F000000) >> 20)+((k & 0x00F00000) >> 20)+((k & 0x000F0000) >> 04)+
				((k & 0x0000F000) << 16)+((k & 0x00000F00) << 16)+((k & 0x000000F0) << 16)+((k & 0x0000000F) << 16));
#elif toHash==4
		return ((k & 0xFFFF0000) >> 16)+((k & 0x0000FFFF) << 16);


#else
		return (HASH) k;
#endif

	}
	
	inline int getMAINPOS(HASH hash){
		return hash&(MAIN_SIZE-1);
	};


private:
	// inline void Free_Node(void * D, int T);

	inline void *  getNode(void*/* volatile  */ *s, int pos);
	inline void *  getNodeRaw(void* /* volatile  */ *s, int pos);

	inline void	decrement_size();
	inline void	increment_size();

	inline bool	isSpine(void *s);
	inline void * /* volatile  */ * unmark_spine(void *s);
	inline void *  mark_spine(void * /* volatile  */ *s);

	inline void mark_data_node(void * /* volatile  */*s, int pos);
	inline bool isMarkedData(void *p);
	inline void* mark_data(void *s);
	inline void* unmark_data(void *s);

	int	 get_size();
	int	 size();
	int	 capacity();

	inline int POW(int x);

	// TODO: update these prototypes once functions are worked out in the cc file
    //ReturnCode Insert(uint32_t key, Desc* desc, uint8_t opid, Node*& inserted, Node*& pred);
    bool TransMap::Insert(Desc* desc, uint8_t opid, KEY k, VALUE v, int T);
    ReturnCode Delete(uint32_t key, Desc* desc, uint8_t opid, Node*& deleted, Node*& pred);

    //TODO: add markfordeletion to Find
    ReturnCode Find(uint32_t key, Desc* desc, uint8_t opid);

    ReturnCode Update(uint32_t key, Desc* desc, uint8_t opid, Node*& inserted, Node*& pred);

    void HelpOps(Desc* desc, uint8_t opid);
    bool IsSameOperation(NodeDesc* nodeDesc1, NodeDesc* nodeDesc2);
    void FinishPendingTxn(NodeDesc* nodeDesc, Desc* desc);
    //bool IsNodeExist(Node* node, uint32_t key);
    bool IsNodeActive(NodeDesc* nodeDesc);
    bool IsKeyExist(NodeDesc* nodeDesc);

    // nodes get marked for deletion in helpops and physically remove in the traversal inside of locatepred
    //void LocatePred(Node*& pred, Node*& curr, uint32_t key); %TODO: markfordeletion in the Find method
    //void MarkForDeletion(const std::vector<Node*>& nodes, const std::vector<Node*>& preds, Desc* desc);

    //void Print();

//TODO: threadid's passed from main.cc start at 1 per maptester's call to workthread
	inline bool putUpdate_first(KEY k,VALUE e_value, VALUE v, int T, DataNode*& toReturn){//T is the executing thread's ID
		if(e_value==v)
			return true;

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

		bool res=putUpdate_main(hash,e_value, temp_bucket,T, nodeDesc);
		if(!res){
			Free_Node_Stack(temp_bucket, T);
		}

#ifdef useThreadWatch
		Thread_watch[T]=0;//Removes the hash from the watchlist
#endif


		return res;
	}
	inline bool putUpdate_main(HASH hash, VALUE e_value, DataNode *temp_bucket, int T, NodeDesc* nodeDesc){

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
				return putUpdate_sub(unmark_spine(node),e_value, temp_bucket, T);

			}
			else if(isMarkedData(node)){//Force Expand The table because someone could not pass the cas
#ifdef DEBUGPRINTS
			    printf("marked found on main!\n");
#endif
			    node=forceExpandTable(T,head,pos,unmark_data(node), MAIN_POW);
				return putUpdate_sub(unmark_spine(node),e_value, temp_bucket, T);
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
						if(((DataNode *)node)->value != e_value){
							return false;
						}
						else{

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

			                        toReturn = node;
			                        return true; 
			                    }
			                    else // weren't able to update the descriptor so retry
			                    {
			                    	goto update_main; // restart, preserving fail count and therefore wait-freedom
			                    	// there must be a concurrent transaction for our descriptor update to have failed
			                    	// update nodeinfo algorithm retries if we fail to update the descriptor
			                    }
			                }

		// 	                //void* node2; // before removing replace
		// 	              	void *node2 = head[pos];
		// 	              	// replace node which is there now, with our new temp_bucket which was passed into the function
		// 					//if( __sync_bool_compare_and_swap ( &((DataNode *)node)->value, ((DataNode *)node)->value, ((DataNode *)temp_bucket)->value ) ){
		// 					if ((node2=replace_node(head, pos, node, temp_bucket)) == node){//Attempt to replace the node
		// 						//Pass the CAS
		// 						//Free_Node(node, T);//Frees the original
		// 						return true;
		// 					}
		// 					else{//Failed the CAS
		// 						if(node2 == NULL){//If it is NULL the return (see above for why)
		// 							return false;
		// 						}
		// 						else if(isSpine(node2)){//If it is a Spine then continue, we don't know if the key was updated.
		// 							return putUpdate_sub(unmark_spine(node2), e_value, temp_bucket, T);
		// 						}
		// 						//If it is the same node marked then we force expand the table
		// 						else if(isMarkedData(node2) && unmark_data(node2)==node){
		// #ifdef DEBUGPRINTS_MARK
		// 							printf("Failed due to mark--main\n");
		// #endif
		// 							node=forceExpandTable(T,head,pos,unmark_data(node2), MAIN_POW);//The expanded spine is returned
		// 							return putUpdate_sub(unmark_spine(node),e_value, temp_bucket, T);//We Examine the next level
		// 						}
		// 						else{
		// 							return false;
		// 						}
		// 					}



						}//End it Else it is value match
					}
					else // the key they wanted to insert, isn't logically in the table so we can insert
	            	{
	            		
	            		// NodeDesc* currDesc = ((DataNode *)node)->nodeDesc;

		             //    if(desc->status != ACTIVE)
		             //    {
		             //        return false;
		             //    }

		             //    //if(currDesc == oldCurrDesc)
		             //    {
		             //        //Update desc to logically add the key to the table since it's already physically there
		             //        //currDesc = __sync_val_compare_and_swap(&((DataNode *)node)->nodeDesc, oldCurrDesc, nodeDesc);

		             //        // If the CAS is successful, then the value before the CAS must have been oldCurrDesc which is returned
		             //        // to currDesc leading to a successful comparison in the if statement below
		             //        // if(currDesc == oldCurrDesc)
		             //        // {
		             //        //     ASSERT_CODE
		             //        //         (
		             //        //          __sync_fetch_and_add(&g_count_ins, 1);
		             //        //         );

		             //        //     return true; 
		             //        // }
		             //        return false; // key isn't in the table, so can't update
		             //    }
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
	inline bool putUpdate_sub(void* /* volatile  */* local, VALUE e_value, DataNode *temp_bucket, int T){
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
							if( ((DataNode *)node)->value != e_value){
								return false;
							}

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

			                        toReturn = node;
			                        return true; 
			                    }
			                    else
			                    	goto update_sub;
			                }

	// 						void *node2=head[pos];
	// 						if((node2=replace_node(local, pos, node, temp_bucket))==node){
	// 						//if( __sync_bool_compare_and_swap ( &((DataNode *)node)->value, ((DataNode *)node)->value, ((DataNode *)temp_bucket)->value ) ){
	// 							//Free_Node(node, T);//CAS Succedded, no need to update size as we are only replacing
	// 							return true;
	// 						}
	// 						else{
	// 							//Get the Current Node
	// 							if(node2 == NULL){//If it is NULL the return (see above for why)
	// 								return false;
	// 							}
	// 							else if(isSpine(node2)){//If it is a Spine then continue, we don't know if the key was updated.
	// 								local=unmark_spine(node2);
	// 								break;
	// 							}
	// 							//If it is the same node marked then we force expand the table
	// 							else if(isMarkedData(node2) && unmark_data(node2)==node){
	// #ifdef DEBUGPRINTS_MARK
	// 								printf("failed as a result of marked--sub 2\n");
	// #endif
	// 								node=forceExpandTable(T,local,pos,unmark_data(node2), right+SUB_POW);//The expanded spine is returned
	// 								local=unmark_spine(node);
	// 								break;
	// 							}
	// 							//We can linearize that the value was inserted then imeditly replace in any olther cas
	// 							else{
	// 								return false;
	// 							}
	// 						}
						}
						else
						{
							return false; //key not there, can't update
						}
						// else // the key they wanted to insert, isn't logically in the table so we can insert
		    //         	{
		            		
		    //         		NodeDesc* currDesc = ((DataNode *)node)->nodeDesc;

			   //              if(desc->status != ACTIVE)
			   //              {
			   //                  return false;
			   //              }

			   //              //if(currDesc == oldCurrDesc)
			   //              {
			   //                  //Update desc to logically add the key to the table since it's already physically there
			   //                  currDesc = __sync_val_compare_and_swap(&((DataNode *)node)->nodeDesc, oldCurrDesc, nodeDesc);

			   //                  // If the CAS is successful, then the value before the CAS must have been oldCurrDesc which is returned
			   //                  // to currDesc leading to a successful comparison in the if statement below
			   //                  if(currDesc == oldCurrDesc)
			   //                  {
			   //                      ASSERT_CODE
			   //                          (
			   //                           __sync_fetch_and_add(&g_count_ins, 1);
			   //                          );

			   //                      return true; 
			   //                  }
			   //              }
		    //         	}
						//else
						//	goto noMatch_updateSub;
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
	inline VALUE get_first(KEY k, int T){
		HASH h=HASH_KEY(k);//Reorders the bits for more even distribution
#ifdef useThreadWatch
		Thread_watch[T]=h;//Adds the hash to the watchlist
#endif
		VALUE v=get_main(h);//Calls the get main function							//TODO: MemCopy?
#ifdef useThreadWatch
		Thread_watch[T]=0;//Removes the hash from the watchlist
#endif
		return v;
	}
	
	inline VALUE get_main(HASH hash) {
	find_main:
		int pos=getMAINPOS(hash);//&(MAIN_SIZE-1));
	#ifdef DEBUG	
		assert(pos >=0 && pos <MAIN_SIZE);
	#endif
		
		void *node= getNode(head,pos);//Gets the pointer value at the sig node, stripping bit mark if it had it

		if (node == NULL)
			return (VALUE)NULL;//Returns NULL because key is not in the table
		else if(isSpine(node))
			return get_sub(hash,unmark_spine(node));//Checks the Sub_Spine
		else{//Is Data Node//Found a Data if it is a key match then it returns the value
			if ( ((DataNode *)node)->hash == hash)									//HASH COMPARE
			{
				NodeDesc* oldCurrDesc = ((DataNode *)node)->nodeDesc;
				FinishPendingTxn(oldCurrDesc, desc);

	            if(IsSameOperation(oldCurrDesc, nodeDesc))
	            {
	                return true; //TODO: need to return some value here, maybe make up a sentinel
	            }

	            // NOTE: because we use a perfect hash function with our wfhm, if the key
	            // doesn't exist here, then it doesn't logically exist anywhere in the table 
	            if( !IsKeyExist( oldCurrDesc ) )
					return (VALUE)NULL;
	            else // the key they wanted to insert, isn't logically in the table so we can insert
            	{
            		
            		NodeDesc* currDesc = ((DataNode *)node)->nodeDesc;

	                if(desc->status != ACTIVE)
	                {
	                    return (VALUE)NULL;
	                }

	                //if(currDesc == oldCurrDesc)
	                {
	                	// the descriptor will save the old fields, so use those with an aborted update
	                	//VALUE oldval = oldCurrDesc->desc->ops[oldCurrDesc->opid].value;

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

	                       	//if(IsAbortedUpdate(oldCurrDesc))
	                       	//{
	                       		//nodeDesc->value = oldval;
	                		//	return oldval;
	                       //	}
	                		//else
	                			return ((DataNode *)node)->value;
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
	
	inline VALUE get_sub(HASH hash, void* /* volatile  */* local){
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

		                if(desc->status != ACTIVE)
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

		                        return ((DataNode *)node)->value;
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

                if(desc->status != ACTIVE)
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

                        return ((DataNode *)node)->value;
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
	inline bool remove_first(KEY k, int T){
		HASH h=HASH_KEY(k);//Reorders the bits for more even distribution
#ifdef useThreadWatch
		Thread_watch[T]=h;//Adds the key to the watchlist
#endif

		bool res=remove_main(h, T);//Checks For the key and removes it if found
#ifdef useThreadWatch
		Thread_watch[T]=0;//Removes the key from the watch list
#endif
		return res;//Returns the result.
	}
	
	inline bool remove_main(HASH hash, int T) {
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
			return remove_sub(hash,unmark_spine(node), T);
		} 
		if(isSpine(node))//examine the subspine
			return remove_sub(hash,unmark_spine(node), T);
		
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

                        return true; 
                    }
                    else
                    	goto delete_main;
                }

                // Don't physically delete nodes
				// if(replace_node(head,pos,node)){//Tries to CAS the key match to NULL
				// 	Free_Node(node,T);//Frees the Node for Reuse
				// 	decrement_size();//Decreases the number of elements
				// 	return true;
				// }
				// else{//If the CAS fails
				// 	void *node2=getNodeRaw(head,pos);
				// 	if(isMarkedData(node2) && unmark_data(node2)!=node){//If it is the same node but marked is at the location
				// 		//Then expand the table, and examine the sub spine
				// 		node2=forceExpandTable(T,head,pos,node, MAIN_POW);//getNodeRaw must return a spine pointer
				// 		return remove_sub(hash,unmark_spine(node2), T);
				// 	}
				// 	if(isSpine(node))//Then expand the table, and examine the sub spine 
				// 		return remove_sub(hash,unmark_spine(node), T);
				// }
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
	
	inline bool remove_sub(HASH hash, void* /* volatile  */ * local, int T) {
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

		                        return true;
		                    }
		                    else
		                    	goto delete_sub;
		                }

		                // Don't physically delete nodes
						// if(replace_node(local,pos,node)){//Try to remove
						// 	Free_Node(node,T);//If success the free the node
						// 	decrement_size();//Decremese element count
						// 	return true;//Return true
						// }
						// else{//If it failed
						// 	void *node2=getNodeRaw(local,pos);
						// 	if(isMarkedData(node2) && unmark_data(node2)!=node){//If it is the same node but marked then force expand and examine subspine
						// 		node=forceExpandTable(T,local,pos,node, right+SUB_POW);
						// 	}
						// 	else//See Logic above remove_main
						// 		return false;
						// }
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
	
	/*This function adds a node that has been removed from the table
	to the reuse stack or vector
	*/
// 	inline void Free_Node(void *node, int T){
// #ifdef useThreadWatch
// 		if(!inUse(((DataNode *)node)->hash,T))//If it is Not in use, then place it on the stack
// 		{
// 			//Add to Stack
// 				((DataNode *)node)->next=Thread_pool_stack[T];//Sets the next pointer to the stack
// 				Thread_pool_stack[T]=node;//Then set the stack to the node
// 				#ifdef DEBUGPRINTS_RECYCLE
// 					printf("Placed on Stack(2) %p by %d\n",node,T);
// 				#endif
// 				return;
// 		}
#ifdef useVectorPool
		else{//IF it is in use then  place it in the vector
			//Add to vector
			int size=Thread_pool_vector_size[T];
			
			for(int i=0; i<size; i++){//Go through each element
				DataNode *D=(DataNode *)(Thread_pool_vector[T][i]);
				if(D==NULL){//If null, place it in the vector
					Thread_pool_vector[T][i]=node;
					#ifdef DEBUGPRINTS_RECYCLE
					    printf("Placed in vector %p by %d\n",node,T);
					#endif
					return;
				}
				else if(!inUse(D->hash, T)){//If it is not inuse move the node to the stack, then replace it
					if(Thread_pool_stack[T]==NULL){//Mo
						Thread_pool_stack[T]=D;
						((DataNode *)D)->next=NULL;
						#ifdef DEBUGPRINTS_RECYCLE
							printf("Placed on Stack(3) %p by %d\n",node,T);
						#endif
					}
					else{
						D->next=Thread_pool_stack[T];
						Thread_pool_stack[T]=D;
						
						#ifdef DEBUGPRINTS_RECYCLE
						printf("Move (P:(%i)) to Stack %p by %d\n",i,D,T);
						#endif
					}
					Thread_pool_vector[T][i]=node;
					#ifdef DEBUGPRINTS_RECYCLE
					printf("Placed(2) in vector (%i) %p by %d\n",i,node,T);
					#endif
					
					return;
				}
			}//End For Loop
			//Should not reach!
			//No Valid Spot Found Expand by one
			
			Thread_pool_vector[T]=(void **)realloc(Thread_pool_vector[T],sizeof(void *)*(size+1));
			Thread_pool_vector[T][size]=node;
			Thread_pool_vector_size[T]=size+1;
		}//End Else
#endif
#else
		return;
#endif
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

	// if (cas_res=replace_node(local, pos, marked_n, (void *)mark_spine(s_head) ) )!=marked_n	
	// 	void *marked_n=(void *)(mark_data((void *)n));
	// 	void * /* volatile  */ *local,int pos
	// 	void *cas_res;
	// 	if(Thread_spines[T]==NULL){
	// 		s_head=(void**)getSpine();
	// 	}
	// 	else{
	// 		s_head=(void**)Thread_spines[T];
	// 		Thread_spines[T]=s_head[0];
	// 	}
	
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

private:
    // Node* m_tail;
    // Node* m_head;

    Allocator<Node>* m_nodeAllocator;
    Allocator<Desc>* m_descAllocator;
    Allocator<NodeDesc>* m_nodeDescAllocator;

    // TODO: make sure counting is disabled during certain performance tests
    ASSERT_CODE
    (
    	// Total node count
        uint32_t g_count = 0;

        uint32_t g_count_ins = 0;
        uint32_t g_count_ins_new = 0;
        uint32_t g_count_del = 0;
        
        // never incremented
        uint32_t g_count_del_new = 0;
        
        uint32_t g_count_fnd = 0;
        uint32_t g_count_upd = 0;
    )

    uint32_t g_count_commit = 0;
    uint32_t g_count_abort = 0;
    uint32_t g_count_fake_abort = 0;
}; // end class TransMap

#endif /* end of include guard: TRANSMAP_H */    
