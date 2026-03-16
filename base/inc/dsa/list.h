#pragma once

// Linux Kernel Doubly-linked Circular List

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>

#ifndef container_of
#define container_of(ptr, type, member) \
    ((type *)((uintptr_t)(ptr) - offsetof(type, member)))
#endif

struct list_head
{
    struct list_head *next;
    struct list_head *prev;
};

// 初始化一个名为name的空双向循环链表
#define LIST_HEAD_INIT(name) \
    {                        \
        &(name), &(name)     \
    }

// 定义一个名为name的空双向循环链表
#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name)

// 初始化一个空双向循环链表
static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list->prev = list;
}

// 增加节点
// 中间插
static inline void __list_add(struct list_head *_new, struct list_head *prev, struct list_head *next)
{
    next->prev = _new;
    _new->next = next;
    _new->prev = prev;
    prev->next = _new;
}

// 头插(将new插入到head之后, 即head和head->next之间)
static inline void list_add(struct list_head *_new, struct list_head *head)
{
    __list_add(_new, head, head->next);
}

// 尾插(将new插入到head之前, 即head->prev和head之间)
static inline void list_add_tail(struct list_head *_new, struct list_head *head)
{
    __list_add(_new, head->prev, head);
}

// 删除节点(其参数包括待删除节点entry的前一个节点prev和后一个节点next)
static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

// 删除节点(其参数为待删除节点entry)
static inline void __list_del_entry(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
}

// 删除节点后置空
static inline void list_del(struct list_head *entry)
{
    __list_del_entry(entry);
    entry->next = entry->prev = NULL; // 确保已删除的节点不再指向任何的链表元素
}

// 删除节点后初始化
static inline void list_del_init(struct list_head *entry)
{
    __list_del_entry(entry);
    INIT_LIST_HEAD(entry); // 确保已删除的节点不再保留对链表的任何引用, 从而避免出现内存安全问题
}

// new替换old节点(不销毁old)
static inline void list_replace(struct list_head *_old, struct list_head *_new)
{
    _new->next = _old->next;
    _new->next->prev = _new;
    _new->prev = _old->prev;
    _new->prev->next = _new;
}

// new替换old节点(销毁old) 一般使用这个
static inline void list_replace_init(struct list_head *_old, struct list_head *_new)
{
    list_replace(_old, _new);
    INIT_LIST_HEAD(_old);
}

// 交换链表头
static inline void list_swap(struct list_head *entry1, struct list_head *entry2)
{
    struct list_head *pos = entry2->prev;

    list_del(entry2);
    list_replace(entry1, entry2);
    if (pos == entry1)
        pos = entry2;
    list_add(entry1, pos);
}

// 移动节点(从源链表删除list节点, 将其头插到head链表
static inline void list_move(struct list_head *list, struct list_head *head)
{
    __list_del_entry(list);
    list_add(list, head);
}

// 移动节点(从源链表删除list节点, 将其尾插到head链表
static inline void list_move_tail(struct list_head *list, struct list_head *head)
{
    __list_del_entry(list);
    list_add_tail(list, head);
}

// 追加链表, 将一个链表的[first, last]这段子链表插入到head链表的尾部
static inline void list_bulk_move_tail(struct list_head *head, struct list_head *first, struct list_head *last)
{
    // 从源链表摘除
    first->prev->next = last->next;
    last->next->prev = first->prev;

    // 插入新链表尾
    head->prev->next = first;
    first->prev = head->prev;

    last->next = head;
    head->prev = last;
}

// 判断一个节点是否是链表中的第一个节点
static inline int list_is_first(const struct list_head *list, const struct list_head *head)
{
    return list->prev == head;
}

// 判断一个节点是否是链表中的最后一个节点
static inline int list_is_last(const struct list_head *list, const struct list_head *head)
{
    return list->next == head;
}

// 判断一个节点是否是链表中的头节点
static inline int list_is_head(const struct list_head *list, const struct list_head *head)
{
    return list == head;
}

// 判断链表是否为空
static inline int list_empty(const struct list_head *head)
{
    return head->next == head;
}

// 旋转链表
static inline void list_rotate_left(struct list_head *head)
{
    struct list_head *first;

    if (!list_empty(head))
    {
        first = head->next;
        list_move_tail(first, head);
    }
}

// 链表是否只有一个节点
static inline int list_is_singular(const struct list_head *head)
{
    return !list_empty(head) && (head->next == head->prev);
}

// 分割(将链表由某个位置一分为二)
// 将原链表head从分割点entry一分为二, 原链表head存储(头~entry), 新链表list存储(entry->next~尾)
static inline void __list_cut_position(struct list_head *list,
                                       struct list_head *head, struct list_head *entry)
{
    struct list_head *new_first = entry->next;
    list->next = head->next;
    list->next->prev = list;
    list->prev = entry;
    entry->next = list;
    head->next = new_first;
    new_first->prev = head;
}

// 将指定节点之后的节点全部从链表中切断, 并移动到新的链表中.
static inline void list_cut_position(struct list_head *list,
                                     struct list_head *head, struct list_head *entry)
{
    if (list_empty(head))
        return;
    if (list_is_singular(head) && !list_is_head(entry, head) && (entry != head->next))
        return;
    if (list_is_head(entry, head))
        INIT_LIST_HEAD(list);
    else
        __list_cut_position(list, head, entry);
}

// 将指定节点之前的节点全部从链表中切断, 并移动到新的链表中.
static inline void list_cut_before(struct list_head *list, struct list_head *head, struct list_head *entry)
{
    if (head->next == entry)
    {
        INIT_LIST_HEAD(list);
        return;
    }
    list->next = head->next;
    list->next->prev = list;
    list->prev = entry->prev;
    list->prev->next = list;
    head->next = entry;
    entry->prev = head;
}

// 拼接
// 在一个链表的prev和next之间, 插入新list链表
static inline void __list_splice(const struct list_head *list, struct list_head *prev, struct list_head *next)
{
    struct list_head *first = list->next;
    struct list_head *last = list->prev;

    first->prev = prev;
    prev->next = first;

    last->next = next;
    next->prev = last;
}

// 将list链表插入head头
static inline void list_splice(const struct list_head *list, struct list_head *head)
{
    if (!list_empty(list))
        __list_splice(list, head, head->next);
}

// 将list链表插入head尾
static inline void list_splice_tail(struct list_head *list,
                                    struct list_head *head)
{
    if (!list_empty(list))
        __list_splice(list, head->prev, head);
}

// 将list链表插入head头, 并初始化list
static inline void list_splice_init(struct list_head *list, struct list_head *head)
{
    if (!list_empty(list))
    {
        __list_splice(list, head, head->next);
        INIT_LIST_HEAD(list);
    }
}

// 将list链表插入head尾, 并初始化list
static inline void list_splice_tail_init(struct list_head *list,
                                         struct list_head *head)
{
    if (!list_empty(list))
    {
        __list_splice(list, head->prev, head);
        INIT_LIST_HEAD(list);
    }
}

// 获取节点
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

// 获取首节点
#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)
// 获取首节点(判空)
#define list_first_entry_or_null(ptr, type, member) ({        \
    struct list_head *head__ = (ptr);                         \
    struct list_head *pos__ = head__->next;                   \
    pos__ != head__ ? list_entry(pos__, type, member) : NULL; \
})

// 获取尾节点
#define list_last_entry(ptr, type, member) \
    list_entry((ptr)->prev, type, member)

// 获取给定节点的下一个节点
#define list_next_entry(pos, member) \
    list_entry((pos)->member.next, typeof(*(pos)), member)

// 获取链表中给定节点的下一个节点, 支持循环遍历
#define list_next_entry_circular(pos, head, member) \
    (list_is_last(&(pos)->member, head) ? list_first_entry(head, typeof(*(pos)), member) : list_next_entry(pos, member))

// 获取给定节点的上一个节点
#define list_prev_entry(pos, member) \
    list_entry((pos)->member.prev, typeof(*(pos)), member)

// 获取链表中给定节点的上一个节点, 支持循环遍历
#define list_prev_entry_circular(pos, head, member) \
    (list_is_first(&(pos)->member, head) ? list_last_entry(head, typeof(*(pos)), member) : list_prev_entry(pos, member))

/**
 * @brief  遍历链表(正向和反向) 通常用于获取节点
 * @param  pos : 输出参数, 遍历得到的链表节点(struct list_head *类型指针)
 * @param  head: 输入参数, 链表头(struct list_head *类型指针)
 */
// 从头正向遍历
#define list_for_each(pos, head) \
    for (pos = (head)->next; !list_is_head(pos, (head)); pos = pos->next)

// 从头反向遍历
#define list_for_each_prev(pos, head) \
    for (pos = (head)->prev; !list_is_head(pos, (head)); pos = pos->prev)

// 从pos位置继续正向遍历
#define list_for_each_continue(pos, head) \
    for (pos = pos->next; !list_is_head(pos, (head)); pos = pos->next)
/**
 * @brief  遍历链表, 以防删除链表项(正向和反向) 通常用于删除节点
 * @param  pos : 输出参数, 遍历得到的链表节点(struct list_head *类型指针)
 * @param  n   : 输入参数, 临时变量(用户层定义结构体指针)
 * @param  head: 输入参数, 链表头(struct list_head *类型指针)
 */
#define list_for_each_safe(pos, n, head)    \
    for (pos = (head)->next, n = pos->next; \
         !list_is_head(pos, (head));        \
         pos = n, n = pos->next)

#define list_for_each_prev_safe(pos, n, head) \
    for (pos = (head)->prev, n = pos->prev;   \
         !list_is_head(pos, (head));          \
         pos = n, n = pos->prev)

// 统计链表节点个数
static inline size_t list_count_nodes(struct list_head *head)
{
    struct list_head *pos;
    size_t count = 0;

    list_for_each(pos, head)
        count++;

    return count;
}

// 给定的结点指针是否指向头节点
#define list_entry_is_head(pos, head, member) \
    (&pos->member == (head))

/**
 * @brief  遍历给定类型的链表(正向和反向) 通常用于获取节点
 * @param  pos   : 输出参数, 遍历得到的链表节点(用户层定义结构体指针)
 * @param  head  : 输入参数, 链表头(struct list_head *类型指针)
 * @param  member: 输入参数, 结构体中list_head的名称
 */

// 从头正向遍历
#define list_for_each_entry(pos, head, member)               \
    for (pos = list_first_entry(head, typeof(*pos), member); \
         !list_entry_is_head(pos, head, member);             \
         pos = list_next_entry(pos, member))

// 从头反向遍历
#define list_for_each_entry_reverse(pos, head, member)      \
    for (pos = list_last_entry(head, typeof(*pos), member); \
         !list_entry_is_head(pos, head, member);            \
         pos = list_prev_entry(pos, member))

#define list_prepare_entry(pos, head, member) \
    ((pos) ?: list_entry(head, typeof(*pos), member))

// 从pos位置继续正向遍历
#define list_for_each_entry_continue(pos, head, member) \
    for (pos = list_next_entry(pos, member);            \
         !list_entry_is_head(pos, head, member);        \
         pos = list_next_entry(pos, member))

// 从pos位置继续反向遍历
#define list_for_each_entry_continue_reverse(pos, head, member) \
    for (pos = list_prev_entry(pos, member);                    \
         !list_entry_is_head(pos, head, member);                \
         pos = list_prev_entry(pos, member))

// 从pos位置开始正向遍历
#define list_for_each_entry_from(pos, head, member) \
    for (; !list_entry_is_head(pos, head, member);  \
         pos = list_next_entry(pos, member))

// 从pos位置开始反向遍历
#define list_for_each_entry_from_reverse(pos, head, member) \
    for (; !list_entry_is_head(pos, head, member);          \
         pos = list_prev_entry(pos, member))

/**
 * @brief  遍历给定类型的链表(正向和反向), 以防删除链表项 通常用于删除节点
 * @param  pos   : 输出参数, 遍历得到的链表节点(用户层定义结构体指针)
 * @param  head  : 输入参数, 链表头(struct list_head *类型指针)
 * @param  member: 输入参数, 结构体中list_head的名称
 */
#define list_for_each_entry_safe(pos, n, head, member)       \
    for (pos = list_first_entry(head, typeof(*pos), member), \
        n = list_next_entry(pos, member);                    \
         !list_entry_is_head(pos, head, member);             \
         pos = n, n = list_next_entry(n, member))

#define list_for_each_entry_safe_continue(pos, n, head, member) \
    for (pos = list_next_entry(pos, member),                    \
        n = list_next_entry(pos, member);                       \
         !list_entry_is_head(pos, head, member);                \
         pos = n, n = list_next_entry(n, member))

#define list_for_each_entry_safe_from(pos, n, head, member) \
    for (n = list_next_entry(pos, member);                  \
         !list_entry_is_head(pos, head, member);            \
         pos = n, n = list_next_entry(n, member))

#define list_for_each_entry_safe_reverse(pos, n, head, member) \
    for (pos = list_last_entry(head, typeof(*pos), member),    \
        n = list_prev_entry(pos, member);                      \
         !list_entry_is_head(pos, head, member);               \
         pos = n, n = list_prev_entry(n, member))

// 用于安全访问指定节点的下一个元素
#define list_safe_reset_next(pos, n, member) \
    n = list_next_entry(pos, member)

#ifdef __cplusplus
}
#endif
