#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * lstack.h
 * This file contains macros to manipulate a singly-linked list as a stack.
 *
 * To use lstack, your structure must have a "next" pointer.
 *
 * ----------------.EXAMPLE -------------------------
 * struct item {
 *      int id;
 *      struct item *next;
 * };
 *
 * struct item *stack = NULL;
 *
 * int main() {
 *      int count;
 *      struct item *tmp;
 *      struct item *item = malloc(sizeof *item);
 *      item->id = 42;
 *      lstack_size(stack, tmp, count);
 *      assert(count == 0);
 *      lstack_push(stack, item);
 *      lstack_size(stack, tmp, count);
 *      assert(count == 1);
 *      lstack_pop(stack, item);
 *      free(item);
 *      lstack_size(stack, tmp, count);
 *      assert(count == 0);
 * }
 * --------------------------------------------------
 */

#define lstack_top(head) (head)

#define lstack_empty(head) (!(head))

#define lstack_push(head, add) \
    lstack_push_(head, add, next)

#define lstack_push_(head, add, next) \
    do                                \
    {                                 \
        (add)->next = (head);         \
        (head) = (add);               \
    } while (0)

#define lstack_pop(head, result) \
    lstack_pop_(head, result, next)

#define lstack_pop_(head, result, next) \
    do                                  \
    {                                   \
        (result) = (head);              \
        (head) = (head)->next;          \
    } while (0)

#define lstack_size(head, el, counter) \
    lstack_size_(head, el, counter, next)

#define lstack_size_(head, el, counter, next)      \
    do                                             \
    {                                              \
        (counter) = 0;                             \
        for ((el) = (head); el; (el) = (el)->next) \
        {                                          \
            ++(counter);                           \
        }                                          \
    } while (0)

#ifdef __cplusplus
}
#endif
