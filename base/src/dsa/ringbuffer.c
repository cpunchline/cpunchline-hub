#include <assert.h>
#include <string.h>

#include "dsa/ringbuffer.h"

// https://blog.csdn.net/dreamispossible/article/details/91162847
static unsigned int rounddown_pow_of_two(unsigned int n)
{
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return (n + 1) >> 1;
}

void ringbuffer_init(struct ringbuffer_t *self, unsigned int item_size, void *buf, unsigned int bufsize)
{
    self->data = buf;
    self->esize = item_size;
    self->mask = rounddown_pow_of_two(bufsize / item_size) - 1;
    self->in = self->out = 0;
}

// item count in buf
unsigned int ringbuffer_len(struct ringbuffer_t *self)
{
    return self->in - self->out;
};

// max item count in buf
unsigned int ringbuffer_cap(struct ringbuffer_t *self)
{
    return self->mask + 1;
};

// avail item count
unsigned int ringbuffer_avail(struct ringbuffer_t *self)
{
    return ringbuffer_cap(self) - ringbuffer_len(self);
};

int ringbuffer_is_full(struct ringbuffer_t *self)
{
    return ringbuffer_len(self) > self->mask;
};

int ringbuffer_is_empty(struct ringbuffer_t *self)
{
    return self->in == self->out;
};

#define ringbuffer_min(a, b) ((a) < (b) ? (a) : (b))

static void ringbuffer_copy_in(struct ringbuffer_t *self, const void *src, unsigned int len, unsigned int off)
{
    unsigned int size = self->mask + 1;
    unsigned int esize = self->esize;
    unsigned int l;

    off &= self->mask;
    if (esize != 1)
    {
        off *= esize;
        size *= esize;
        len *= esize;
    }
    l = ringbuffer_min(len, size - off);

    memcpy(self->data + off, src, l);
    memcpy(self->data, (const unsigned char *)src + l, len - l);
}

unsigned int ringbuffer_in(struct ringbuffer_t *self, const void *buf, unsigned int item_count)
{
    unsigned int avail = ringbuffer_avail(self);
    if (item_count > avail)
        item_count = avail;

    ringbuffer_copy_in(self, buf, item_count, self->in);

    self->in += item_count;
    return item_count;
}

static void ringbuffer_copy_out(struct ringbuffer_t *self, void *dst, unsigned int len, unsigned int off)
{
    unsigned int size = self->mask + 1;
    unsigned int esize = self->esize;
    unsigned int l;

    off &= self->mask;
    if (esize != 1)
    {
        off *= esize;
        size *= esize;
        len *= esize;
    }
    l = ringbuffer_min(len, size - off);

    memcpy(dst, self->data + off, l);
    memcpy((unsigned char *)dst + l, self->data, len - l);
}

unsigned int ringbuffer_out_peek(struct ringbuffer_t *self, void *buf, unsigned int len)
{
    unsigned int l;
    l = self->in - self->out;
    if (len > l)
        len = l;
    ringbuffer_copy_out(self, buf, len, self->out);
    return len;
}

unsigned int ringbuffer_out(struct ringbuffer_t *self, void *buf, unsigned int item_count)
{
    item_count = ringbuffer_out_peek(self, buf, item_count);
    self->out += item_count;
    return item_count;
}