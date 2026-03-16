#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plug.h"

#define PLUG(name, ret, ...) ret name(__VA_ARGS__);
LIST_OF_PLUGS
#undef PLUG

typedef struct
{
    int a;
    int b;
} Plug;

static Plug *p = NULL;

void plug_init(void)
{
    p = malloc(sizeof(*p));
    assert(p != NULL && "Buy more RAM lol");
    memset(p, 0, sizeof(*p));
    p->a = 0;
    p->b = 0;
    printf("plug_init\n");
}

void *plug_pre_reload(void)
{
    p->a = 12;
    p->b = 12;
    printf("plug_pre_reload\n");
    return p;
}

void plug_post_reload(void *pp)
{
    p = pp;
    printf("plug_post_reload\n");
}

void *plug_load_resource(const char *file_path, size_t *size)
{
    (void)file_path;
    (void)size;
    printf("plug_load_resource\n");

    return NULL;
}

void plug_free_resource(void *data)
{
    (void)data;
    printf("plug_free_resource\n");
}

void plug_update(void)
{
    // start
    printf("plug_update\n");
    printf("a=[%d]\n", p->a);
    printf("b=[%d]\n", p->b);
    // end
}