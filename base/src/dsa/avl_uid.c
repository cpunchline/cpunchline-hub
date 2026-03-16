#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

#include "dsa/avl_uid.h"

static int random_fd = -1;

static int avl_strcmp(const void *k1, const void *k2, void *ptr)
{
    (void)ptr;
    return strcmp(k1, k2);
}

static int avl_cmp_id(const void *k1, const void *k2, void *ptr)
{
    (void)ptr;
    const uint32_t *id1 = k1;
    const uint32_t *id2 = k2;

    if (*id1 < *id2)
        return -1;
    else
        return *id1 > *id2;
}

void avl_init_string_tree(struct avl_tree *tree, bool dup)
{
    avl_init(tree, avl_strcmp, dup, NULL);
}

void avl_init_id_tree(struct avl_tree *tree)
{
    if (random_fd < 0)
    {
        random_fd = open("/dev/urandom", O_RDONLY);
        if (random_fd < 0)
        {
            perror("open");
            exit(1);
        }
    }

    avl_init(tree, avl_cmp_id, false, NULL);
}

bool avl_alloc_id(struct avl_tree *tree, struct avl_uid *id, uint32_t val)
{
    id->avl.key = &id->id;
    if (val)
    {
        id->id = val;
        return avl_insert(tree, &id->avl) == 0;
    }

    do
    {
        if (read(random_fd, &id->id, sizeof(id->id)) != sizeof(id->id))
            return false;
    } while (avl_insert(tree, &id->avl) != 0);

    return true;
}