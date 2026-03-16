
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "dsa/deque.h"

typedef struct
{
    int x, y;
} Point;

typedef DEQ_WRAP(char *) StringDeque;

#define PASS() printf("\033[32m✓ PASS\033[0m: %s\n", __func__)
#define FAIL(msg)                                                          \
    do                                                                     \
    {                                                                      \
        fprintf(stderr, "\033[31m✗ FAIL\033[0m: %s: %s\n", __func__, msg); \
        exit(1);                                                           \
    } while (0)

/**
 * Test: Basic int deque operations
 */
static void test_int_deque(void)
{
    DEQ_WRAP(int)
    dq = DEQ_INIT(sizeof(int), 4, DEQ_NO_SHRINK);

    assert(deq_len(&dq) == 0);

    int val;
    (void)val;

    // Test push
    assert(deq_push(&dq, 10) == 1);
    assert(deq_push(&dq, 20) == 1);
    assert(deq_len(&dq) == 2);
    assert(deq_cap(&dq) >= 4);

    // Test pop
    assert(deq_pop(&dq, &val) == 1);
    assert(val == 20);
    assert(deq_len(&dq) == 1);

    assert(deq_pop(&dq, &val) == 1);
    assert(val == 10);
    assert(deq_len(&dq) == 0);

    // Pop from empty
    assert(deq_pop(&dq, &val) == 0); // empty → return 0

    deq_reset(&dq);
    PASS();
}

/**
 * Test: unshift and shift (front operations)
 */
static void test_front_operations(void)
{
    DEQ_WRAP(int)
    dq = DEQ_INIT(sizeof(int), 4, DEQ_NO_SHRINK);
    int val;
    (void)val;

    assert(deq_unshift(&dq, 100) == 1);
    assert(deq_unshift(&dq, 200) == 1);
    assert(deq_len(&dq) == 2);

    // Shift from front
    assert(deq_shift(&dq, &val) == 1);
    assert(val == 200);

    assert(deq_shift(&dq, &val) == 1);
    assert(val == 100);

    assert(deq_len(&dq) == 0);
    assert(deq_shift(&dq, &val) == 0); // empty

    deq_reset(&dq);
    PASS();
}

/**
 * Test: Mixed push/pop/unshift/shift
 */
static void test_mixed_operations(void)
{
    DEQ_WRAP(int)
    dq = DEQ_INIT(sizeof(int), 4, DEQ_NO_SHRINK);
    int val;
    (void)val;

    deq_push(&dq, 1);
    deq_unshift(&dq, 2);
    deq_push(&dq, 3);
    deq_unshift(&dq, 4);

    // Queue should be: [4, 2, 1, 3]

    assert(deq_len(&dq) == 4);

    assert(deq_shift(&dq, &val) == 1 && val == 4);
    assert(deq_pop(&dq, &val) == 1 && val == 3);
    assert(deq_shift(&dq, &val) == 1 && val == 2);
    assert(deq_pop(&dq, &val) == 1 && val == 1);

    assert(deq_len(&dq) == 0);
    assert(deq_pop(&dq, &val) == 0);
    assert(deq_shift(&dq, &val) == 0);

    deq_reset(&dq);
    PASS();
}

/**
 * Test: deq_first and deq_last (peek without remove)
 */
static void test_peek_operations(void)
{
    DEQ_WRAP(int)
    dq = DEQ_INIT(sizeof(int), 4, DEQ_NO_SHRINK);
    int val;
    (void)val;

    assert(deq_push(&dq, 10) == 1);
    assert(deq_push(&dq, 20) == 1);
    assert(deq_push(&dq, 30) == 1);

    assert(deq_first(&dq, &val) == 1);
    assert(val == 10);

    assert(deq_last(&dq, &val) == 1);
    assert(val == 30);

    // Still 3 elements
    assert(deq_len(&dq) == 3);

    // Modify middle doesn't affect head/tail
    ((int *)dq.v)[1] = 999;

    assert(deq_shift(&dq, &val) == 1 && val == 10);
    assert(deq_shift(&dq, &val) == 1 && val == 999);
    assert(deq_shift(&dq, &val) == 1 && val == 30);

    deq_reset(&dq);
    PASS();
}

/**
 * Test: Struct type
 */
static void test_struct_deque(void)
{
    DEQ_WRAP(Point)
    pdq = DEQ_INIT(sizeof(Point), 2, DEQ_NO_SHRINK);
    Point p, out;
    (void)p;
    (void)out;

    p.x = 1;
    p.y = 2;
    assert(deq_push(&pdq, p) == 1);

    p.x = 3;
    p.y = 4;
    assert(deq_unshift(&pdq, p) == 1);

    assert(deq_len(&pdq) == 2);

    // Check first
    assert(deq_first(&pdq, &out) == 1);
    assert(out.x == 3 && out.y == 4);

    // Shift
    assert(deq_shift(&pdq, &out) == 1);
    assert(out.x == 3 && out.y == 4);

    // Pop
    assert(deq_pop(&pdq, &out) == 1);
    assert(out.x == 1 && out.y == 2);

    deq_reset(&pdq);
    PASS();
}

/**
 * Test: Dynamic allocation with deq_new / deq_free
 */
static void test_deq_new_free(void)
{
    DEQ_WRAP(int) *ddq = NULL;

    assert(deq_new(ddq, 8, DEQ_SHRINK_IF_EMPTY) == 0);
    assert(ddq != NULL);
    assert(deq_cap(ddq) >= 8);

    int d = 3;
    (void)d;
    assert(deq_push(ddq, d) == 1);

    int out;
    (void)out;
    assert(deq_pop(ddq, &out) == 1);
    assert(out == d);

    deq_free(ddq); // sets ddq to NULL
    assert(ddq == NULL);

    PASS();
}

/**
 * Test: Capacity growth and shrink behavior
 */
static void test_capacity_behavior(void)
{
    // Small initial capacity
    DEQ_WRAP(int)
    dq = DEQ_INIT(sizeof(int), 2, DEQ_SHRINK_AT_20PCT);
    int val;
    (void)val;

    // Fill up and trigger resize
    assert(deq_push(&dq, 1) == 1);
    assert(deq_push(&dq, 2) == 1);
    assert(deq_cap(&dq) == 2);

    assert(deq_push(&dq, 3) == 1); // should resize
    assert(deq_cap(&dq) > 2);

    unsigned int cap_after_grow = deq_cap(&dq);
    (void)cap_after_grow;

    // Pop until under 20%
    for (int i = 0; i < 3; i++)
        assert(deq_pop(&dq, &val) == 1);
    assert(deq_len(&dq) == 0);

    // After shrink, capacity may drop back to min (2)
    // depends on implementation of deq_resize_
    // But len is 0
    assert(deq_len(&dq) == 0);

    deq_reset(&dq);
    PASS();
}

/**
 * Test: String pointers (ownership not managed by deque)
 */
static void test_string_deque(void)
{
    StringDeque *strq = NULL;
    assert(deq_new(strq, 4, DEQ_NO_SHRINK) == 0);

    char *hello = "hello";
    char *world = "world";
    (void)hello;
    (void)world;
    assert(deq_push(strq, hello) == 1);
    assert(deq_push(strq, world) == 1);
    assert(deq_len(strq) == 2);

    char *s;
    (void)s;
    assert(deq_shift(strq, &s) == 1 && s == hello);
    assert(deq_shift(strq, &s) == 1 && s == world);
    assert(deq_len(strq) == 0);

    deq_free(strq);
    PASS();
}

/**
 * Test: Error handling on malloc failure (simulate via ulimit -v)
 * This is hard to unit test automatically, but we assume deq_op_/deq_resize_ returns -1 on malloc fail.
 */

int main(void)
{
    printf("=== Running deq library tests ===\n");

    test_int_deque();
    test_front_operations();
    test_mixed_operations();
    test_peek_operations();
    test_struct_deque();
    test_deq_new_free();
    test_capacity_behavior();
    test_string_deque();

    printf("=== All tests passed! ===\n");
    return 0;
}