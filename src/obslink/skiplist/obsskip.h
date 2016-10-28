#ifndef __OBSSKIP_H__
#define __OBSSKIP_H__


#include <cstdint>
#include "common/assert.h"
#include "common/allocator.h"

typedef unsigned long setkey_t;
typedef void         *setval_t;


#ifdef __SET_IMPLEMENTATION__

/*************************************
 * INTERNAL DEFINITIONS
 */

/* Fine for 2^NUM_LEVELS nodes. */
#define NUM_LEVELS 20


/* Internal key values with special meanings. */
#define INVALID_FIELD   (0)    /* Uninitialised field value.     */
#define SENTINEL_KEYMIN ( 1UL) /* Key value of first dummy node. */
#define SENTINEL_KEYMAX (~0UL) /* Key value of last dummy node.  */


/*
 * Used internally be set access functions, so that callers can use
 * key values 0 and 1, without knowing these have special meanings.
 */
#define CALLER_TO_INTERNAL_KEY(_k) ((_k) + 2)
#define INTERNAL_TO_CALLER_KEY(_k) ((_k) - 2)


/*
 * SUPPORT FOR WEAK ORDERING OF MEMORY ACCESSES
 */

#ifdef WEAK_MEM_ORDER

/* Read field @_f into variable @_x. */
#define READ_FIELD(_x,_f)                                       \
do {                                                            \
    (_x) = (_f);                                                \
    if ( (_x) == INVALID_FIELD ) { RMB(); (_x) = (_f); }        \
    assert((_x) != INVALID_FIELD);                              \
} while ( 0 )

#else

/* Read field @_f into variable @_x. */
#define READ_FIELD(_x,_f) ((_x) = (_f))

#endif

#endif /* __SET_IMPLEMENTATION__ */

/*************************************
 * PUBLIC DEFINITIONS
 */

/*************************************
 * Transaction Definitions
 */

struct Operator_o
{
    uint8_t type;
    uint32_t key;
};

struct Desc_o
{
    static size_t SizeOf(uint8_t size)
    {
        return sizeof(uint8_t) + sizeof(uint8_t) + sizeof(Operator_o) * size;
    }

    volatile uint8_t status;
    uint8_t size;
    Operator_o ops[];
};

struct NodeDesc_o
{
    NodeDesc_o(Desc_o* _desc, uint8_t _opid)
        : desc(_desc), opid(_opid){}

    Desc_o* desc;
    uint8_t opid;
};

struct node_t_o
{
    int        level;
#define LEVEL_MASK     0x0ff
#define READY_FOR_FREE 0x100
    setkey_t  k;
    setval_t  v;

    NodeDesc_o* nodeDesc;

    node_t_o* next[1];
};

struct obs_skip
{
    Allocator<Desc_o>* descAllocator;
    Allocator<NodeDesc_o>* nodeDescAllocator;

    node_t_o* tail;
    node_t_o head;
};


/*
 * Key range accepted by set functions.
 * We lose three values (conveniently at top end of key space).
 *  - Known invalid value to which all fields are initialised.
 *  - Sentinel key values for up to two dummy nodes.
 */
#define KEY_MIN  ( 0U)
#define KEY_MAX  ((~0U) - 3)


void init_transskip_subsystem(void);
void destroy_transskip_subsystem(void);


bool execute_ops(obs_skip* l, Desc_o* desc);

/*
 * Allocate an empty set.
 */
obs_skip *obsskip_alloc(Allocator<Desc_o>* _descAllocator, Allocator<NodeDesc_o>* _nodeDescAllocator);

void  transskip_free(obs_skip* l);

#endif /* __SET_H__ */
