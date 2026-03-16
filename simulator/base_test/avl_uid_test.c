#include <stdio.h>
#include <string.h>

#include "dsa/avl_uid.h"

struct test_uid_entry
{
    struct avl_uid uid;
    int data;
};

static void print_uid_entry(struct test_uid_entry *entry)
{
    if (entry == NULL)
    {
        printf("null\n");
        return;
    }
    printf("id: %u, data: %d\n", entry->uid.id, entry->data);
}

int main(void)
{
    struct avl_tree tree;
    struct test_uid_entry entries[10];
    struct test_uid_entry *entry = NULL;

    // Initialize AVL UID tree
    avl_init_id_tree(&tree);

    printf("=== Test AVL UID Tree ===\n\n");

    // Allocate IDs with specific values
    printf("Allocating IDs with specific values:\n");
    for (int i = 0; i < 5; ++i)
    {
        memset(&entries[i], 0, sizeof(struct test_uid_entry));
        entries[i].data = i * 10;
        uint32_t id_val = (uint32_t)(i + 1) * 100;
        if (avl_alloc_id(&tree, &entries[i].uid, id_val))
        {
            printf("Allocated ID %u for data %d\n", entries[i].uid.id, entries[i].data);
        }
        else
        {
            printf("Failed to allocate ID %u\n", id_val);
        }
    }

    printf("\nTree count: %u\n", tree.count);

    // Allocate IDs with random values
    printf("\nAllocating IDs with random values:\n");
    for (int i = 5; i < 10; ++i)
    {
        memset(&entries[i], 0, sizeof(struct test_uid_entry));
        entries[i].data = i * 10;
        if (avl_alloc_id(&tree, &entries[i].uid, 0))
        {
            printf("Allocated random ID %u for data %d\n", entries[i].uid.id, entries[i].data);
        }
        else
        {
            printf("Failed to allocate random ID\n");
        }
    }

    printf("\nTree count after random allocation: %u\n", tree.count);

    // Find elements by ID
    printf("\nSearching for specific IDs:\n");
    uint32_t search_id = 100;
    struct avl_uid *found = avl_find_id(&tree, search_id);
    if (found != NULL)
    {
        entry = container_of(found, struct test_uid_entry, uid);
        printf("Found ID %u: ", search_id);
        print_uid_entry(entry);
    }
    else
    {
        printf("ID %u not found\n", search_id);
    }

    search_id = 300;
    found = avl_find_id(&tree, search_id);
    if (found != NULL)
    {
        entry = container_of(found, struct test_uid_entry, uid);
        printf("Found ID %u: ", search_id);
        print_uid_entry(entry);
    }
    else
    {
        printf("ID %u not found\n", search_id);
    }

    // Iterate through all entries
    printf("\nIterating through all entries:\n");
    if (!avl_is_empty(&tree))
    {
        avl_for_each_element(&tree, found, avl)
        {
            entry = container_of(found, struct test_uid_entry, uid);
            print_uid_entry(entry);
        }
    }

    // Free some IDs
    printf("\nFreeing IDs 200 and 400:\n");
    avl_free_id(&tree, &entries[1].uid);
    printf("Freed ID 200\n");
    avl_free_id(&tree, &entries[3].uid);
    printf("Freed ID 400\n");

    printf("\nTree count after freeing: %u\n", tree.count);

    // Search for freed ID
    search_id = 200;
    found = avl_find_id(&tree, search_id);
    if (found != NULL)
    {
        printf("ID %u still found (unexpected!)\n", search_id);
    }
    else
    {
        printf("ID %u not found (expected after free)\n", search_id);
    }

    // Remove all remaining elements
    printf("\nRemoving all remaining elements:\n");
    struct avl_uid *ptr;
    avl_remove_all_elements(&tree, found, avl, ptr)
    {
        entry = container_of(found, struct test_uid_entry, uid);
        print_uid_entry(entry);
        memset(entry, 0, sizeof(struct test_uid_entry));
    }

    printf("\nTree count after clearing: %u\n", tree.count);
    printf("Tree is empty: %s\n", avl_is_empty(&tree) ? "true" : "false");

    // Test string tree
    printf("\n=== Test AVL String Tree ===\n\n");

    struct string_entry
    {
        struct avl_node avl_node;
        const char *str;
        int value;
    };

    struct avl_tree str_tree;
    avl_init_string_tree(&str_tree, false);

    struct string_entry str_entries[5] = {
        {.str = "hello", .value = 1},
        {.str = "world", .value = 2},
        {.str = "avl",   .value = 3},
        {.str = "tree",  .value = 4},
        {.str = "test",  .value = 5}
    };

    printf("Inserting strings:\n");
    for (int i = 0; i < 5; ++i)
    {
        str_entries[i].avl_node.key = str_entries[i].str;
        if (avl_insert(&str_tree, &str_entries[i].avl_node) == 0)
        {
            printf("Inserted: %s -> %d\n", str_entries[i].str, str_entries[i].value);
        }
    }

    printf("\nIterating through string tree:\n");
    struct string_entry *str_entry;
    if (!avl_is_empty(&str_tree))
    {
        avl_for_each_element(&str_tree, str_entry, avl_node)
        {
            printf("%s -> %d\n", str_entry->str, str_entry->value);
        }
    }

    // Search for a string
    const char *search_str = "tree";
    struct avl_node *str_node = avl_find(&str_tree, search_str);
    if (str_node != NULL)
    {
        str_entry = container_of(str_node, struct string_entry, avl_node);
        printf("\nFound string '%s': value = %d\n", search_str, str_entry->value);
    }

    printf("\nString tree count: %u\n", str_tree.count);

    return 0;
}
