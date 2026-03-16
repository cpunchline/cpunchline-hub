#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include "dsa/avl.h"

// AVL Tree Based on Unique ID

struct avl_uid
{
    struct avl_node avl;
    uint32_t id;
};

void avl_init_id_tree(struct avl_tree *tree);
void avl_init_string_tree(struct avl_tree *tree, bool dup);
bool avl_alloc_id(struct avl_tree *tree, struct avl_uid *id, uint32_t val);

static inline void avl_free_id(struct avl_tree *tree, struct avl_uid *id)
{
    avl_delete(tree, &id->avl);
}

static inline struct avl_uid *avl_find_id(struct avl_tree *tree, uint32_t id)
{
    struct avl_node *avl;

    avl = avl_find(tree, &id);
    if (!avl)
        return NULL;

    return container_of(avl, struct avl_uid, avl);
}

#ifdef __cplusplus
}
#endif
