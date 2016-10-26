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

#define HASH unsigned int
#define KEY_SIZE 32
#define toHash 5
#ifndef SUB_POW
	#define SUB_POW 6
#endif
#define SUB_SIZE POW(SUB_POW)
#define MAX_CAS_FAILURE 10

// template <class KEY, class VALUE>//, typename _tMemory>
#define KEY uint32_t
#define VALUE uint32_t
class TransMap
{
public:

	enum OpStatus
	{
	    MAP_ACTIVE = 0,
	    MAP_COMMITTED,
	    MAP_ABORTED
	};

	// enum ReturnCode
 //    {
 //        OK = 0,
 //        SKIP,
 //        FAIL
 //    };

	enum OpType
	{
	    MAP_FIND = 0,
	    MAP_INSERT,
	    MAP_DELETE,
	    MAP_UPDATE
	};

	struct Operator
	{
	    uint8_t type;
	    uint32_t key;
	    uint32_t value;
	};

    struct Desc
    {
        static size_t SizeOf(uint8_t size)
        {
            return sizeof(uint8_t) + sizeof(uint8_t) + sizeof(Operator) * size;
        }

        // Status of the transaction: values in [0, size] means live txn, values -1 means aborted, value -2 means committed.
        volatile uint8_t status;
        uint8_t size;
        Operator ops[];
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

	TransMap(/*Allocator<Node>* nodeAllocator,*/ Allocator<Desc>* descAllocator, Allocator<NodeDesc>* nodeDescAllocator, uint64_t initalPowerOfTwo, uint64_t numThreads);
	~TransMap();

	bool ExecuteOps(Desc* desc, int threadId);

    //Desc* AllocateDesc(uint8_t size);

    inline bool Insert(Desc* desc, uint8_t opid, KEY k, VALUE v, int T);
    inline bool Update(Desc* desc, uint8_t opid, KEY k,/*VALUE e_value,*/ VALUE v, int T, DataNode*& toReturn);
    inline bool Delete(Desc* desc, uint8_t opid, KEY k, int T);
    inline VALUE Find(Desc* desc, uint8_t opid, KEY k, int T);
    //NOTE: add markfordeletion to Find
	

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

private:
    // Node* m_tail;
    // Node* m_head;

    Allocator<DataNode>* m_nodeAllocator;
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
