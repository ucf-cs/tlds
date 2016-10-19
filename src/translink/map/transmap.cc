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