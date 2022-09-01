/******************************************************************************
 * skip_cas.c
 *
 * Skip lists, allowing concurrent update by use of CAS primitives.
 *
 * Copyright (c) 2001-2003, K A Fraser
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

#define __SET_IMPLEMENTATION__

#include "lockfreeskip.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "common/fraser/portable_defns.h"
#include "common/fraser/ptst.h"

/*
 * SKIP LIST
 */

typedef struct node_st node_t;
typedef struct fr_set_st boost_skip;
typedef VOLATILE node_t *sh_node_pt;

struct node_st {
  int level;
#define LEVEL_MASK 0x0ff
#define READY_FOR_FREE 0x100
  setkey_t k;
  setval_t v;
  sh_node_pt next[1];
};

struct fr_set_st {
  node_t *tail;
  node_t head;
};

static int gc_id[NUM_LEVELS];

/*
 * PRIVATE FUNCTIONS
 */

/*
 * Random level generator. Drop-off rate is 0.5 per level.
 * Returns value 1 <= level <= NUM_LEVELS.
 */
static int get_level(ptst_t *ptst) {
  unsigned long r = rand_next(ptst);
  int l = 1;
  r = (r >> 4) & ((1 << (NUM_LEVELS - 1)) - 1);
  while ((r & 1)) {
    l++;
    r >>= 1;
  }
  return (l);
}

/*
 * Allocate a new node, and initialise its @level field.
 * NB. Initialisation will eventually be pushed into garbage collector,
 * because of dependent read reordering.
 */
static node_t *alloc_node(ptst_t *ptst) {
  int l;
  node_t *n;
  l = get_level(ptst);
  n = fr_gc_alloc(ptst, gc_id[l - 1]);
  n->level = l;

  return (n);
}

/* Free a node to the garbage collector. */
static void free_node(ptst_t *ptst, sh_node_pt n) {
  fr_gc_free(ptst, (void *)n, gc_id[(n->level & LEVEL_MASK) - 1]);
}

/*
 * Search for first non-deleted node, N, with key >= @k at each level in @l.
 * RETURN VALUES:
 *  Array @pa: @pa[i] is non-deleted predecessor of N at level i
 *  Array @na: @na[i] is N itself, which should be pointed at by @pa[i]
 *  MAIN RETURN VALUE: same as @na[0].
 */
static sh_node_pt strong_search_predecessors(boost_skip *l, setkey_t k,
                                             sh_node_pt *pa, sh_node_pt *na) {
  sh_node_pt x, x_next, old_x_next, y, y_next;
  setkey_t y_k;
  int i;

retry:
  RMB();

  x = &l->head;
  for (i = NUM_LEVELS - 1; i >= 0; i--) {
    /* We start our search at previous level's unmarked predecessor. */
    READ_FIELD(x_next, x->next[i]);
    /* If this pointer's marked, so is @pa[i+1]. May as well retry. */
    if (is_marked_ref(x_next)) goto retry;

    for (y = x_next;; y = y_next) {
      /* Shift over a sequence of marked nodes. */
      for (;;) {
        READ_FIELD(y_next, y->next[i]);
        if (!is_marked_ref(y_next)) break;
        y = get_unmarked_ref(y_next);
      }

      READ_FIELD(y_k, y->k);
      if (y_k >= k) break;

      /* Update estimate of predecessor at this level. */
      x = y;
      x_next = y_next;
    }

    /* Swing forward pointer over any marked nodes. */
    if (x_next != y) {
      old_x_next = CASPO(&x->next[i], x_next, y);
      if (old_x_next != x_next) goto retry;
    }

    if (pa) pa[i] = x;
    if (na) na[i] = y;
  }

  return (y);
}

/* This function does not remove marked nodes. Use it optimistically. */
static sh_node_pt weak_search_predecessors(boost_skip *l, setkey_t k,
                                           sh_node_pt *pa, sh_node_pt *na) {
  sh_node_pt x, x_next;
  setkey_t x_next_k;
  int i;

  x = &l->head;
  for (i = NUM_LEVELS - 1; i >= 0; i--) {
    for (;;) {
      READ_FIELD(x_next, x->next[i]);
      x_next = get_unmarked_ref(x_next);

      READ_FIELD(x_next_k, x_next->k);
      if (x_next_k >= k) break;

      x = x_next;
    }

    if (pa) pa[i] = x;
    if (na) na[i] = x_next;
  }

  return (x_next);
}

/*
 * Mark @x deleted at every level in its list from @level down to level 1.
 * When all forward pointers are marked, node is effectively deleted.
 * Future searches will properly remove node by swinging predecessors'
 * forward pointers.
 */
static void mark_deleted(sh_node_pt x, int level) {
  sh_node_pt x_next;

  while (--level >= 0) {
    x_next = x->next[level];
    while (!is_marked_ref(x_next)) {
      x_next = CASPO(&x->next[level], x_next, get_marked_ref(x_next));
    }
    WEAK_DEP_ORDER_WMB(); /* mark in order */
  }
}

static int check_for_full_delete(sh_node_pt x) {
  int level = x->level;
  return ((level & READY_FOR_FREE) ||
          (CASIO(&x->level, level, level | READY_FOR_FREE) != level));
}

static void do_full_delete(ptst_t *ptst, boost_skip *l, sh_node_pt x,
                           int level) {
  int k = x->k;
#ifdef WEAK_MEM_ORDER
  sh_node_pt preds[NUM_LEVELS];
  int i = level;
retry:
  (void)strong_search_predecessors(l, k, preds, NULL);
  /*
   * Above level 1, references to @x can disappear if a node is inserted
   * immediately before and we see an old value for its forward pointer. This
   * is a conservative way of checking for that situation.
   */
  if (i > 0) RMB();
  while (i > 0) {
    node_t *n = get_unmarked_ref(preds[i]->next[i]);
    while (n->k < k) {
      n = get_unmarked_ref(n->next[i]);
      RMB(); /* we don't want refs to @x to "disappear" */
    }
    if (n == x) goto retry;
    i--; /* don't need to check this level again, even if we retry. */
  }
#else
  (void)strong_search_predecessors(l, k, NULL, NULL);
#endif
  free_node(ptst, x);
}

/*
 * PUBLIC FUNCTIONS
 */

boost_skip *boostskip_alloc(void) {
  boost_skip *l;
  node_t *n;
  int i;

  n = malloc(sizeof(*n) + (NUM_LEVELS - 1) * sizeof(node_t *));
  memset(n, 0, sizeof(*n) + (NUM_LEVELS - 1) * sizeof(node_t *));
  n->k = SENTINEL_KEYMAX;

  /*
   * Set the forward pointers of final node to other than NULL,
   * otherwise READ_FIELD() will continually execute costly barriers.
   * Note use of 0xfe -- that doesn't look like a marked value!
   */
  memset(n->next, 0xfe, NUM_LEVELS * sizeof(node_t *));

  l = malloc(sizeof(*l) + (NUM_LEVELS - 1) * sizeof(node_t *));
  l->head.k = SENTINEL_KEYMIN;
  l->head.level = NUM_LEVELS;
  for (i = 0; i < NUM_LEVELS; i++) {
    l->head.next[i] = n;
  }

  l->tail = n;

  return (l);
}

setval_t set_update(boost_skip *l, setkey_t k, setval_t v, int overwrite) {
  setval_t ov, new_ov;
  ptst_t *ptst;
  sh_node_pt preds[NUM_LEVELS], succs[NUM_LEVELS];
  sh_node_pt pred, succ, new = NULL, new_next, old_next;
  int i, level;

  k = CALLER_TO_INTERNAL_KEY(k);

  ptst = fr_critical_enter();

  succ = weak_search_predecessors(l, k, preds, succs);

retry:
  ov = NULL;

  if (succ->k == k) {
    /* Already a @k node in the list: update its mapping. */
    new_ov = succ->v;
    do {
      if ((ov = new_ov) == NULL) {
        /* Finish deleting the node, then retry. */
        READ_FIELD(level, succ->level);
        mark_deleted(succ, level & LEVEL_MASK);
        succ = strong_search_predecessors(l, k, preds, succs);
        goto retry;
      }
    } while (overwrite && ((new_ov = CASPO(&succ->v, ov, v)) != ov));

    if (new != NULL) free_node(ptst, new);
    goto out;
  }

#ifdef WEAK_MEM_ORDER
  /* Free node from previous attempt, if this is a retry. */
  if (new != NULL) {
    free_node(ptst, new);
    new = NULL;
  }
#endif

  /* Not in the list, so initialise a new node for insertion. */
  if (new == NULL) {
    new = alloc_node(ptst);
    new->k = k;
    new->v = v;
  }
  level = new->level;

  /* If successors don't change, this saves us some CAS operations. */
  for (i = 0; i < level; i++) {
    new->next[i] = succs[i];
  }

  /* We've committed when we've inserted at level 1. */
  WMB_NEAR_CAS(); /* make sure node fully initialised before inserting */
  old_next = CASPO(&preds[0]->next[0], succ, new);
  if (old_next != succ) {
    succ = strong_search_predecessors(l, k, preds, succs);
    goto retry;
  }

  /* Insert at each of the other levels in turn. */
  i = 1;
  while (i < level) {
    pred = preds[i];
    succ = succs[i];

    /* Someone *can* delete @new under our feet! */
    new_next = new->next[i];
    if (is_marked_ref(new_next)) goto success;

    /* Ensure forward pointer of new node is up to date. */
    if (new_next != succ) {
      old_next = CASPO(&new->next[i], new_next, succ);
      if (is_marked_ref(old_next)) goto success;
      assert(old_next == new_next);
    }

    /* Ensure we have unique key values at every level. */
    if (succ->k == k) goto new_world_view;
    assert((pred->k < k) && (succ->k > k));

    /* Replumb predecessor's forward pointer. */
    old_next = CASPO(&pred->next[i], succ, new);
    if (old_next != succ) {
    new_world_view:
      RMB(); /* get up-to-date view of the world. */
      (void)strong_search_predecessors(l, k, preds, succs);
      continue;
    }

    /* Succeeded at this level. */
    i++;
  }

success:
  /* Ensure node is visible at all levels before punting deletion. */
  WEAK_DEP_ORDER_WMB();
  if (check_for_full_delete(new)) {
    MB(); /* make sure we see all marks in @new. */
    do_full_delete(ptst, l, new, level - 1);
  }
out:
  fr_critical_exit(ptst);
  return (ov);
}

setval_t set_remove(boost_skip *l, setkey_t k) {
  setval_t v = NULL, new_v;
  ptst_t *ptst;
  sh_node_pt preds[NUM_LEVELS], x;
  int level, i;

  k = CALLER_TO_INTERNAL_KEY(k);

  ptst = fr_critical_enter();

  x = weak_search_predecessors(l, k, preds, NULL);

  if (x->k > k) goto out;

  READ_FIELD(level, x->level);
  level = level & LEVEL_MASK;

  /* Once we've marked the value field, the node is effectively deleted. */
  new_v = x->v;
  do {
    v = new_v;
    if (v == NULL) goto out;
  } while ((new_v = CASPO(&x->v, v, NULL)) != v);

  /* Committed to @x: mark lower-level forward pointers. */
  WEAK_DEP_ORDER_WMB(); /* enforce above as linearisation point */
  mark_deleted(x, level);

  /*
   * We must swing predecessors' pointers, or we can end up with
   * an unbounded number of marked but not fully deleted nodes.
   * Doing this creates a bound equal to number of threads in the system.
   * Furthermore, we can't legitimately call 'free_node' until all shared
   * references are gone.
   */
  for (i = level - 1; i >= 0; i--) {
    if (CASPO(&preds[i]->next[i], x, get_unmarked_ref(x->next[i])) != x) {
      if ((i != (level - 1)) || check_for_full_delete(x)) {
        MB(); /* make sure we see node at all levels. */
        do_full_delete(ptst, l, x, i);
      }
      goto out;
    }
  }

  free_node(ptst, x);

out:
  fr_critical_exit(ptst);
  return (v);
}

setval_t set_lookup(boost_skip *l, setkey_t k) {
  setval_t v = NULL;
  ptst_t *ptst;
  sh_node_pt x;

  k = CALLER_TO_INTERNAL_KEY(k);

  ptst = fr_critical_enter();

  x = weak_search_predecessors(l, k, NULL, NULL);
  if (x->k == k) READ_FIELD(v, x->v);

  fr_critical_exit(ptst);
  return (v);
}

void init_boostskip_subsystem(void) {
  int i;

  fr_init_ptst_subsystem();
  fr_init_gc_subsystem();

  for (i = 0; i < NUM_LEVELS; i++) {
    gc_id[i] = fr_gc_add_allocator(sizeof(node_t) + i * sizeof(node_t *));
  }
}

void destroy_boostskip_subsystem(void) { fr_destroy_gc_subsystem(); }

void boostskip_print(boost_skip *l) {
  node_t *curr = l->head.next[0];

  while (curr != l->tail) {
    printf("Node [%p] Key [%u]\n", curr, curr->k - 2);
    curr = (node_t *)get_unmarked_ref(curr->next[0]);
  }
}
