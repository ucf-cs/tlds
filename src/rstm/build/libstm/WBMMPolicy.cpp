/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#include <stm/WBMMPolicy.hpp>
using namespace stm;

namespace
{
  /*** figure out if one timestamp is strictly dominated by another */
  inline bool
  is_strictly_older(uint32_t* newer, uint32_t* older, uint32_t old_len)
  {
      for (uint32_t i = 0; i < old_len; ++i)
          if ((newer[i] <= older[i]) && (newer[i] & 1))
              return false;
      return true;
  }
}

pad_word_t stm::trans_nums[MAX_THREADS] = {{0}};


void WBMMPolicy::handle_full_prelimbo()
{
    // get the current timestamp from the epoch
    prelimbo->length = threadcount.val;
    for (uint32_t i = 0, e = prelimbo->length; i < e; ++i)
        prelimbo->ts[i] = trans_nums[i].val;

    // push prelimbo onto the front of the limbo list:
    prelimbo->older = limbo;
    limbo = prelimbo;

    //  check if anything after limbo->head is dominated by ts.  Exit the loop
    //  when the list is empty, or when we find something that is strictly
    //  dominated.
    //
    //  NB: the list is in sorted order by timestamp.
    limbo_t* current = limbo->older;
    limbo_t* prev = limbo;
    while (current != NULL) {
        if (is_strictly_older(limbo->ts, current->ts, current->length))
            break;
        prev = current;
        current = current->older;
    }

    // If current != NULL, it is the head of a list of reclaimables
    if (current) {
        // detach /current/ from the list
        prev->older = NULL;

        // free all blocks in each node's pool and free the node
        while (current != NULL) {
            // free blocks in current's pool
            for (unsigned long i = 0; i < current->POOL_SIZE; i++)
                free(current->pool[i]);

            // free the node and move on
            limbo_t* old = current;
            current = current->older;
            free(old);
        }
    }
    prelimbo = new limbo_t();
}
