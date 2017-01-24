#ifndef __STMSKIP_H__
#define __STMSKIP_H__

#include <stdint.h>

typedef unsigned long setkey_t;
typedef void         *setval_t;

typedef struct set_op_st
{
    uint8_t type;
    uint32_t key;
}set_op;


#ifdef __STMSKIP_IMPLEMENTATION__

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


#else

/*************************************
 * PUBLIC DEFINITIONS
 */


/*
 * Key range accepted by set functions.
 * We lose three values (conveniently at top end of key space).
 *  - Known invalid value to which all fields are initialised.
 *  - Sentinel key values for up to two dummy nodes.
 */
#define KEY_MIN  ( 0U)
#define KEY_MAX  ((~0U) - 3)

typedef void stm_skip; /* opaque */

void init_stmskip_subsystem(void);
void destory_stmskip_subsystem(void);

/*
 * Allocate an empty set.
 */
stm_skip *stmskip_alloc(void);

bool stmskip_execute_ops(stm_skip* l, set_op ops[], int op_size);

void ResetMetrics();

#endif /* __SET_IMPLEMENTATION__ */

#endif /* __SET_H__ */
