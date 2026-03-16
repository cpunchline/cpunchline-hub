#include <stdio.h>
#include <stdlib.h>
#include "dsa/list.h"
#include "dsa/safe_list.h"
#include "dsa/vlist.h"

// 测试节点结构
struct test_node
{
    int data;
    struct list_head list;
};

// 创建节点(简化辅助)
static struct test_node *new_node(int data)
{
    struct test_node *node = malloc(sizeof(*node));
    node->data = data;
    INIT_LIST_HEAD(&node->list);
    return node;
}

// 打印整个链表
static void print_list(struct list_head *head)
{
    printf("list: ");
    struct test_node *pos;
    list_for_each_entry(pos, head, list)
    {
        printf("%d ", pos->data);
    }
    printf("\n");
}

// ===== Safe List Test =====

struct safe_test_node
{
    int data;
    struct safe_list list;
};

static int safe_list_cb(void *ctx, struct safe_list *list)
{
    int *sum = (int *)ctx;
    struct safe_test_node *node = container_of(list, struct safe_test_node, list);
    printf("  Visiting node: %d\n", node->data);
    *sum += node->data;

    // Delete node with data=2 during traversal
    if (node->data == 2)
    {
        printf("  Deleting node 2 during traversal\n");
        safe_list_del(&node->list);
        free(node);
    }
    return 0;
}

static void test_safe_list(void)
{
    printf("\n=== Safe List Test ===\n");

    SAFE_LIST(head);

    // Create and add nodes
    struct safe_test_node *n1 = malloc(sizeof(*n1));
    n1->data = 1;
    safe_list_add(&n1->list, &head);

    struct safe_test_node *n2 = malloc(sizeof(*n2));
    n2->data = 2;
    safe_list_add(&n2->list, &head);

    struct safe_test_node *n3 = malloc(sizeof(*n3));
    n3->data = 3;
    safe_list_add(&n3->list, &head);

    struct safe_test_node *n4 = malloc(sizeof(*n4));
    n4->data = 4;
    safe_list_add_first(&n4->list, &head);

    printf("List empty: %s\n", safe_list_empty(&head) ? "yes" : "no");

    // Safe traversal with deletion
    printf("Safe traversal (will delete node 2):\n");
    int sum = 0;
    safe_list_for_each(&head, safe_list_cb, &sum);
    printf("Sum of visited nodes: %d\n", sum);

    // Clean up remaining nodes
    printf("Cleaning up remaining nodes...\n");
    struct safe_test_node *node;
    struct safe_list *cur, *tmp_list;
    list_for_each_entry_safe(cur, tmp_list, &head.list, list)
    {
        node = container_of(cur, struct safe_test_node, list);
        printf("  Freeing node: %d\n", node->data);
        safe_list_del(&node->list);
        free(node);
    }

    printf("List empty: %s\n", safe_list_empty(&head) ? "yes" : "no");
}

// ===== VList Test =====

struct vlist_test_node
{
    struct vlist_node vlist;
    int key;
    char name[32];
};

static int vlist_comp_int(const void *k1, const void *k2, void *ptr)
{
    (void)ptr;
    const int *key1 = k1;
    const int *key2 = k2;
    return (*key1 < *key2) ? -1 : ((*key1 > *key2) ? 1 : 0);
}

static void my_vlist_update_cb(struct vlist_tree *tree,
                               struct vlist_node *node_new,
                               struct vlist_node *node_old)
{
    (void)tree;

    if (node_new && node_old)
    {
        struct vlist_test_node *n_new = container_of(node_new, struct vlist_test_node, vlist);
        struct vlist_test_node *n_old = container_of(node_old, struct vlist_test_node, vlist);
        printf("  Update: key=%d, old_name=%s, new_name=%s\n",
               n_new->key, n_old->name, n_new->name);
    }
    else if (node_new)
    {
        struct vlist_test_node *n = container_of(node_new, struct vlist_test_node, vlist);
        printf("  Add: key=%d, name=%s\n", n->key, n->name);
    }
    else if (node_old)
    {
        struct vlist_test_node *n = container_of(node_old, struct vlist_test_node, vlist);
        printf("  Delete: key=%d, name=%s\n", n->key, n->name);
        free(n);
    }
}

static void test_vlist(void)
{
    printf("\n=== VList Test ===\n");

    struct vlist_tree tree;
    vlist_init(&tree, vlist_comp_int, my_vlist_update_cb);

    // First version - add some nodes
    printf("\nVersion 1 - Adding nodes:\n");
    vlist_update(&tree);

    struct vlist_test_node *n1 = malloc(sizeof(*n1));
    n1->key = 1;
    snprintf(n1->name, sizeof(n1->name), "node_1_v1");
    vlist_add(&tree, &n1->vlist, &n1->key);

    struct vlist_test_node *n2 = malloc(sizeof(*n2));
    n2->key = 2;
    snprintf(n2->name, sizeof(n2->name), "node_2_v1");
    vlist_add(&tree, &n2->vlist, &n2->key);

    struct vlist_test_node *n3 = malloc(sizeof(*n3));
    n3->key = 3;
    snprintf(n3->name, sizeof(n3->name), "node_3_v1");
    vlist_add(&tree, &n3->vlist, &n3->key);

    vlist_flush(&tree);

    // Second version - update node 2 and add node 4
    printf("\nVersion 2 - Update node 2, add node 4:\n");
    vlist_update(&tree);

    struct vlist_test_node *n2_v2 = malloc(sizeof(*n2_v2));
    n2_v2->key = 2;
    snprintf(n2_v2->name, sizeof(n2_v2->name), "node_2_v2");
    vlist_add(&tree, &n2_v2->vlist, &n2_v2->key);

    struct vlist_test_node *n4 = malloc(sizeof(*n4));
    n4->key = 4;
    snprintf(n4->name, sizeof(n4->name), "node_4_v2");
    vlist_add(&tree, &n4->vlist, &n4->key);

    vlist_flush(&tree);

    // List all nodes
    printf("\nCurrent nodes:\n");
    struct vlist_test_node *node;
    vlist_for_each_element(&tree, node, vlist)
    {
        printf("  key=%d, name=%s\n", node->key, node->name);
    }

    // Find a node
    int search_key = 2;
    node = vlist_find(&tree, &search_key, node, vlist);
    if (node)
    {
        printf("\nFound node with key=2: name=%s\n", node->name);
    }

    // Clean up all
    printf("\nCleaning up all nodes:\n");
    vlist_flush_all(&tree);
}

int main(void)
{
    // 定义并初始化链表头
    LIST_HEAD(mylist);

    printf("=== Simple Linux Kernel List Test ===\n");

    // 1. 插入几个节点(头插)
    struct test_node *n1 = new_node(1);
    struct test_node *n2 = new_node(2);
    struct test_node *n3 = new_node(3);

    list_add_tail(&n1->list, &mylist); // 尾插 1
    list_add_tail(&n2->list, &mylist); // 尾插 2
    list_add(&n3->list, &mylist);      // 头插 3

    printf("After insert 1,2,3: ");
    print_list(&mylist); // 输出: 3 1 2

    // 2. 遍历并查找
    printf("Traversal: ");
    struct test_node *pos;
    list_for_each_entry(pos, &mylist, list)
    {
        printf("%d ", pos->data);
    }
    printf("\n");

    // 3. 删除节点 n1 (data=1)
    printf("Deleting node with data=1...\n");
    list_del(&n1->list);
    free(n1);
    printf("After delete 1: ");
    print_list(&mylist); // 输出: 3 2

    // 4. 替换 n2 为新节点 n4
    struct test_node *n4 = new_node(4);
    printf("Replacing node 2 with 4...\n");
    list_replace(&n2->list, &n4->list);
    list_del(&n2->list);
    free(n2);
    printf("After replace: ");
    print_list(&mylist); // 输出: 3 4

    // 5. 再插入一个尾部节点
    struct test_node *n5 = new_node(5);
    list_add_tail(&n5->list, &mylist);
    printf("After add tail 5: ");
    print_list(&mylist); // 输出: 3 4 5

    // 6. 最后清空释放
    printf("Cleaning up...\n");
    struct test_node *tmp;
    list_for_each_entry_safe(pos, tmp, &mylist, list)
    {
        list_del(&pos->list);
        free(pos);
    }

    printf("All done. List is empty: %s\n", list_empty(&mylist) ? "yes" : "no");

    // Test safe_list
    test_safe_list();

    // Test vlist
    test_vlist();

    return 0;
}