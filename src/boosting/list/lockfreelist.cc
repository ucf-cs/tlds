#include "boosting/list/lockfreelist.h"

#include <limits.h>

#include <cstdio>
#include <cstdlib>

inline bool is_marked_ref(LockfreeList::Node* i) {
  return (bool)((intptr_t)i & 0x1L);
}

inline LockfreeList::Node* get_unmarked_ref(LockfreeList::Node* w) {
  return (LockfreeList::Node*)((intptr_t)w & ~0x1L);
}

inline LockfreeList::Node* get_marked_ref(LockfreeList::Node* w) {
  return (LockfreeList::Node*)((intptr_t)w | 0x1L);
}

/*
 * list_search looks for value val, it
 *  - returns right_node owning val (if present) or its immediately higher
 *    value present in the list (otherwise) and
 *  - sets the left_node to the node owning the value immediately lower than
 * val. Encountered nodes that are marked as logically deleted are physically
 * removed from the list, yet not garbage collected.
 */

LockfreeList::Node* LockfreeList::LocatePred(uint32_t key, Node** left) {
  Node* right;
  Node* left_next;

repeat_search:
  do {
    // Step 1: Traverse the list and find left (<val) and right (>=val).
    Node* i = m_head;
    Node* i_next = i->next;

    do {
      if (!is_marked_ref(i_next)) {
        (*left) = i;
        left_next = i_next;
      }

      i = get_unmarked_ref(i_next);

      if (i == m_tail) {
        break;
      }

      i_next = i->next;

    } while (is_marked_ref(i_next) || i->key < key);

    right = i;

    // Step 2: check nodes are adjacent
    if (left_next == right) {
      if ((right != m_tail) && is_marked_ref(right->next)) {
        goto repeat_search;
      } else {
        return right;
      }
    }

    // Step 3: If there are marked nodes between left and right try to "remove"
    // them.
    if (__sync_bool_compare_and_swap(&(*left)->next, left_next, right)) {
      if (right != m_tail && is_marked_ref(right->next))
        goto repeat_search;
      else
        return right;
    }
  } while (true);
}

// Returns 1 if found, 0 otherwise.
bool LockfreeList::Find(uint32_t key) {
  Node* left;
  return (LocatePred(key, &left)->key == key);
}

// Returns a new list and initializes the memory pool.
LockfreeList::LockfreeList() {
  // Initialize tail.
  m_tail = (Node*)malloc(sizeof(Node));
  m_tail->key = 0xffffffff;
  m_tail->next = NULL;

  // Initialize head.
  m_head = (Node*)malloc(sizeof(Node));
  m_head->key = 0;
  m_head->next = m_tail;

  // Initialize the memory pool and the first block.
  memptr = 0;
  mem = (Node**)malloc(MEM_BLOCK_CNT * sizeof(Node));
  mem[0] = (Node*)malloc(MEM_BLOCK_SIZE * sizeof(Node));
}

// Deletes the entire memory pool and the entire list.
LockfreeList::~LockfreeList() {
  int i = 0;
  while (mem[i] != NULL) {
    free(mem[i]);
    i += 1;
  }

  // free(mem);
  free(m_head);
  free(m_tail);
}

// Traverses the list and increments size counter.
int LockfreeList::Size() {
  int size = 0;  // without head + tail.
  Node* i = m_head->next;
  while (i != m_tail) {
    size += 1;
    i = get_unmarked_ref(i->next);
  }
  return size;
}

// Inserts a new node with value val in the list and returns 1,
// or returns 0 if a node with that value already exists.
bool LockfreeList::Insert(uint32_t key) {
  Node* n = NULL;
  Node* left;
  Node* right;

  do {
    // Search for left and right nodes.
    right = LocatePred(key, &left);
    if (right->key == key) {
      return false;  // already exists.
    }

    // n does not exist! Initialize it and insert it.
    if (n == NULL) {
      // Fetch and increment the global memory pointer.
      uint32_t my_memptr = __sync_fetch_and_add(&memptr, 1);
      // Figure out what block to use.
      uint32_t my_memblock = my_memptr / MEM_BLOCK_SIZE;

      // If that block is a new one, initialize it.
      if (mem[my_memblock] == NULL) {
        Node* tmpmem = (Node*)malloc(MEM_BLOCK_SIZE * sizeof(Node));
        // Only one succeeds to initialize it. The rest free the temporary
        // malloc.
        if (!__sync_bool_compare_and_swap(&mem[my_memblock], NULL, tmpmem)) {
          free(tmpmem);
        }
      }
      // Assign n a place in memory.
      n = &mem[my_memblock][my_memptr % MEM_BLOCK_SIZE];
      n->key = key;
    }
    n->next = right;  // point to right.

    // try to change left->next to point to n instead of right.
    if (__sync_bool_compare_and_swap(&(left->next), right, n)) {
      return true;
    }
    // If CAS fails, someone messed up. Retry!
  } while (true);
}

// Removes the node with value val from the list and returns 1,
// or returns 0 if the node with that value did not exists.
// The node is "removed" by marking its "next" field.
bool LockfreeList::Delete(uint32_t key) {
  Node* left;
  Node* right;

  do {
    // Search for left and right nodes.
    right = LocatePred(key, &left);
    if (right->key != key) {
      return false;  // does not exist.
    }

    // n exists! Try to mark right->next.
    if (__sync_bool_compare_and_swap(&(right->next),
                                     get_unmarked_ref(right->next),
                                     get_marked_ref(right->next))) {
      // Also try to link left with right->next.
      // if it fails it's ok - someone else fixed it.
      __sync_bool_compare_and_swap(&(left->next), right,
                                   get_unmarked_ref(right->next));
      return true;
    }

    // If CAS fails, something changed. Retry!
  } while (true);
}

void LockfreeList::Print() {
  Node* curr = m_head->next;

  while (curr) {
    printf("Node [%p] Key [%u]\n", curr, curr->key);
    curr = curr->next;
  }
}
