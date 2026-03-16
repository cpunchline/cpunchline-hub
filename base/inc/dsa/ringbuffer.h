#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

struct ringbuffer_t
{
    unsigned char *data;
    unsigned int in, out, mask, esize;
};

void ringbuffer_init(struct ringbuffer_t *self, unsigned int item_size, void *buf, unsigned int bufsize);

// item count in buf
unsigned int ringbuffer_len(struct ringbuffer_t *self);

// max item count in buf
unsigned int ringbuffer_cap(struct ringbuffer_t *self);

// avail item count
unsigned int ringbuffer_avail(struct ringbuffer_t *self);

int ringbuffer_is_full(struct ringbuffer_t *self);
int ringbuffer_is_empty(struct ringbuffer_t *self);

unsigned int ringbuffer_in(struct ringbuffer_t *self, const void *buf, unsigned int item_count);
unsigned int ringbuffer_out(struct ringbuffer_t *self, void *buf, unsigned int item_count);
unsigned int ringbuffer_out_peek(struct ringbuffer_t *self, void *buf, unsigned int len);

#ifdef __cplusplus
}
#endif
