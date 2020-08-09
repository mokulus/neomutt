/**
 * @file
 * Linear Array data structure
 *
 * @authors
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @page array Linear array API
 *
 * API to store contiguous elements.
 */

#include <stdbool.h>
#include "mutt/memory.h"

/**
 * ARRAY_HEADROOM - additional number of elements to reserve, to prevent
 * frequent reallocations.
 */
#define ARRAY_HEADROOM 25

/**
 * ARRAY_HEAD - define a named struct for arrays of elements of a certain type.
 * @param name Name of the resulting struct
 * @param type Type of the elements stored in the array
 */
#define ARRAY_HEAD(name, type) \
  struct name { \
    size_t size, capacity; \
    type *entries; \
  }

/**
 * ARRAY_HEAD_INITIALIZER - static initializer for arrays.
 */
#define ARRAY_HEAD_INITIALIZER \
  { 0, 0, NULL }

/**
 * ARRAY_INIT - initialize an array.
 * @param head Pointer to a struct defined using ARRAY_HEAD
 */
#define ARRAY_INIT(head) \
  memset((head), 0, sizeof(*(head)))

/**
 * ARRAY_EMPTY - check if an array is empty.
 * @param head Pointer to a struct defined using ARRAY_HEAD
 * @retval true The array is empty
 * @retval false The array is not empty
 */
#define ARRAY_EMPTY(head) \
  ((head)->size == 0)

/**
 * ARRAY_SIZE - return the number of elements stored.
 * @param head Pointer to a struct defined using ARRAY_HEAD
 * @retval size The number of elements stored
 *
 * @note Because it is possible to add elements in the middle of the array,
 * see ARRAY_SET, the number returned by ARRAY_SIZE can be larger than the
 * number of elements actually stored. Holes are filled with zero at
 * ARRAY_RESERVE time and are left untouched by ARRAY_SHRINK.
 */
#define ARRAY_SIZE(head) \
  ((head)->size)

/**
 * ARRAY_CAPACITY - return the number of elements the array can store without
 * reallocation.
 * @param head Pointer to a struct defined using ARRAY_HEAD
 * @retval cap The capacity of the array
 */
#define ARRAY_CAPACITY(head) \
  ((head)->capacity)

/**
 * ARRAY_GET - return element at index.
 * @param head Pointer to a struct defined using ARRAY_HEAD
 * @param idx  Index, between 0 and ARRAY_SIZE-1
 * @retval elem Pointer to the element at the given index
 * @retval NULL Index was out of bounds
 * @note Because it is possible to add elements in the middle of the array, it
 * is also possible to retrieve elements that weren't previously explicitly
 * set. In that case, the memory returned is all zeroes.
 */
#define ARRAY_GET(head, idx) \
  ((head)->size > (idx) ? (head)->entries[(idx)] : NULL)

/**
 * ARRAY_SET - set an element in the array.
 * @param head Pointer to a struct defined using ARRAY_HEAD
 * @param idx  Index, between 0 and ARRAY_SIZE-1
 * @param elem Element to copy
 * @retval true Element was inserted
 * @retval false Element was not inserted, array was full
 * @note This method has the side effect of changing the array size, if the
 * insertion happens after the last element.
 */
#define ARRAY_SET(head, idx, elem) \
  (((head)->capacity > (idx) \
    ? true \
    : ARRAY_RESERVE((head), (idx) + 1)), \
   ARRAY_SET_NORESERVE((head), (idx), (elem)))

/**
 * ARRAY_ADD - add an element at the end of the array.
 * @param head Pointer to a struct defined using ARRAY_HEAD
 * @param elem Element to copy
 * @retval true Element was added
 * @retval false Element was not added, array was full
 */
#define ARRAY_ADD(head, elem) \
  (((head)->capacity > (head)->size \
    ? true \
    : ARRAY_RESERVE((head), (head)->size + 1)), \
   ARRAY_ADD_NORESERVE((head), (elem)))

/**
 * ARRAY_SHRINK - mark a number of slots at the end of the array as unused.
 * @param head Pointer to a struct defined using ARRAY_HEAD
 * @param num  Number of slots to mark as unused
 * @retval New size of the array
 * @note This method does not do any memory management and has no effect on the
 * capacity nor the contents of the array. It is just a resize which only works
 * downwards.
 */
#define ARRAY_SHRINK(head, num) \
  ((head)->size -= MIN((num), (head)->size))

/**
 * ARRAY_ELEM_SIZE - number of bytes occupied by an element of this array
 * @param head Pointer to a struct defined using ARRAY_HEAD
 * @retval Number of bytes per element
 */
#define ARRAY_ELEM_SIZE(head) \
  (sizeof(*(head)->entries))

/**
 * ARRAY_RESERVE - reserve memory for the array
 * @param head Pointer to a struct defined using ARRAY_HEAD
 * @param num Number of elements to make room for
 * @retval New capacity of the array
 */
#define ARRAY_RESERVE(head, num) \
  (((head)->capacity > (num)) \
   ? (head)->capacity \
   : ((mutt_mem_realloc(&(head)->entries, ((num) + ARRAY_HEADROOM) * ARRAY_ELEM_SIZE(head))), \
      (memset((head)->entries + (head)->capacity, 0, \
              ((num) + ARRAY_HEADROOM - (head)->capacity) * ARRAY_ELEM_SIZE(head))), \
      ((head)->capacity = (num) + ARRAY_HEADROOM)))

/**
 * ARRAY_FREE - release all memory
 * @param head Pointer to a struct defined using ARRAY_HEAD
 * @retval 0
 */
#define ARRAY_FREE(head) \
  (FREE(&(head)->entries), (head)->size = (head)->capacity = 0)

/**
 * ARRAY_FOREACH - iterate over all elements of the array
 * @param elem Variable to be used as pointer to the element at each iteration
 * @param head Pointer to a struct defined using ARRAY_HEAD
 */
#define ARRAY_FOREACH(elem, head) \
  ARRAY_FOREACH_FROM_TO((elem), (head), 0, (head)->size)

/**
 * ARRAY_FOREACH_FROM - iterate from an index to the end
 * @param elem Variable to be used as pointer to the element at each iteration
 * @param head Pointer to a struct defined using ARRAY_HEAD
 * @param from Staring index (inclusive)
 * @note The from index must be between 0 and ARRAY_SIZE(head)
 */
#define ARRAY_FOREACH_FROM(elem, head, from) \
  ARRAY_FOREACH_FROM_TO((elem), (head), (from), (head)->size)

/**
 * ARRAY_FOREACH_TO - iterate from the beginning to an index
 * @param elem Variable to be used as pointer to the element at each iteration
 * @param head Pointer to a struct defined using ARRAY_HEAD
 * @param to   Terminating index (exclusive)
 * @note The to index must be between 0 and ARRAY_SIZE(head)
 */
#define ARRAY_FOREACH_TO(elem, head, to) \
  ARRAY_FOREACH_FROM_TO((elem), (head), 0, (to))

/**
 * ARRAY_FOREACH_FROM_TO  - iterate between two indexes
 * @param elem Variable to be used as pointer to the element at each iteration
 * @param head Pointer to a struct defined using ARRAY_HEAD
 * @param from Starting index (inclusive)
 * @param to   Terminating index (exclusive)
 * @note The from and to indexes must be between 0 and ARRAY_SIZE(head); the
 * from index must not be bigger than to index.
 */
#define ARRAY_FOREACH_FROM_TO(elem, head, from, to) \
  for ((elem)  = &(head)->entries[(from)]; \
       (elem) != &(head)->entries[(to)]; \
       (elem)++)

/**
 * ARRAY_IDX - return the index of an element of the array
 * @param head Pointer to a struct defined using ARRAY_HEAD
 * @param elem Pointer to an element of the array
 * @retval idx The index of element in the array
 */
#define ARRAY_IDX(head, elem) \
  (elem - (head)->entries)

/******************************************************************************
 * Internal APIs
 *****************************************************************************/
#define ARRAY_SET_NORESERVE(head, idx, elem) \
  ((head)->capacity > (idx) \
   ? (((head)->size = MAX((head)->size, ((idx) + 1))), \
      ((head)->entries[(idx)] = (elem)), \
      true) \
   : false)

#define ARRAY_ADD_NORESERVE(head, elem) \
  ((head)->capacity > (head)->size \
   ? (((head)->entries[(head)->size++] = (elem)), \
      true) \
   : false)

