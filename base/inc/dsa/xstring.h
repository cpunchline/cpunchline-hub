#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

/**
 * struct xstring - string metadata
 * @str: pointer to buf
 * @len: current length of buf contents
 * @cap: maximum capacity of buf
 * @truncated: -1 indicates truncation
 */
typedef struct xstring
{
    char *str;
    size_t len;
    size_t cap;
    int truncated;
} xstring;

/**
 * xstrInit - initialize an xstring struct
 * @x: pointer to xstring
 * @str: buffer to manage
 * @size: size of str
 * @keep: if !0, keep existing contents of str;
 *        otherwise, str[0] = '\0'
 *
 * Return: x->truncated
 */
static inline int xstrInit(xstring *x, char *str, const size_t size, int keep)
{
    assert(x && str && size > 0);
    memset(x, 0, sizeof(*x));

    x->str = str;
    x->cap = size;

    if (keep)
    {
        x->len = strnlen(str, x->cap);
        if (x->cap == x->len)
        {
            x->truncated = -1;
            x->len--;
        }
    }

    *(x->str + x->len) = '\0';

    return x->truncated;
}

/**
 * xstrNew - mallocs and inits
 * @size: size of buffer to allocate
 *
 * mallocs both buffer and struct, calls xstrInit
 *
 * Return: ptr to xstring or NULL
 */
static inline xstring *xstrNew(const size_t size)
{
    char *str;
    xstring *x;

    if (size < 1)
    {
        errno = EINVAL;
        return NULL;
    }

    str = malloc(size);
    if (!str)
        return NULL;

    x = malloc(sizeof(struct xstring));
    if (!x)
    {
        free(str);
        return NULL;
    }

    xstrInit(x, str, size, 0);
    return x;
}

/**
 * xstrClear - clear xstring
 * @x: pointer to xstring
 *
 * This sets x->len and x->truncated to 0 and str[0] to '\0';
 *
 * Return: x->truncated (always 0)
 */
#define xstrClear(x) (xstrInit((x), (x)->str, (x)->cap, 0))

/**
 * xstrAddChar - add a single character
 * @x: pointer to xstring
 * @c: character to append
 *
 * Return: x->truncated
 */
static inline int xstrAddChar(xstring *x, const char c)
{
    assert(x);

    if (x->truncated || c == '\0')
        return x->truncated;

    if (x->len + 1 < x->cap)
    {
        char *p = x->str + x->len;
        *p++ = c;
        *p = '\0';
        x->len++;
    }
    else
        x->truncated = -1;

    return x->truncated;
}

/**
 * xstrAdd - append a string
 * @x: pointer to xstring
 * @src: string to append
 *
 * Append as much from src as fits - if not all, flag truncation.
 *
 * Return: x->truncated
 */
static inline int xstrAdd(xstring *x, const char *src)
{
    char *p, *q, *s;

    assert(x);

    if (x->truncated || !src || *src == '\0')
        return x->truncated;

    for (s = (char *)src,
        p = x->str + x->len,
        q = x->str + x->cap - 1;
         *s != '\0' && p < q; p++, s++)
    {
        *p = *s;
    }

    *p = '\0';
    x->len = (size_t)(p - x->str);

    if (*s != '\0')
        x->truncated = -1;

    return x->truncated;
}

/**
 * xstrAddSub - append a substring
 * @x: pointer to xstring
 * @src: string to append
 * @len: length of substring
 *
 * Append substring and '\0' if len fits, otherwise flag truncation.
 *
 * Return: x->truncated
 */
static inline int xstrAddSub(xstring *x, const char *src, size_t len)
{
    assert(x);

    if (x->truncated || !src || len == 0)
        return x->truncated;

    if (x->len + len + 1 > x->cap)
        return x->truncated = -1;

    memcpy(x->str + x->len, src, len);

    x->len += len;
    *(x->str + x->len) = '\0';

    return x->truncated;
}

/** xstrCat - append multiple strings
 * @x: pointer to xstring
 * @...: one or more strings followed by NULL
 *
 * Run xstrAdd for each string.
 *
 * Return: x->truncated
 */
static inline int xstrCat(xstring *x, ...)
{
    va_list ap;
    char *s;

    assert(x);
    va_start(ap, x);
    while ((s = va_arg(ap, char *)) != NULL)
    {
        if (xstrAdd(x, s) == -1)
            break;
    }
    va_end(ap);
    return x->truncated;
}

/** xstrJoin - append multiple strings joined by sep
 * @x: pointer to xstring
 * @sep: separator string
 * @...: one or more strings followed by NULL
 *
 * Run xstrAdd for each string and append sep between each pair.
 *
 * Return: x->truncated
 */
static inline int xstrJoin(xstring *x, const char *sep, ...)
{
    va_list ap;
    char *s;
    int i;

    assert(x && sep);
    va_start(ap, sep);
    for (i = 0; (s = va_arg(ap, char *)) != NULL; i++)
    {
        if (i && xstrAdd(x, sep) == -1)
            break;
        if (xstrAdd(x, s) == -1)
            break;
    }
    va_end(ap);
    return x->truncated;
}

/**
 * xstrAddSubs - append multiple substrings
 * @x: pointer to xstring
 * @...: one or more pairs of string and length followed by NULL
 *
 * Run xstrAddSub for each pair of string and length.
 *
 * Return: x->truncated
 */
static inline int xstrAddSubs(xstring *x, ...)
{
    va_list ap;
    char *s;

    assert(x);
    va_start(ap, x);
    while ((s = va_arg(ap, char *)) != NULL)
    {
        size_t n = va_arg(ap, size_t);
        if (xstrAddSub(x, s, n) == -1)
            break;
    }
    va_end(ap);
    return x->truncated;
}

#define transact(x, y) ({                   \
    size_t last;                            \
    int xstring_transact_ret;               \
    assert((x));                            \
    last = (x)->len;                        \
    if ((xstring_transact_ret = (y)) == -1) \
    {                                       \
        (x)->len = last;                    \
        *((x)->str + (x)->len) = '\0';      \
    }                                       \
    xstring_transact_ret;                   \
})

/**
 * xstrAddT - append a string as a transaction
 * @x: pointer to xstring
 * @src: string to append
 *
 * Run xstrAdd. Reterminate at initial length if truncation occurs.
 *
 * Return: x->truncated
 */
#define xstrAddT(x, y) transact(x, xstrAdd((x), y))

/** xstrCatT - append multiple strings as one transaction
 * @x: pointer to xstring
 * @...: one or more strings followed by NULL
 *
 * Run xstrCat. Reterminate at initial length if truncation occurs.
 *
 * Return: x->truncated
 */
#define xstrCatT(x, y...) transact(x, xstrCat((x), ##y))

/** xstrJoinT - append multiple strings joined by sep as one transaction
 * @x: pointer to xstring
 * @sep: separator string
 * @...: one or more strings followed by NULL
 *
 * Run xstrJoin. Reterminate at initial length if truncation occurs.
 *
 * Return: x->truncated
 */
#define xstrJoinT(x, y...) transact(x, xstrJoin((x), ##y))

/**
 * xstrAddSubsT - append multiple substrings as one transaction
 * @x: pointer to xstring
 * @...: one or more pairs of string and length followed by NULL
 *
 * Run xstrAddSubs. Reterminate at initial length if truncation occurs.
 *
 * Return: x->truncated
 */
#define xstrAddSubsT(x, y...) transact(x, xstrAddSubs((x), ##y))

/**
 * xstrAddSubT - same as xstrAddSub
 */
// addsub is already transactional
#define xstrAddSubT xstrAddSub

#define unknown -1

/**
 * xstrContains3 - test if first string contains second
 * @x: pointer to xstring
 * @y: pointer to xstring
 * @where: -1 (ends), 0 (anywhere), 1 (begins)
 *
 * If neither x nor y are truncated, returns true (1) or false (0).
 * If y is truncated, return unknown (-1).
 * If x is truncated, return true (1) if known portion of x contains y, unknown (-1) otherwise.
 *
 * Return: -1, 0, or 1
 */

/* Does the first string contain the second */
static inline int xstrContains3(xstring *x, xstring *y, int where)
{
    int b = 0;

    assert(x && y && where >= -1 && where <= 1);

    if (y->truncated)
        return unknown;

    if (x->len < y->len)
        return 0;

    switch (where)
    {
        case -1:
            b = strncmp(x->str + x->len - y->len, y->str, y->len) == 0;
            break;
        case 0:
            b = strstr(x->str, y->str) != NULL;
            break;
        case 1:
            b = strncmp(x->str, y->str, y->len) == 0;
            break;
        default:
            break;
    }

    return b ? 1 : x->truncated ? unknown
                                : 0;
}

/**
 * xstrEq3 - test if two strings are equal
 * @x: pointer to xstring
 * @y: pointer to xstring
 *
 * Return true (1) if the strings held by x and y match and no truncation has occurred.
 * Return unknown (-1) if either is flagged as truncated.
 * Return false (0) otherwise.
 *
 * Return: -1, 0, or 1
 */
static inline int xstrEq3(xstring *x, xstring *y)
{
    assert(x && y);

    if (x->truncated || y->truncated)
        return unknown;

    return x->len == y->len && xstrContains3(x, y, 1);
}

/**
 * xstrEq - test if two strings are equal
 * @x: pointer to xstring
 * @y: pointer to xstring
 *
 * Same as xstrEq3, but return false (0) for unknown (-1).
 *
 * Return: 0 or 1
 */
#define xstrEq(x, y) (xstrEq3((x), (y)) == 1)

/**
 * xstrContains - test if first string contains second
 * @x: pointer to xstring
 * @y: pointer to xstring
 * @where: -1 (ends), 0 (anywhere), 1 (begins)
 *
 * Same as xstrContains3, but return false (0) for unknown (-1).
 *
 * Return: 0 or 1
 */
#define xstrContains(x, y, w) (xstrContains3((x), (y), (w)) == 1)

/**
 * xstrFree - free malloced memory
 * @x: pointer to xstring
 */
static inline void xstrFree(xstring *x)
{
    assert(x);
    free(x->str);
    free(x);
}

#ifdef __cplusplus
}
#endif
