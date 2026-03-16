#include <stdio.h>
#include <stdlib.h>

#include "dsa/lstack.h"

struct stack_node
{
    int data;
    struct stack_node *next;
};

int main(void)
{
    printf("=== LStack Test ===\n");

    struct stack_node *stack = NULL;

    printf("Stack empty: %s\n", lstack_empty(stack) ? "yes" : "no");

    // Push some elements
    printf("\nPushing elements 1, 2, 3, 4, 5:\n");
    for (int i = 1; i <= 5; i++)
    {
        struct stack_node *node = malloc(sizeof(*node));
        node->data = i;
        lstack_push(stack, node);
        printf("  Pushed: %d, top is now: %d\n", i, lstack_top(stack)->data);
    }

    // Check size
    int count;
    struct stack_node *tmp;
    lstack_size(stack, tmp, count);
    printf("\nStack size: %d\n", count);

    // Peek at top
    printf("Top element: %d\n", lstack_top(stack)->data);

    // Pop some elements
    printf("\nPopping 3 elements:\n");
    for (int i = 0; i < 3; i++)
    {
        struct stack_node *node;
        lstack_pop(stack, node);
        printf("  Popped: %d\n", node->data);
        free(node);
    }

    lstack_size(stack, tmp, count);
    printf("\nStack size after popping: %d\n", count);
    printf("Top element: %d\n", lstack_top(stack)->data);

    // Pop remaining elements
    printf("\nPopping remaining elements:\n");
    while (!lstack_empty(stack))
    {
        struct stack_node *node;
        lstack_pop(stack, node);
        printf("  Popped: %d\n", node->data);
        free(node);
    }

    printf("\nStack empty: %s\n", lstack_empty(stack) ? "yes" : "no");

    // Test with string data
    printf("\n=== LStack with String Data ===\n");

    struct string_node
    {
        const char *name;
        struct string_node *next;
    };

    struct string_node *str_stack = NULL;

    const char *names[] = {"Alice", "Bob", "Charlie", "David", "Eve"};

    printf("Pushing strings:\n");
    for (int i = 0; i < 5; i++)
    {
        struct string_node *node = malloc(sizeof(*node));
        node->name = names[i];
        lstack_push(str_stack, node);
        printf("  Pushed: %s\n", node->name);
    }

    struct string_node *str_tmp;
    lstack_size(str_stack, str_tmp, count);
    printf("\nString stack size: %d\n", count);

    printf("Popping all strings:\n");
    while (!lstack_empty(str_stack))
    {
        struct string_node *node;
        lstack_pop(str_stack, node);
        printf("  Popped: %s\n", node->name);
        free(node);
    }

    printf("\nString stack empty: %s\n", lstack_empty(str_stack) ? "yes" : "no");

    return 0;
}
