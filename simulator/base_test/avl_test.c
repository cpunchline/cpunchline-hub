#include <stdio.h>
#include <string.h>

#include "dsa/avl.h"

typedef int avl_key_type;
typedef int avl_val_type;

struct avl_entry
{
    struct avl_node avl_node;
    avl_key_type key;
    avl_val_type val;
};

static int avl_comp_int(const void *k1, const void *k2, void *ptr)
{
    (void)ptr;
    const avl_key_type *key1 = k1;
    const avl_key_type *key2 = k2;
    if (*key1 < *key2)
    {
        return -1;
    }
    else if (*key1 > *key2)
    {
        return 1;
    }
    return 0;
}

static int avl_insert_entry(struct avl_tree *tree, struct avl_entry *entry)
{
    printf("insert %d\n", entry->key);
    entry->avl_node.key = &entry->key;
    return avl_insert(tree, &entry->avl_node);
}

static void avl_remove_entry(struct avl_tree *tree, struct avl_entry *entry)
{
    printf("remove %d\n", entry->key);
    avl_delete(tree, &entry->avl_node);
}

static struct avl_entry *avl_search_entry(struct avl_tree *tree, const avl_key_type *key)
{
    struct avl_node *node = avl_find(tree, key);
    if (node == NULL)
    {
        return NULL;
    }
    return container_of(node, struct avl_entry, avl_node);
}

static void avl_entry_print(struct avl_entry *entry)
{
    if (entry == NULL)
    {
        printf("null\n");
        return;
    }
    printf("%d:%d\n", entry->key, entry->val);
}

int main(void)
{
    struct avl_tree tree;
    struct avl_entry *entry = NULL;

    // Initialize AVL tree, not allow duplicates
    avl_init(&tree, avl_comp_int, false, NULL);

    struct avl_entry entries[10];
    for (int i = 0; i < 10; ++i)
    {
        memset(&entries[i], 0, sizeof(struct avl_entry));
        entries[i].key = i;
        entries[i].val = i;
    }

    // Insert elements in non-sequential order
    avl_insert_entry(&tree, &entries[1]);
    avl_insert_entry(&tree, &entries[2]);
    avl_insert_entry(&tree, &entries[3]);
    avl_insert_entry(&tree, &entries[7]);
    avl_insert_entry(&tree, &entries[8]);
    avl_insert_entry(&tree, &entries[9]);
    avl_insert_entry(&tree, &entries[4]);
    avl_insert_entry(&tree, &entries[5]);
    avl_insert_entry(&tree, &entries[6]);

    printf("\nTree count: %u\n", tree.count);

    // Remove some elements
    avl_remove_entry(&tree, &entries[1]);
    avl_remove_entry(&tree, &entries[9]);
    avl_remove_entry(&tree, &entries[4]);
    avl_remove_entry(&tree, &entries[6]);

    printf("\nTree count after removal: %u\n", tree.count);

    // Search for existing element
    int key = 5;
    entry = avl_search_entry(&tree, &key);
    printf("\nSearch for key 5: ");
    avl_entry_print(entry);

    // Search for removed element
    key = 4;
    entry = avl_search_entry(&tree, &key);
    printf("Search for key 4: ");
    avl_entry_print(entry);

    // Test find_lessequal
    key = 6;
    struct avl_node *node = avl_find_lessequal(&tree, &key);
    if (node != NULL)
    {
        entry = container_of(node, struct avl_entry, avl_node);
        printf("\nFind lessequal to 6: ");
        avl_entry_print(entry);
    }

    // Test find_greaterequal
    key = 4;
    node = avl_find_greaterequal(&tree, &key);
    if (node != NULL)
    {
        entry = container_of(node, struct avl_entry, avl_node);
        printf("Find greaterequal to 4: ");
        avl_entry_print(entry);
    }

    // Iterate through all remaining elements
    printf("\nIterate through all elements:\n");
    if (!avl_is_empty(&tree))
    {
        avl_for_each_element(&tree, entry, avl_node)
        {
            avl_entry_print(entry);
        }
    }

    // Remove all remaining elements
    printf("\nRemoving all elements:\n");
    struct avl_entry *ptr;
    avl_remove_all_elements(&tree, entry, avl_node, ptr)
    {
        avl_entry_print(entry);
        memset(entry, 0, sizeof(struct avl_entry));
    }

    printf("\nTree count after clearing: %u\n", tree.count);
    printf("Tree is empty: %s\n", avl_is_empty(&tree) ? "true" : "false");

    return 0;
}
