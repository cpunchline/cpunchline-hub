#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/* Licensed under Apache License v2.0 - see LICENSE file for details */
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/**
 * struct deq - deque metadata
 * @v: char pointer to malloced memory
 * @head: index of first item in deque
 * @tail: index after last item in deque
 * @len: length of deque
 * @cap: total capacity of deque
 * @min: initial capacity of deque
 * @esz: element size
 * @shrink: flag specifying shrink behavior
 *
 * len is distance between head and tail. cap changes with resizing.
 * shrink must be one of DEQ_NO_SHRINK, DEQ_SHRINK_IF_EMPTY, or DEQ_SHRINK_AT_20PCT.
 * When shrinking, min is the smallest size.
 */

enum deq_flag
{
    DEQ_NO_SHRINK,
    DEQ_SHRINK_IF_EMPTY,
    DEQ_SHRINK_AT_20PCT
};

enum deq_op
{
    DEQ_PUSH,
    DEQ_POP,
    DEQ_SHIFT,
    DEQ_UNSHIFT
};

struct deq
{
    char *v;
    unsigned head, tail, len, cap, min, esz, shrink;
};

/**
 * DEQ_WRAP - declare a wrapper type for struct deq and base type
 * @basetype: the base type to wrap
 *
 * Example:
 *    // init inline, defer malloc to first push/unshift
 *    struct point { int x, y; } a;
 *    DEQ_WRAP(struct point) pointq = DEQ_INIT(sizeof(struct point), 64, DEQ_NO_SHRINK);
 *
 *    // or init and malloc by function call
 *    struct vector3 { double x, y, z; };
 *    typedef DEQ_WRAP(struct vector3) vector3q_t;
 *    vector3q_t v;
 *
 *    if (deq_init(&v, 16, DEQ_SHRINK_IF_EMPTY) == -1)
 *        err(1, "deq_init");
 *
 *    a.x = 1; a.y = 1;
 *    // first malloc for pointq
 *    if (deq_push(&pointq, a) == -1)
 *        err(1, "deq_push");
 */
#define DEQ_WRAP(basetype) \
    union                  \
    {                      \
        struct deq deq;    \
        basetype *v;       \
    }

#define DEQ_INIT_DEQ(esz, min, shrink)        \
    (struct deq)                              \
    {                                         \
        0, 0, 0, 0, 0, (min), (esz), (shrink) \
    }

#define DEQ_INIT(esz, min, shrink)            \
    {                                         \
        .deq = DEQ_INIT_DEQ(esz, min, shrink) \
    }

static inline int deq_resize_(struct deq *q, unsigned n)
{
    char *t;

    assert(q && n > 0 && n >= q->len);

    if (!(t = malloc(q->esz * n)))
        return -1;

    if (q->len)
    {
        unsigned part1 = q->head + q->len <= q->cap ? q->len : q->cap - q->head;
        unsigned part2 = q->len - part1;
        memcpy(t, q->v + q->head * q->esz, q->esz * part1);
        if (part2)
            memcpy(t + q->esz * part1, q->v, q->esz * part2);
    }
    if (q->cap)
        free(q->v);

    q->v = t;
    q->head = 0;
    q->tail = q->len;
    q->cap = n;

    return 0;
}

static inline void deq_reset_(struct deq *q)
{
    assert(q);

    if (q->v)
        free(q->v);

    q->v = 0;
    q->head = q->tail = q->len = q->cap = 0;
}

static inline int deq_op_(struct deq *q, enum deq_op op, unsigned *i)
{
    assert(q && i);
    assert(op == DEQ_PUSH || op == DEQ_POP || op == DEQ_SHIFT || op == DEQ_UNSHIFT);

    switch (op)
    {
        case DEQ_PUSH:
        case DEQ_UNSHIFT:
            if (q->len == q->cap && deq_resize_(q, q->cap == 0 ? q->min : q->cap * 2) == -1)
                return -1;
            break;
        case DEQ_POP:
        case DEQ_SHIFT:
            if (q->cap > q->min)
            {
                if (q->shrink == DEQ_SHRINK_IF_EMPTY && q->len == 1 && deq_resize_(q, q->min) == -1)
                    return -1;
                if (q->shrink == DEQ_SHRINK_AT_20PCT && (q->len - 1) * 5 <= q->cap && deq_resize_(q, q->cap / 2) == -1)
                    return -1;
            }
            if (q->len == 0)
                return 0;
        default:
            break;
    }

    switch (op)
    {
        case DEQ_PUSH:
            *i = q->tail++;
            q->tail %= q->cap;
            q->len++;
            break;
        case DEQ_SHIFT:
            *i = q->head++;
            q->head %= q->cap;
            q->len--;
            break;
        case DEQ_POP:
            q->tail = (q->tail == 0 ? q->cap : q->tail) - 1;
            *i = q->tail;
            q->len--;
            break;
        case DEQ_UNSHIFT:
            q->head = (q->head == 0 ? q->cap : q->head) - 1;
            *i = q->head;
            q->len++;
            break;
        default:
            break;
    }

    return 1;
}

/**
 * deq_init - initialize struct deq and malloc
 * @w: pointer to wrapper
 * @min: initial capacity of deque
 * @shrink: flag specifying shrink behavior
 *
 * Returns: 0 on success, -1 on error
 */
#define deq_init(w, min, shrink) ({                        \
    (w)->deq = DEQ_INIT_DEQ(sizeof(*(w)->v), min, shrink); \
    deq_resize_(&(w)->deq, (min));                         \
})

/**
 * deq_new - malloc wrapper and run deq_init
 * @w: pointer to wrapper
 * @min: initial capacity of deque
 * @shrink: flag specifying shrink behavior
 *
 * Example:
 *    vector3q_t *z;
 *
 *    if (deq_new(z, 16, DEQ_SHRINK_AT_20PCT) == -1)
 *        err(1, "deq_new");
 *    //later
 *    deq_free(z);
 *
 * Returns: 0 on success, -1 on error
 */
#define deq_new(w, min, shrink) ({           \
    w = malloc(sizeof(*w));                  \
    if (w && deq_init(w, min, shrink) == -1) \
    {                                        \
        free(w);                             \
        w = 0;                               \
    }                                        \
    w ? 0 : -1;                              \
})

/**
 * deq_push - add element to end of deque
 * @w: pointer to wrapper
 * @e: element to add
 *
 * Returns: 1 on success, -1 on error
 */
#define deq_push(w, e) ({                           \
    unsigned __i;                                   \
    int __ret = deq_op_(&(w)->deq, DEQ_PUSH, &__i); \
    if (__ret == 1)                                 \
        (w)->v[__i] = (e);                          \
    __ret;                                          \
})

/**
 * deq_unshift - add element to beginning of deque
 * @w: pointer to wrapper
 * @e: element to add
 *
 * Returns: 1 on success, -1 on error
 */
#define deq_unshift(w, e) ({                           \
    unsigned __i;                                      \
    int __ret = deq_op_(&(w)->deq, DEQ_UNSHIFT, &__i); \
    if (__ret == 1)                                    \
        (w)->v[__i] = (e);                             \
    __ret;                                             \
})

/**
 * deq_pop - dequeue element from end of deque
 * @w: pointer to wrapper
 * @e: pointer to receive dequeued element
 *
 * Returns: 1 on success, 0 if deque is empty, -1 on error
 *
 * Example:
 *    DEQ_WRAP(int) w = DEQ_INIT(sizeof(int), 8, DEQ_NO_SHRINK);
 *    int ret, i;
 *    // ... after enqueuing some ints
 *    while ((ret = deq_pop(&w, &i)) == 1)
 *        printf("%d\n", i);
 *    if (ret == -1)
 *        err(1, "deq_pop");
 */
#define deq_pop(w, e) ({                           \
    unsigned __i;                                  \
    int __ret = deq_op_(&(w)->deq, DEQ_POP, &__i); \
    if (__ret == 1)                                \
        *(e) = (w)->v[__i];                        \
    __ret;                                         \
})

/**
 * deq_shift - dequeue element from beginning of deque
 * @w: pointer to wrapper
 * @e: pointer to receive dequeued element
 *
 * Returns: 1 on success, 0 if deque is empty, -1 on error
 */
#define deq_shift(w, e) ({                           \
    unsigned __i;                                    \
    int __ret = deq_op_(&(w)->deq, DEQ_SHIFT, &__i); \
    if (__ret == 1)                                  \
        *(e) = (w)->v[__i];                          \
    __ret;                                           \
})

/**
 * deq_first - get element from beginning of deque, deque is unchanged
 * @w: pointer to wrapper
 * @e: pointer to receive element
 *
 * Returns: 1 on success, 0 if deque is empty
 */
#define deq_first(w, e) ({            \
    int __ret = 0;                    \
    assert(w);                        \
    assert(e);                        \
    if ((w)->deq.len > 0)             \
    {                                 \
        *(e) = (w)->v[(w)->deq.head]; \
        __ret = 1;                    \
    }                                 \
    __ret;                            \
})

/**
 * deq_last - get element from end of deque, deque is unchanged
 * @w: pointer to wrapper
 * @e: pointer to receive element
 *
 * Returns: 1 on success, 0 if deque is empty
 */
#define deq_last(w, e) ({                                                 \
    int __ret = 0;                                                        \
    assert(w);                                                            \
    assert(e);                                                            \
    if ((w)->deq.len > 0)                                                 \
    {                                                                     \
        unsigned __i = (w)->deq.tail == 0 ? (w)->deq.cap : (w)->deq.tail; \
        *(e) = (w)->v[__i - 1];                                           \
        __ret = 1;                                                        \
    }                                                                     \
    __ret;                                                                \
})

/**
 * deq_reset - set struct deq indexes and counters to zero, and free malloced buffer
 * @w: pointer to wrapper
 *
 * Returns: void
 */
#define deq_reset(w)           \
    do                         \
    {                          \
        assert(w);             \
        deq_reset_(&(w)->deq); \
        (w)->v = 0;            \
    } while (0)

/**
 * deq_free - run deq_reset and free malloced wrapper
 * @w: pointer to wrapper
 *
 * Returns: void
 */
#define deq_free(w)   \
    do                \
    {                 \
        deq_reset(w); \
        free(w);      \
        w = 0;        \
    } while (0)

/**
 * deq_len - return deque length
 * @w: pointer to wrapper
 *
 * Returns: int
 */
#define deq_len(w) ({ assert(w); (w)->deq.len; })

/**
 * deq_cap - return deque capacity
 * @w: pointer to wrapper
 *
 * Returns: int
 */
#define deq_cap(w) ({ assert(w); (w)->deq.cap; })

#ifdef __cplusplus
}
#endif
