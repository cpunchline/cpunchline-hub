#include <stdio.h>
#include <string.h>

#include "dsa/xstring.h"

// 简化断言输出
#define TEST_ASSERT(cond, msg)                                                     \
    do                                                                             \
    {                                                                              \
        if (!(cond))                                                               \
        {                                                                          \
            fprintf(stderr, "[FAIL] %s (line %d): %s\n", __FILE__, __LINE__, msg); \
            return 1;                                                              \
        }                                                                          \
    } while (0)

#define TEST_OK(msg) \
    printf("[PASS] %s\n", msg)

static int test_basic_init(void)
{
    char buf[16];
    xstring xs;

    // 测试 xstrInit:清空初始化
    xstrInit(&xs, buf, sizeof(buf), 0);
    TEST_ASSERT(xs.len == 0, "len should be 0 after init with keep=0");
    TEST_ASSERT(xs.cap == 16, "cap should be 16");
    TEST_ASSERT(xs.truncated == 0, "truncated should be 0");
    TEST_ASSERT(strcmp(xs.str, "") == 0, "str should be empty string");
    TEST_OK("test_basic_init - clear init");

    // 测试 xstrInit:保留内容
    strcpy(buf, "hello");
    xstrInit(&xs, buf, sizeof(buf), 1);
    TEST_ASSERT(xs.len == 5, "len should be 5 after init with keep=1");
    TEST_ASSERT(strcmp(xs.str, "hello") == 0, "str should be 'hello'");
    TEST_ASSERT(xs.truncated == 0, "truncated should still be 0");
    TEST_OK("test_basic_init - keep content");

    // 测试刚好填满的情况(会被截断标记)
    memset(buf, 'x', 15);
    buf[15] = '\0';
    xstrInit(&xs, buf, 16, 1);
    TEST_ASSERT(xs.len == 15, "len should be 15");
    TEST_ASSERT(xs.truncated == 0, "should not be truncated if null-terminated");

    memset(buf, 'y', 16); // 没有 \0
    xstrInit(&xs, buf, 16, 1);
    TEST_ASSERT(xs.len == 15, "len should be capped at cap-1 when no null");
    TEST_ASSERT(xs.truncated == -1, "should be marked truncated");
    TEST_ASSERT(buf[15] == '\0', "null terminator should be enforced");
    TEST_OK("test_basic_init - truncation on no null");

    return 0;
}

static int test_xstrNew_free(void)
{
    xstring *x = xstrNew(32);
    TEST_ASSERT(x != NULL, "xstrNew should return non-NULL for valid size");
    TEST_ASSERT(x->str != NULL, "x->str should be allocated");
    TEST_ASSERT(x->cap == 32, "allocated cap should be 32");
    TEST_ASSERT(x->len == 0, "new string should have len 0");
    TEST_ASSERT(x->truncated == 0, "new string should not be truncated");
    TEST_ASSERT(strcmp(x->str, "") == 0, "new string should be empty");

    xstrAdd(x, "hello");
    TEST_ASSERT(strcmp(x->str, "hello") == 0, "should contain 'hello'");
    TEST_ASSERT(x->len == 5, "len should be 5");

    xstrFree(x);
    TEST_OK("test_xstrNew_free");
    return 0;
}

static int test_append_ops(void)
{
    char buf[64];
    xstring xs;
    xstrInit(&xs, buf, sizeof(buf), 0);

    // xstrAddChar
    xstrAddChar(&xs, 'H');
    xstrAddChar(&xs, 'i');
    TEST_ASSERT(strcmp(xs.str, "Hi") == 0, "xstrAddChar failed");
    TEST_ASSERT(xs.len == 2, "xstrAddChar len failed");
    TEST_OK("xstrAddChar");

    // xstrAdd
    xstrAdd(&xs, " there");
    TEST_ASSERT(strcmp(xs.str, "Hi there") == 0, "xstrAdd failed");
    TEST_ASSERT(xs.len == 8, "xstrAdd len failed");
    TEST_OK("xstrAdd");

    // xstrAddSub
    xstrAddSub(&xs, " - partial", 4);
    TEST_ASSERT(strcmp(xs.str, "Hi there - p") == 0, "xstrAddSub failed");
    TEST_ASSERT(xs.len == 12, "xstrAddSub len failed");
    TEST_OK("xstrAddSub");

    // xstrCat
    xstrClear(&xs);
    xstrCat(&xs, "Hello", " ", "World", NULL);
    TEST_ASSERT(strcmp(xs.str, "Hello World") == 0, "xstrCat failed");
    TEST_ASSERT(xs.len == 11, "xstrCat len failed");
    TEST_OK("xstrCat");

    // xstrJoin
    xstrClear(&xs);
    xstrJoin(&xs, ", ", "apple", "banana", "cherry", NULL);
    TEST_ASSERT(strcmp(xs.str, "apple, banana, cherry") == 0, "xstrJoin failed");
    TEST_ASSERT(xs.len == 21, "xstrJoin len failed");
    TEST_OK("xstrJoin");

    // xstrAddSubs
    xstrClear(&xs);
    xstrAddSubs(&xs, "abc", 1, "def", 2, "ghi", 3, NULL);
    TEST_ASSERT(strcmp(xs.str, "adeghi") == 0, "xstrAddSubs failed");
    TEST_ASSERT(xs.len == 6, "xstrAddSubs len failed");
    TEST_OK("xstrAddSubs");

    return 0;
}

static int test_transactional_ops(void)
{
    char buf[10];
    xstring xs;
    xstrInit(&xs, buf, sizeof(buf), 0);

    // 成功的事务
    xstrCatT(&xs, "Hi", NULL);
    TEST_ASSERT(strcmp(xs.str, "Hi") == 0, "xstrCatT success failed");
    TEST_OK("xstrCatT - success");

    // 失败的事务:应被回滚
    int ret = xstrCatT(&xs, " - this is too long", NULL);
    TEST_ASSERT(ret == -1, "xstrCatT should return -1 on truncation");
    TEST_ASSERT(strcmp(xs.str, "Hi") == 0, "xstrCatT should rollback on truncation");
    TEST_ASSERT(xs.len == 2, "xstrCatT should restore len on rollback");
    TEST_OK("xstrCatT - rollback on truncation");

    // xstrJoinT
    xstrClear(&xs);
    xstrJoinT(&xs, ":", "a", "b", "c", NULL);
    TEST_ASSERT(strcmp(xs.str, "a:b:c") == 0, "xstrJoinT success failed");
    TEST_ASSERT(xs.len == 5, "xstrJoinT len failed");

    xstrJoinT(&xs, ":", "very", "long", "string", "here", NULL);
    TEST_ASSERT(strcmp(xs.str, "a:b:c") == 0, "xstrJoinT should not modify on truncation");
    TEST_OK("xstrJoinT - rollback");

    return 0;
}

static int test_comparison(void)
{
    char buf1[20], buf2[20];
    xstring x, y;

    xstrInit(&x, buf1, sizeof(buf1), 0);
    xstrInit(&y, buf2, sizeof(buf2), 0);

    xstrAdd(&x, "hello world");
    xstrAdd(&y, "hello");

    // xstrContains3: anywhere
    TEST_ASSERT(xstrContains3(&x, &y, 0) == 1, "x should contain y (anywhere)");
    TEST_ASSERT(xstrContains(&x, &y, 0) == 1, "xstrContains macro failed");

    // begins with
    xstrClear(&x);
    xstrAdd(&x, "hello there");
    TEST_ASSERT(xstrContains3(&x, &y, 1) == 1, "x should begin with y");
    TEST_ASSERT(xstrContains(&x, &y, 1) == 1, "xstrContains (begin) failed");

    // ends with
    xstrClear(&x);
    xstrClear(&y);
    xstrAdd(&x, "say hello");
    xstrAdd(&y, "hello");
    TEST_ASSERT(xstrContains3(&x, &y, -1) == 1, "x should end with y");
    TEST_ASSERT(xstrContains(&x, &y, -1) == 1, "xstrContains (end) failed");

    // not contains
    xstrClear(&y);
    xstrAdd(&y, "xyz");
    TEST_ASSERT(xstrContains3(&x, &y, 0) == 0, "x should not contain y");
    TEST_ASSERT(xstrContains(&x, &y, 0) == 0, "xstrContains (not contains) failed");

    // xstrEq3
    xstrClear(&x);
    xstrClear(&y);
    xstrAdd(&x, "test");
    xstrAdd(&y, "test");
    TEST_ASSERT(xstrEq3(&x, &y) == 1, "xstrEq3: equal strings should return 1");
    TEST_ASSERT(xstrEq(&x, &y) == 1, "xstrEq: equal strings should return 1");

    xstrAdd(&y, "x");
    TEST_ASSERT(xstrEq3(&x, &y) == 0, "xstrEq3: unequal should return 0");
    TEST_ASSERT(xstrEq(&x, &y) == 0, "xstrEq: unequal should return 0");

    // truncated → unknown
    xstrClear(&x);
    xstrAdd(&x, "this is way too long for the buffer and will truncate");
    TEST_ASSERT(x.truncated == -1, "x should be truncated");
    TEST_ASSERT(xstrEq3(&x, &y) == -1, "xstrEq3: truncated should return unknown");
    TEST_ASSERT(xstrContains3(&x, &y, 0) == -1, "xstrContains3: truncated x should return unknown");

    TEST_OK("test_comparison");
    return 0;
}

int main(void)
{
    printf("=== Running xstring Tests ===\n");

    if (test_basic_init())
        return 1;
    if (test_xstrNew_free())
        return 1;
    if (test_append_ops())
        return 1;
    if (test_transactional_ops())
        return 1;
    if (test_comparison())
        return 1;

    printf("=== All tests passed! ===\n");
    return 0;
}