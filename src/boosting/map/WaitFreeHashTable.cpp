//#define NDEBUG
//#define DEBUGPRINTS 1 
//#define DEBUGPRINTS_RECYCLE 1
//#define DEBUGPRINTS_MARK 1
//#define VALIDATE 1 
//#define debug 1
//#define DEBUG 1
//#define USE_KEY 1
//#define SPINE_COUNT 1
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <iostream>
#include <iomanip>


#define HASH unsigned int
#define KEY_SIZE 32
#define toHash 5
#ifndef SUB_POW
	#define SUB_POW 6
#endif
#define SUB_SIZE POW(SUB_POW)
#define MAX_CAS_FAILURE 10

///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
////////////////////////////////Notes /////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
/* Planned Improvements:
 *	ContainsKey,
 *	PutifAbsent,
 *	PutIfValueEquals,
 *	DeleteIfValueEquals,
 *	DeleteIfAbsenet <---GET IT lol
 * 
 *	 
 */
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

inline void Free_Node(void * D, int T);

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

#ifdef VALIDATE
int	check_table_state();
void	check_table_state_p();
int	hit_count();
void quick_print(void* /* volatile  */ * head,int ms, bool t, int p);
int print_reuseable_memory();
#endif

inline int POW(int x);



template <class KEY, class VALUE>//, typename _tMemory>

class WaitFreeHashTable{
public:
	
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
////////////////////////////////Globals //////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////	
	
	typedef struct {
		union{
			HASH hash;
			void* next;
		};
		#ifdef USE_KEY
		KEY key;/*Remove after Debugging */
		#endif
		
		VALUE value;
	}DataNode;
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

// /* volatile  */ unsigned int v_capacity;
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
////////////////////////////Constructor //////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////		

	WaitFreeHashTable(int initalPowerOfTwo, int numThreads){
		MAIN_SIZE				=POW(initalPowerOfTwo);
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
	
	//Public Interfaces
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
		//hash);
		return hash&(MAIN_SIZE-1);
//		int one=MAIN_POW%2;
//		int left=MAIN_POW/2 +one;
//		int right=MAIN_POW/2;
//		
//		hash=hash&(MAIN_SIZE-1);
//		return (	(hash>>right) + (hash<<left)	)&(MAIN_SIZE-1);
//
	};//&(MAIN_SIZE-1));

	
	void nodes_at_each_depth(){
		printf("Main Pow:%d\t Sub Pow:%d\t Total Elements %d\n",MAIN_POW, SUB_POW,elements);
		void */* volatile  */*local=head;

		int* count=(int *)malloc(sizeof(int)*3);
		count[0]=0;
		count[1]=0;
		count[2]=0;
		
		for(int i=0; i<MAIN_SIZE; i++){
			void *node=getNodeRaw(local, i);
			if(isSpine(node)){
				count[1]++;
				continue;
			}
			else if(node == NULL){
				count[2]++;
			}
			else{
				count[0]++;
			}
		}
		printf("On Main Spine: Data Nodes:%d\t Spine Nodes: %d\t NullNodes:%d\n", count[0], count[1], count[2]);

		int depth=(KEY_SIZE-MAIN_POW)/SUB_POW +1;
		for(int j=1; j<=depth; j++){
			count[0]=0;
			count[1]=0;
			count[2]=0;
			
			for(int i=0; i<MAIN_SIZE; i++){
				void *node=getNodeRaw(local, i);
				if(isSpine(node)){
					int* c_res=count_nodes_at_depth(unmark_spine(node),1,j);

					int p=0;
					count[p]=count[p]+c_res[p++];
					count[p]=count[p]+c_res[p++];
					count[p]=count[p]+c_res[p++];
				}
				else{
					continue;
				}
			}
			printf("On Depth %d: Data Nodes:%d\t Spine Nodes: %d\t NullNodes:%d\n",j, count[0], count[1], count[2]);
		}

	};
	
	int* count_nodes_at_depth(void* /* volatile  */* local, int d, int t_d){
		int* count=(int *)malloc(sizeof(int)*3);
		count[0]=0;
		count[1]=0;
		count[2]=0;
		if(t_d==d){
			for(int i=0; i<SUB_SIZE; i++){
				void *node=getNodeRaw(local, i);
				if(isSpine(node)){
					count[1]++;
					continue;
				}
				else if(node == NULL){
					count[2]++;
				}
				else{
					count[0]++;
				}
			}
		}
		else{
			for(int i=0; i<SUB_SIZE; i++){
				void *node=getNodeRaw(local, i);
				if(isSpine(node)){
					
					int* c_res=count_nodes_at_depth(unmark_spine(node),d+1,t_d);
					int p=0;
					count[p]=count[p]+c_res[p++];
					count[p]=count[p]+c_res[p++];
					count[p]=count[p]+c_res[p++];
				}
				else {
					continue;
				}
			}
		}
		return count;
	};

	#ifdef VALIDATE
	
	void check_table_state(){check_table_state_p();};
	void print_table(){quick_print(head,MAIN_SIZE, true, 0);};
	int hit_count(){
		int c=0;
		for(int i=0; i<MAIN_SIZE; i++)
			if(head[i]!=NULL)
				c++;
		return (c);
	}

	int print_reuseable_memory(){
		int count=0;
		for(int i=0; i<Threads; i++){
			DataNode *t=Thread_pool_stack[i];
			while(t!=NULL){
				count++;
				t=t->next;
			}
			for(int j=0; j<Thread_pool_vector_size[i]; j++){
				if(Thread_pool_vector[i][j]!=NULL)
					count++;
				
			}
		}
		printf("Reusable data nodes: %d (total mem uses %d)\n",count,count*sizeof(DataNode));
		
		count=0;
		for(int i=0; i<Threads; i++){
			void **t=Thread_spines[i];
			while(t!=NULL){
				count++;
				t=t[0];
			}
		}
		printf("Reusable Spine nodes: %d (total mem uses %d)\n",count,SUB_SIZE*sizeof(void *));
	}
	#endif

private:
///////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////Put Functions///////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
/*
The Following are valid results of the put function:
Significant Node is the position in the hash table where the key belongs. This has a direct relationship to depth.

If the Significant node is read as NULL the thread will CAS NULL for its Node;
	If the CAS passes, the thread will return.
	If the CAS fails and the Significant is now
		*NULL, it will try again incrementing its cas_fail_count
		*A key match, it returns, it linearizes that the key's value was updated then immeditly replaced by a newwer value.
		*A non key match, it attempts to expand and insert.
			If expansion fails, it examines the significant node again, incrementing its cas_fail_count 
			If it Succedes then it returns.
		*A Spine node, then it examines the next significant node and acts accordingly.

If the Significant node is read as a Key Match the thread will CAS the Key Match for its Node;
	If the CAS passes, the thread will return.
	If the CAS fails and the Significant is now
		*NULL, returns, it linearizes that the key's value was updated then immeditly deleted by another thread
		*A key match, it returns, it linearizes that the key's value was updated then immeditly replaced by a newwer value.
		*A non key match:
			For this to occur a thread must delete the key, and a thread must insert a different key at this position.
			 It linearizes that the key's value was updated then immeditly deleted by another thread, then another thread inserted a non key match
		*A Spine node, then it examines the next significant node and acts accordingly.

If the Significant node is read as a NON Key Match the thread will CAS the NON Key Match for a Spine containing the threads node and the current node, and possibly additional spines that resolve further collision;
	If the CAS passes, the thread will return.
	If the CAS fails and the Significant is now
		*NULL, it will procede with the logic above for reading NULL value, incrementing its cas_fail_count
		*A key match, it returns, it linearizes that the non key match was deleted, then the thread inserted its value which was then immeditly replaced by a newwer value.
		*A non key match, it will try again, incrementing its cas_fail_count.
		*A Spine node, then it examines the next significant node and acts accordingly.
		
If the Significant node is read as spine node, then the thread will
	1) Set local equal to the node
	2) Right shift the hash
	3) Isolate the position of the significant  node from the bits of the shifted hash
	4)  Read the significant node and act acodingly.
	
If in the rare and unlikely case the cas_fail_count is greater then the defined threshold, 
	the thread will use atmoic sync fetch and or to mark the significant node.
If a node is marked then the thread will CAS that node for a Spine contating that node.
The reason for this is that all operations must be bounded in a Wait-Free Algorithm, regardless of CAS contention.
This bounds the number of times the thread will loop as described above.
Once a node is marked, it must be replaced by a spine node.

The number of spines that can be made is bounded by the constants KeySize, M and S. 
(Keysize=number of bits in the key)
(2^M=Length of the Main Memory Array)
(2^S=Length of the Sub Memory Arrays)
The Max Depth is equal to 1+(KeySize-M)/S rounded up

Because of this we can treat nodes at the max depth differently.
If a node is at the max depth it can't be a spine or a non key match. 
So regardless of if th CAS passes or fails the thread returns
*/


	inline bool putIfAbsent_first(KEY k, VALUE v, int T){//T is the executing thread's ID
		HASH hash=HASH_KEY(k);//reorders the bits in the key to more evenly distribute the bits
#ifdef useThreadWatch
		Thread_watch[T]=hash;//Buts the hash in the watchlist
#endif
		
		//Allocates a bucket, then stores the value, key and hash into it
		#ifdef USE_KEY
			DataNode *temp_bucket=Allocate_Node(v,k,hash,T);
		#else
			DataNode *temp_bucket=Allocate_Node(v,hash,T);
		#endif
		#ifdef DEBUG
		assert(temp_bucket!=NULL);
		#endif
		
		bool res=putIfAbsent_main(hash,temp_bucket,T);
		if(!res){
			Free_Node_Stack(temp_bucket, T);
		}
		
#ifdef useThreadWatch
		Thread_watch[T]=0;//Removes the hash from the watchlist
#endif
		return res;
	}
	inline bool putIfAbsent_main(HASH hash,DataNode *temp_bucket, int T){
		
		//This count bounds the number of times the thread will loop as a result of CAS failure.
		int cas_fail_count=0;
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
			
			void *node2;
			if(node==NULL){//See Logic Above
				if( (node2=replace_node(head, pos, NULL, temp_bucket))==NULL){
					increment_size();//Increments the size of table
					return true;
				}	
				else{
					if( /*node2!=NULL &&*/ isSpine(node2)){//If it is a Spine then continue, we don't know if the key was updated.
						return putIfAbsent_sub(unmark_spine(node2), temp_bucket, T);
					}
					else{
						cas_fail_count++;
						node=getNodeRaw(head,pos);//Gets the pointer value at the position of interest

						continue;
					}
					
				}
			}
			else if(isSpine(node)){//Check the Sub Spines
				return putIfAbsent_sub(unmark_spine(node), temp_bucket, T);
				
			}
			else if(isMarkedData(node)){//Force Expand The table because someone could not pass the cas
			    #ifdef DEBUGPRINTS
			    printf("marked found on main!\n");
			    #endif
			    node=forceExpandTable(T,head,pos,unmark_data(node), MAIN_POW);
				return putIfAbsent_sub(unmark_spine(node), temp_bucket, T);
			}
			else{//It is a Data Node
				#ifdef DEBUG
				if( ((DataNode *)node)->hash==0){
					printf("Zero Hash!");
				}
				#endif
				if( ((DataNode *)node)->hash==temp_bucket->hash){//It is a key match
					return false;
				}
				else{//Create a Spine
					//Allocate Spine will return true if it succeded, and false if it failed.
					//See Below for functionality.
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
	inline bool putIfAbsent_sub(void* /* volatile  */* local, DataNode *temp_bucket, int T){
		
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
						else if(((DataNode *)node2)->hash==temp_bucket->hash){//HASH COMPARE
							//See Logic above on why
							return false;
						}
						else{
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
					if( ((DataNode *)node)->hash==temp_bucket->hash){//It is a key match
						return false;
					}
					else{//Create a Spine
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


	inline bool putUpdate_first(KEY k,VALUE e_value, VALUE v, int T){//T is the executing thread's ID
		if(e_value==v)
			return true;
		
		HASH hash=HASH_KEY(k);//reorders the bits in the key to more evenly distribute the bits
#ifdef useThreadWatch
		Thread_watch[T]=hash;//Buts the hash in the watchlist
#endif

		//Allocates a bucket, then stores the value, key and hash into it
#ifdef USE_KEY
		DataNode *temp_bucket=Allocate_Node(v,k,hash,T);
#else
		DataNode *temp_bucket=Allocate_Node(v,hash,T);
#endif
#ifdef DEBUG
		assert(temp_bucket!=NULL);
#endif

		bool res=putUpdate_main(hash,e_value, temp_bucket,T);
		if(!res){
			Free_Node_Stack(temp_bucket, T);
		}

#ifdef useThreadWatch
		Thread_watch[T]=0;//Removes the hash from the watchlist
#endif


		return res;
	}
	inline bool putUpdate_main(HASH hash, VALUE e_value, DataNode *temp_bucket, int T){

		//This count bounds the number of times the thread will loop as a result of CAS failure.
		int cas_fail_count=0;
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
				if( ((DataNode *)node)->hash==temp_bucket->hash){//It is a key match
					if(((DataNode *)node)->value != e_value){
						return false;
					}
					else{
						void *node2;
						if( (node2=replace_node(head, pos, node, temp_bucket)) == node){//Attempt to replace the node
							//Pass the CAS
							Free_Node(node, T);//Frees the original
							return true;
						}
						else{//Failed the CAS
							if(node2 == NULL){//If it is NULL the return (see above for why)
								return false;
							}
							else if(isSpine(node2)){//If it is a Spine then continue, we don't know if the key was updated.
								return putUpdate_sub(unmark_spine(node2), e_value, temp_bucket, T);
							}
							//If it is the same node marked then we force expand the table
							else if(isMarkedData(node2) && unmark_data(node2)==node){
	#ifdef DEBUGPRINTS_MARK
								printf("Failed due to mark--main\n");
	#endif
								node=forceExpandTable(T,head,pos,unmark_data(node2), MAIN_POW);//The expanded spine is returned
								return putUpdate_sub(unmark_spine(node),e_value, temp_bucket, T);//We Examine the next level
							}
							else{
								return false;
							}
						}
					}//End it Else it is value match
				}
				else{//Create a Spine
					//Allocate Spine will return true if it succeded, and false if it failed.
					//See Below for functionality.
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
	}//End Put Main

	/**
	 See Put_Main for logic Discription, this section is vary similar to put_main, but instead allows multiple level traversal
	 If you desire 3 or more different lengths of memory array then copy put_main,
	 replace MAIN_POW/MAIN_SIZE with correct values, then change put_main's calls to put subs to the copied function

	 **DONT FORGET: to do get/delete as well
	 */
	inline bool putUpdate_sub(void* /* volatile  */* local, VALUE e_value, DataNode *temp_bucket, int T){

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
					if( ((DataNode *)node)->hash==temp_bucket->hash){//It is a key match
						if( ((DataNode *)node)->value != e_value){
							return false;
						}

						void *node2;
						if((node2=replace_node(local, pos, node, temp_bucket))==node){
							Free_Node(node, T);//CAS Succedded, no need to update size as we are only replacing
							return true;
						}
						else{
							//Get the Current Node
							if(node2 == NULL){//If it is NULL the return (see above for why)
								return false;
							}
							else if(isSpine(node2)){//If it is a Spine then continue, we don't know if the key was updated.
								local=unmark_spine(node2);
								break;
							}
							//If it is the same node marked then we force expand the table
							else if(isMarkedData(node2) && unmark_data(node2)==node){
#ifdef DEBUGPRINTS_MARK
								printf("failed as a result of marked--sub 2\n");
#endif
								node=forceExpandTable(T,local,pos,unmark_data(node2), right+SUB_POW);//The expanded spine is returned
								local=unmark_spine(node);
								break;
							}
							//We can linearize that the value was inserted then imeditly replace in any olther cas
							else{
								return false;
							}
						}
					}
					else{//Create a Spine
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
	}//End Sub Put

	

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
				return ((DataNode *)node)->value;
			else//otherwise return NULL because there is no key match
				return (VALUE)NULL;
		}//End Is Data Node
	}//End Get Main
	
	inline VALUE get_sub(HASH hash, void* /* volatile  */* local){
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
					return ((DataNode *)node)->value;
				else
					return (VALUE)NULL;
			}//End Is Data Node
			local=unmark_spine(node);
			pos=h&(SUB_SIZE-1);
		}//End For loop
		
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
			return ((DataNode *)node)->value;
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
			if(replace_node(head,pos,node)){//Tries to CAS the key match to NULL
				Free_Node(node,T);//Frees the Node for Reuse
				decrement_size();//Decreases the number of elements
				return true;
			}
			else{//If the CAS fails
				void *node2=getNodeRaw(head,pos);
				if(isMarkedData(node2) && unmark_data(node2)!=node){//If it is the same node but marked is at the location
					//Then expand the table, and examine the sub spine
					node2=forceExpandTable(T,head,pos,node, MAIN_POW);//getNodeRaw must return a spine pointer
					return remove_sub(hash,unmark_spine(node2), T);
				}
				if(isSpine(node))//Then expand the table, and examine the sub spine 
					return remove_sub(hash,unmark_spine(node), T);
			}
		}//End is Hash match
		
		return false;
	}//End Remove Main
	
	inline bool remove_sub(HASH hash, void* /* volatile  */ * local, int T) {
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
					if(replace_node(local,pos,node)){//Try to remove
						Free_Node(node,T);//If success the free the node
						decrement_size();//Decremese element count
						return true;//Return true
					}
					else{//If it failed
						void *node2=getNodeRaw(local,pos);
						if(isMarkedData(node2) && unmark_data(node2)!=node){//If it is the same node but marked then force expand and examine subspine
							node=forceExpandTable(T,local,pos,node, right+SUB_POW);
						}
						else//See Logic above remove_main
							return false;
					}
				}
				else
					return false;
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
		inline DataNode * Allocate_Node(VALUE v, KEY k, HASH h, int T){
	#else
		inline DataNode * Allocate_Node(VALUE v, HASH h, int T){
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
	

	    return new_temp_node;//Return
	}
	
	/*This function adds a node that has been removed from the table
	to the reuse stack or vector
	*/
	inline void Free_Node(void *node, int T){
#ifdef useThreadWatch
		if(!inUse(((DataNode *)node)->hash,T))//If it is Not in use, then place it on the stack
		{
			//Add to Stack
				((DataNode *)node)->next=Thread_pool_stack[T];//Sets the next pointer to the stack
				Thread_pool_stack[T]=node;//Then set the stack to the node
				#ifdef DEBUGPRINTS_RECYCLE
					printf("Placed on Stack(2) %p by %d\n",node,T);
				#endif
				return;
		}
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
	};
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
	
//////Atomic CAS/Writes////
//SWAPS NULL OR DATA NODE FOR DATA NODE
	inline void * replace_node(void* /* volatile  */ *s , int pos, void *current_node, DataNode *new_node){
		return replace_node(s,pos, (void *) current_node, (void *)new_node);
	}
//SWAPS A DATA NODE FOR SPINE NODE
	inline void * replace_node(void* /* volatile  */ *s, int pos, DataNode *current_node, void* new_node){
		return replace_node(s,pos, (void *) current_node, (void *)new_node);
	}
//Replaces two pointers
	inline void * replace_node(void* /* volatile  */ *s, int pos, void *current_node, void *new_node) {
		
		if (current_node == s[pos]) {
			return __sync_val_compare_and_swap(&(s[pos]), current_node, new_node);
		}
		return  s[pos];
	}

//DELETE NODE!
	inline bool replace_node(void* /* volatile  */ *s, int pos, void *current_node /*, new node=NULL*/) {
		if (current_node == s[pos]) {
			return __sync_bool_compare_and_swap(&(s[pos]), current_node, NULL);
		}
		return false;
	}
	
	inline int POW(int pow){
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
	
   

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////Validation Functions////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
#ifdef VALIDATE
	void print_key(HASH k) {
		int Shifts=(KEY_SIZE-MAIN_POW)/SUB_POW+1;
		unsigned int temp = k&(MAIN_SIZE-1);
		
		printf("K(%10u): %6d ",k ,temp);
		k = k>>MAIN_POW;
		while (Shifts>=0 || k!=0) {
			temp = k&(SUB_SIZE-1);
			printf(" | %6d", temp);
			k = k>>SUB_POW;
			Shifts--;
		}
		printf("\n");
	}


	int depth[64];
	void check_table_state_p() {
		printf("Starting State Checker\n");
		int max_depth=0;
		for (int i = 0; i < MAIN_SIZE; i++) {
			void *node =head[i];
			depth[0] = i;
			if (node == NULL)
				continue;
			else if (isSpine(node)) {
				int res=check_table_state(unmark_spine(node), MAIN_POW, 1);
				if(res>max_depth) max_depth=res;
				continue;
			}
			DataNode* sig_node=(DataNode *)node;
			HASH hash = sig_node->hash;
			
			int pos = (hash&(MAIN_SIZE-1));
			if (i != pos) {
				printf("Item out of place on Main:at POS %d, belongs in %d, hash: %d, ",i,pos, sig_node->hash);
				print_key(sig_node->hash);
			}
		}

		printf("Finished State Checker\n");
		printf("Max Depth: %d\n",max_depth);
	}
   

	int check_table_state(void* /* volatile  */ *s, int right, int d) {
		int max_depth=d;
		for (int i = 0; i < SUB_SIZE; i++) {
			void *node =getNode(s,i);//s[i];
			depth[d] = i;
		   
			if (node == NULL){
				continue;
			}
			
			if (isSpine(node)) {
				int res=check_table_state(unmark_spine(node), SUB_POW + right, d + 1);
				if(res>max_depth) max_depth=res;
				continue;
			}
			if(isMarkedData(node)){
				printf("Makred data????\n");
				continue;
			}
			DataNode *sig_node=(DataNode *)node;
			if (!check_key(d, sig_node->hash)) {
				printf("Hash %d, is on the wrong spine: \n", sig_node->hash);
				printf("\tDepth: %2d\n\t Current Location: %3d\n\t",d ,i);
			   	print_key(sig_node->hash);
				printf("Traversal: ");
				for(int j=0; j<=d; j++)
					printf("%3d -",depth[j]);
				printf("\n");
				printf("Node: K:%u HASH_KEY(%u) V:%lu H:%u \t",0/*sig_node->key*/,0/*HASH_KEY(sig_node->key)*/,sig_node->value,sig_node->hash);
				#ifdef USE_KEY
				printf("Get Returns: %lu\n",get(sig_node->key,0));
				#endif
				printf("\tCheck For Two pointer(removes node then  tries again!):\n");
				s[i]=NULL;
				#ifdef USE_KEY
				printf("\t\tGet AGAIN Returns: %lu\n",get(sig_node->key,0));
				#endif
				s[i]=node;
				printf("Node Id %p\n",sig_node);
				continue;
			}
			int pos = (sig_node->hash>> right) &(SUB_SIZE-1);

			if (i == pos) {
				continue;  
			} 
			else{
				printf("WRONG SPOT::: Hash %d, is in the wrong spot\n", sig_node->hash);
				printf("\tDepth: %2d\n\tPos Proper: N/A \n\tCurrent Location: %3d\n\t",d ,i);
				print_key(sig_node->hash);
				printf("Traversal: ");
				for(int j=0; j<=d; j++)
					printf("%3d -",depth[j]);
				printf("\n");
				continue;
			}
			
		  
		}
		depth[d] = 0;
		return max_depth;
	}//End Check table state2
	
	bool check_key(int d, HASH h) {
		int temp = h&(MAIN_SIZE-1);;
		h = h>>MAIN_POW;
		for (int i = 0; i <= d; i++) {
			if (depth[i] != temp)
				return false;
			else {
				temp = h&(SUB_SIZE-1);
				h = (h>>SUB_POW);
			}

		}

		return true;
	}
	void quick_print(void* /* volatile  */* s){
		quick_print(s,SUB_SIZE, false,0);
	}
	void quick_print(void* /* volatile  */* s, int l, bool recur, int tab){
		for(int i=0; i<l; i++){
			void *n=s[i];
			print_tab(tab);
			if(n==NULL){
				printf("I: %2d NULL %p \n",i,n);
				continue;
			}
			printf("I: %2d N: %p\t",i,n);
			if(isSpine(n)){
				printf(" SPINE NODE \n");
				if(recur)
						quick_print(s,SUB_SIZE, true, tab+1);
			}
			else{
				DataNode *temp=(DataNode *)n;
				print_tab(tab);
				printf("Type: Data Node Hash: %3d Value: %4d \n",temp->hash,temp->value);
				print_key(temp->hash);
			}
		}
		printf("============================\n\n");
	}
	void print_tab(int tab){
		for(int i=0; i<tab; i++){
			printf(" ");
		}
	}
	bool isEmptyArray(void * /* volatile  */*s_temp,int l){
		for(int i=0; i<l; i++)
			if(s_temp[i]!=NULL) 
				return false;
		return true;	
	}
	bool checkStoredSpines(void * /* volatile  */*s_head){
		for(int i=1; i<SUB_SIZE; i++)
			if(s_head[i]!=NULL) 
			return false;
		if(s_head[0]==NULL)
			return true;
		else if(isSpine(s_head[0]))
			return checkStoredSpines(unmark_spine(s_head[0]));
		else
			return false;
			
	}
	bool verify_node_belongs(DataNode *N, void * /* volatile  */ *s, int d_pos){
		HASH h=N->hash;
		int pos=h&(MAIN_SIZE-1);
		h=h>>MAIN_POW;
		
		void *node=getNode(head,pos);
		if(isSpine(node)){
			void * /* volatile  */ *local=unmark_spine(node);
			while(true){
				pos=h&(SUB_SIZE-1);
				h=h>>SUB_POW;
				node=getNode(local,pos);
				if(isSpine(node)){
					local=unmark_spine(node);
				}
				else if(local==s && d_pos==pos){
					assert(N==node);
					return true;
				}
				else
					return false;
			}
			
		}
		else if(head==s && d_pos==pos){
			if(N!=node){
				printf("Error-3!\n");
			}
			return true;
		}
		else
			return false;
	}
#endif
};
