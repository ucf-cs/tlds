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
	#define SUB_POW 6 //TODO:note subspine size
#endif

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
	#define SUB_SIZE POW(SUB_POW)

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

	void * forceExpandTable(int T,void * /* volatile  */ *local,int pos, void *n, int right);
	

	#ifdef USE_KEY
		inline DataNode * Allocate_Node(VALUE v, KEY k, HASH h, int T, NodeDesc* nodeDesc);
	#else
		inline DataNode * Allocate_Node(VALUE v, HASH h, int T, NodeDesc* nodeDesc);
	#endif
	
	inline void Free_Node_Stack(void *node, int T);
	
	inline bool Allocate_Spine(int T, void* /* volatile  */* s, int pos, DataNode *n1/*current node*/, DataNode *n2/*colliding node*/, int right);
	
	inline void * getSpine();
   
#ifdef useThreadWatch
	inline bool inUse(HASH h, int T);
#endif
	
	inline void * replace_node(void* /* volatile  */ *s , int pos, void *current_node, DataNode *new_node);
	inline void * replace_node(void* /* volatile  */ *s, int pos, DataNode *current_node, void* new_node);
	inline void * replace_node(void* /* volatile  */ *s, int pos, void *current_node, void *new_node);
	inline bool replace_node(void* /* volatile  */ *s, int pos, void *current_node /*, new node=NULL*/);

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
