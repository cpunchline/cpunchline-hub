#include <stdio.h>
#include <stdlib.h>

#include "dsa/heap.h"

// Data node containing heap_node and actual value
typedef struct _test_node_t
{
    struct heap_node node; // Must be the first member
    int value;
} test_node_t;

// Comparison function: for min heap
static int less_than(const struct heap_node *lhs, const struct heap_node *rhs)
{
    test_node_t *l = (test_node_t *)lhs;
    test_node_t *r = (test_node_t *)rhs;
    return l->value < r->value;
}

// Comparison function: for max heap
static int larger_than(const struct heap_node *lhs, const struct heap_node *rhs)
{
    test_node_t *l = (test_node_t *)lhs;
    test_node_t *r = (test_node_t *)rhs;
    return l->value > r->value;
}

// Convert heap_node pointer to test_node_t pointer
#define my_node_entry(ptr) container_of(ptr, test_node_t, node)
#ifndef container_of
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
#include <stddef.h>
#endif

// Inorder traversal print (for observing tree structure)
static void print_inorder(struct heap_node *root)
{
    if (!root)
        return;
    print_inorder(root->left);
    test_node_t *data = my_node_entry(root);
    printf("%d ", data->value);
    print_inorder(root->right);
}

// Validate heap property recursively
static int validate_heap(struct heap_node *node, heap_compare_fn compare, int is_min_heap)
{
    if (!node)
        return 1;
    if (node->left)
    {
        int cmp = compare(node, node->left);
        if (is_min_heap && cmp == 0)
            return 0; // parent <= left
        if (!is_min_heap && cmp == 0)
            return 0; // parent >= left
        if (!validate_heap(node->left, compare, is_min_heap))
            return 0;
    }
    if (node->right)
    {
        int cmp = compare(node, node->right);
        if (is_min_heap && cmp == 0)
            return 0; // parent <= right
        if (!is_min_heap && cmp == 0)
            return 0; // parent >= right
        if (!validate_heap(node->right, compare, is_min_heap))
            return 0;
    }
    return 1;
}

// Create a new node
static test_node_t *new_node(int value)
{
    test_node_t *n = (test_node_t *)malloc(sizeof(test_node_t));
    if (!n)
        return NULL;
    n->node.parent = n->node.left = n->node.right = NULL;
    n->value = value;
    return n;
}

// Free all nodes
static void free_heap(struct heap *heap)
{
    while (heap->root)
    {
        test_node_t *n = my_node_entry(heap->root);
        heap_dequeue(heap);
        free(n);
    }
}

int main(void)
{
    struct heap heap;

    printf("=== Testing Min Heap ===\n");
    heap_init(&heap, less_than);

    int values[] = {10, 5, 20, 3, 7, 15, 30};
    int n = sizeof(values) / sizeof(values[0]);

    printf("Inserting: ");
    for (int i = 0; i < n; ++i)
    {
        test_node_t *node = new_node(values[i]);
        printf("%d ", node->value);
        heap_insert(&heap, &node->node);
    }
    printf("\n");

    printf("Inorder traversal: ");
    print_inorder(heap.root);
    printf("\n");

    printf("Validate min heap property: %s\n",
           validate_heap(heap.root, heap.compare, 1) ? "PASS" : "FAIL");

    printf("Dequeue order: ");
    while (heap.root)
    {
        test_node_t *top = my_node_entry(heap.root);
        printf("%d ", top->value);
        heap_dequeue(&heap);
    }
    printf("\n");

    printf("\n=== Testing Max Heap ===\n");
    heap_init(&heap, larger_than);

    printf("Inserting: ");
    for (int i = 0; i < n; ++i)
    {
        test_node_t *node = new_node(values[i]);
        printf("%d ", node->value);
        heap_insert(&heap, &node->node);
    }
    printf("\n");

    printf("Inorder traversal: ");
    print_inorder(heap.root);
    printf("\n");

    printf("Validate max heap property: %s\n",
           validate_heap(heap.root, heap.compare, 0) ? "PASS" : "FAIL");

    printf("Dequeue order: ");
    while (heap.root)
    {
        test_node_t *top = my_node_entry(heap.root);
        printf("%d ", top->value);
        heap_dequeue(&heap);
    }
    printf("\n");

    printf("\n=== Testing Remove Intermediate Node ===\n");
    heap_init(&heap, less_than);
    int small_vals[] = {1, 2, 3, 4, 5, 6};
    test_node_t *nodes[6];
    for (int i = 0; i < 6; ++i)
    {
        nodes[i] = new_node(small_vals[i]);
        heap_insert(&heap, &nodes[i]->node);
    }

    printf("Inserted: 1 2 3 4 5 6\n");
    printf("Removing node with value 3\n");
    heap_remove(&heap, &nodes[2]->node);
    free(nodes[2]);

    printf("Dequeue order: ");
    while (heap.root)
    {
        test_node_t *top = my_node_entry(heap.root);
        printf("%d ", top->value);
        heap_dequeue(&heap);
    }
    printf("\n");

    free_heap(&heap);

    printf("All tests completed.\n");
    return 0;
}
