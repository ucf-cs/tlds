/******************************************************************************
 * skip_stm.c
 * 
 * Skip lists, allowing concurrent update by use of the STM abstraction.
 * 
 * Copyright (c) 2003, K A Fraser
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define __STMSKIP_IMPLEMENTATION__

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
extern "C"{
#include "common/fraser/portable_defns.h"
#include "common/fraser/gc.h"
#include "common/fraser/stm.h"
}
#include "stmskip.h"


static uint32_t g_count_commit = 0;
static uint32_t g_count_abort = 0;
static uint32_t g_count_real_abort = 0;

enum SetOpType
{
    FIND = 0,
    INSERT,
    DELETE
};

typedef struct node_st node_t;
typedef stm_blk set_t;

struct node_st
{
    int       level;
    setkey_t  k;
    setval_t  v;
    stm_blk  *next[NUM_LEVELS];
};

static struct {
    CACHE_PAD(0);
    stm *memory;    /* read-only */
    CACHE_PAD(2);
} shared;

#define MEMORY (shared.memory)

/*
 * Random level generator. Drop-off rate is 0.5 per level.
 * Returns value 1 <= level <= NUM_LEVELS.
 */
static int get_level(ptst_t *ptst)
{
    unsigned long r = rand_next(ptst);
    int l = 1;
    r = (r >> 4) & ((1 << (NUM_LEVELS-1)) - 1);
    while ( (r & 1) ) { l++; r >>= 1; }
    return l;
}


/*
 * Search for first non-deleted node, N, with key >= @k at each level in @l.
 * RETURN VALUES:
 *  Array @pa: @pa[i] is non-deleted predecessor of N at level i
 *  Array @na: @na[i] is N itself, which should be pointed at by @pa[i]
 *  MAIN RETURN VALUE: same as @na[0], direct pointer open for reading.
 */
static node_t *search_predecessors(
    ptst_t *ptst, stm_tx *tx, set_t *l, setkey_t k, stm_blk **pa, stm_blk **na)
{
    stm_blk *xb, *x_nextb;
    node_t  *x,  *x_next;
    int      i;

    xb = l;
    x  = (node_t*)read_stm_blk(ptst, tx, l);
    for ( i = NUM_LEVELS - 1; i >= 0; i-- )
    {
        for ( ; ; )
        {
            x_nextb = x->next[i];
            x_next  = (node_t*)read_stm_blk(ptst, tx, x_nextb);
            if ( x_next->k >= k ) break;
            xb = x_nextb;
            x  = x_next;
        }

        if ( pa ) pa[i] = xb;
        if ( na ) na[i] = x_nextb;
    }

    return x_next;
}


/*
 * PUBLIC FUNCTIONS
 */

set_t *stmskip_alloc(void)
{
    ptst_t  *ptst;
    stm_blk *hb, *tb;
    node_t  *h, *t;
    int      i;

    ptst = fr_critical_enter();

    tb = new_stm_blk(ptst, MEMORY);
    t  = (node_t*)init_stm_blk(ptst, MEMORY, tb);
    memset(t, 0, sizeof(*t));
    t->k = SENTINEL_KEYMAX;

    hb = new_stm_blk(ptst, MEMORY);
    h  = (node_t*)init_stm_blk(ptst, MEMORY, hb);
    memset(h, 0, sizeof(*h));
    h->k     = SENTINEL_KEYMIN;
    h->level = NUM_LEVELS;
    for ( i = 0; i < NUM_LEVELS; i++ )
        h->next[i] = tb;

    fr_critical_exit(ptst);

    return hb;
}


setval_t stmskip_update(ptst_t *ptst, stm_tx *tx, set_t *l, setkey_t k, setval_t v)
{
    setval_t  ov;
    stm_blk  *bpreds[NUM_LEVELS], *bsuccs[NUM_LEVELS], *newb = NULL;
    node_t   *x, *p, *new_node;
    int       i;

    k = CALLER_TO_INTERNAL_KEY(k);

    x = search_predecessors(ptst, tx, l, k, bpreds, bsuccs);

    if ( x->k == k )
    {
        ov = x->v;
    }
    else
    {
        ov = NULL;

        if ( newb == NULL )
        {
            newb = new_stm_blk(ptst, MEMORY);
            new_node  = (node_t*)init_stm_blk(ptst, MEMORY, newb);
            new_node->k = k;
            new_node->v = v;
            new_node->level = get_level(ptst);
        }

        for ( i = 0; i < new_node->level; i++ )
        {
            new_node->next[i] = bsuccs[i];
            p = (node_t*)write_stm_blk(ptst, tx, bpreds[i]);
            p->next[i] = newb;
        }
    }

    //if ( (ov != NULL) && (newb != NULL) ) 
        //free_stm_blk(ptst, MEMORY, newb);

    return ov;
}


setval_t stmskip_remove(ptst_t *ptst, stm_tx *tx, set_t *l, setkey_t k)
{
    setval_t  v;
    stm_blk  *bpreds[NUM_LEVELS], *bsuccs[NUM_LEVELS];
    node_t   *p, *x;
    int       i;

    k = CALLER_TO_INTERNAL_KEY(k);
  
    x = search_predecessors(ptst, tx, l, k, bpreds, bsuccs);
    if ( x->k == k )
    {
        v = x->v;
        for ( i = 0; i < x->level; i++ )
        {
            p = (node_t*)write_stm_blk(ptst, tx, bpreds[i]);
            p->next[i] = x->next[i];
        }
    }
    else
    {
        v = NULL;
    }

    //if ( v != NULL ) 
        //free_stm_blk(ptst, MEMORY, bsuccs[0]);

    return v;
}


setval_t stmskip_lookup(ptst_t *ptst, stm_tx *tx, set_t *l, setkey_t k)
{
    setval_t  v;
    node_t   *x;

    k = CALLER_TO_INTERNAL_KEY(k);

    x = search_predecessors(ptst, tx, l, k, NULL, NULL);

    v = (x->k == k) ? x->v : NULL;

    return v;
}


bool __attribute__ ((optimize (0))) stmskip_execute_ops(void* s, set_op ops[], int op_size) 
{
    set_t* l = (set_t*)s;

    bool ret = false;

    ptst_t *ptst;
    stm_tx   *tx;

    ptst = fr_critical_enter();
    new_stm_tx(tx, ptst, MEMORY);

    for(int i = 0; i < op_size; ++i)
    {
        setkey_t k = ops[i].key;

        if(ops[i].type == FIND)
        {
            ret = stmskip_lookup(ptst, tx, l, k) != NULL;
        }
        else if(ops[i].type == INSERT)
        {
            ret = stmskip_update(ptst, tx, l, k, (void*)0xf0f0f0f0) == NULL;
        }
        else
        {
            ret = stmskip_remove(ptst, tx, l, k) != NULL;
        }

        if(ret == false)
        {
            abort_stm_tx(ptst, tx);
            __sync_fetch_and_add(&g_count_real_abort, 1);
            break;
        }
    }

    if(commit_stm_tx(ptst, tx))
    {
        __sync_fetch_and_add(&g_count_commit, 1);
    }
    else
    {
        __sync_fetch_and_add(&g_count_abort, 1);
    }

    fr_critical_exit(ptst);

    return ret;
}

void init_stmskip_subsystem(void)
{
    fr_init_ptst_subsystem();
    fr_init_gc_subsystem();

    ptst_t *ptst = fr_critical_enter();
    _init_stm_subsystem(0);
    MEMORY = new_stm(ptst, sizeof(node_t));
    fr_critical_exit(ptst);
}

void destory_stmskip_subsystem(void)
{
    ptst_t *ptst = fr_critical_enter();

    free_stm(ptst, MEMORY);

    fr_critical_exit(ptst);

    printf("Total commit %u, abort (total/fake) %u/%u\n", g_count_commit, g_count_abort, g_count_abort - g_count_real_abort);
}

void ResetMetrics()
{
    g_count_commit = 0;
    g_count_abort = 0;
    g_count_real_abort = 0;
}