#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "list.h"

// supports safe traversal deletion

struct safe_list
{
    struct list_head list;
    struct safe_list_iterator *i;
};

struct safe_list_iterator
{
    struct safe_list_iterator **head;
    struct safe_list_iterator *next_i;
    struct safe_list *next;
};

#define INIT_SAFE_LIST(_head)       \
    do                              \
    {                               \
        INIT_LIST_HEAD(_head.list); \
        (_head)->i = NULL;          \
    } while (0)

#define SAFE_LIST_INIT(_name)            \
    {                                    \
        LIST_HEAD_INIT(_name.list), NULL \
    }
#define SAFE_LIST(_name) struct safe_list _name = SAFE_LIST_INIT(_name)

static inline int safe_list_empty(struct safe_list *head)
{
    return list_empty(&head->list);
}

static inline void
__safe_list_set_iterator(struct safe_list *list,
                         struct safe_list_iterator *i)
{
    struct safe_list_iterator *next_i;
    struct safe_list *next;

    next = list_entry(list->list.next, struct safe_list, list);
    next_i = next->i;

    next->i = i;
    i->next = next;
    i->head = &next->i;

    i->next_i = next_i;
    if (next_i)
        next_i->head = &i->next_i;
}

static inline void
__safe_list_del_iterator(struct safe_list_iterator *i)
{
    *i->head = i->next_i;
    if (i->next_i)
        i->next_i->head = i->head;
}

static inline void
__safe_list_move_iterator(struct safe_list *list,
                          struct safe_list_iterator *i)
{
    __safe_list_del_iterator(i);
    __safe_list_set_iterator(list, i);
}

static inline int safe_list_for_each(struct safe_list *head,
                                     int (*cb)(void *ctx, struct safe_list *list),
                                     void *ctx)
{
    struct safe_list_iterator i;
    struct safe_list *cur;
    int ret = 0;

    for (cur = list_entry(head->list.next, struct safe_list, list),
        __safe_list_set_iterator(cur, &i);
         cur != head;
         cur = i.next, __safe_list_move_iterator(cur, &i))
    {
        ret = cb(ctx, cur);
        if (ret)
            break;
    }

    __safe_list_del_iterator(&i);
    return ret;
}

static inline void safe_list_add(struct safe_list *list, struct safe_list *head)
{
    list->i = NULL;
    list_add_tail(&list->list, &head->list);
}

static inline void safe_list_add_first(struct safe_list *list, struct safe_list *head)
{
    list->i = NULL;
    list_add(&list->list, &head->list);
}

static inline void safe_list_del(struct safe_list *list)
{
    struct safe_list_iterator *i, *next_i, **tail;
    struct safe_list *next;

    next = list_entry(list->list.next, struct safe_list, list);
    list_del(&list->list);

    if (!list->i)
        return;

    next_i = next->i;
    tail = &next->i;

    for (i = list->i; i; i = i->next_i)
    {
        tail = &i->next_i;
        i->next = next;
    }

    next->i = list->i;
    list->i->head = &next->i;
    *tail = next_i;
    if (next_i)
        next_i->head = tail;

    list->i = NULL;
}

#ifdef __cplusplus
}
#endif
