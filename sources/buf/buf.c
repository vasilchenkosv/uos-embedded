#include <runtime/lib.h>
#include <buf/buf.h>
#include <mem/mem.h>

/*
 * Allocates a buf of the requested size, plus the reserved space
 * for protocol headers. Buffer memory for buf is allocated as one
 * large chunk.
 */
buf_t *
buf_alloc (mem_pool_t *m, unsigned short size, unsigned short reserved)
{
	buf_t *p;

	/* Allocate memory for it. */
	p = (buf_t *)mem_alloc_dirty (m, sizeof(buf_t) + size + reserved);
	if (! p)
		return 0;

	/* Set up internal structure of the buf. */
	p->payload = (unsigned char*) p + sizeof (buf_t) + reserved;
	p->len = p->tot_len = size;
	p->next = 0;
	return p;
}

/*
 * Reallocates the memory for a buf. We have to step
 * through the chain until we find the new endpoint in the buf chain.
 * Then the buf that is right on the endpoint is resized and any
 * further bufs on the chain are deallocated.
 */
void
buf_truncate (buf_t *p, unsigned short size)
{
    buf_t *q, *r;
    unsigned short rsize;

    if (p->tot_len <= size)
        return;

    /* First, step over the bufs that should still be in the chain. */
    rsize = size;
    q = p;
    while (rsize > q->len) {
        rsize -= q->len;
        q = q->next;
    }
    if (q->len != rsize) {
    /* Adjust the length of the buf that will be halved. */
    mem_truncate (q, q->payload - (unsigned char*)q + rsize);
    q->len = rsize;
    }

    /* And deallocate any left over bufs. */
    r = q->next;
    q->next = 0;
    for (q = r; q; q = r) {
        r = q->next;
        buf_free (q);
    }
    p->tot_len = size;
}

void buf_pack(buf_t *p) {
    buf_t *q, *r;

    /* deallocate any left over bufs. */
    r = p;
    q = r->next;
	for (; q; q = r->next) {
	    if (q->len != 0) {
	        r = q;
	        continue;
	    }
        r->next = q->next;
        q->next = 0;
        mem_free(q);
	}

	//* truncates memory blocks if it too large
	for (q = p; q; q = q->next) {
	    unsigned act_size = q->payload - (unsigned char*)q + q->len;
	    if ( mem_size(q) > (act_size + sizeof(buf_t)) )
	        mem_truncate (q, act_size);
	}
}

//* same as buf_truncate, but leave buffer memory allocation as is
void buf_truncate_soft (buf_t *p, unsigned short size){
    buf_t *q, *r;
    unsigned short rsize;

    if (p->tot_len <= size)
        return;

    /* First, step over the bufs that should still be in the chain. */
    rsize = size;
    q = p;
    while (rsize > q->len) {
        assert(q != 0);
        rsize -= q->len;
        q = q->next;
    }
    /* Adjust the length of the buf that will be halved. */
    q->len = rsize;

    /* And deallocate any left over bufs. */
    r = q->next;
    q->next = 0;
    for (q = r; q; q = r) {
        r = q->next;
        q->len = 0;
        q->tot_len = 0;
    }
    p->tot_len = size;
}

/*
 * Adjust the ->payload pointer so that space for a header appears in
 * the buf. Also, the ->tot_len and ->len fields are adjusted.
 * Return 0 when there is not enough space for header.
 */
bool_t
buf_add_header (buf_t *p, short header_size)
{
	if ((p->payload - header_size) < ((unsigned char*) p + sizeof (buf_t)) )
		return 0;

	p->payload -= header_size;
	p->len += header_size;
	p->tot_len += header_size;

	return 1;
}

/*
 * reset p->payload pointer to header_size offset from allocated mem buffer
 * */
bool_t buf_reset_header (buf_t *p, short header_size){
    int preserve = p->payload - (unsigned char*) p - sizeof (buf_t);
    if (header_size < (p->len+preserve))
    {
        unsigned h = preserve - header_size;
        p->payload -= h;
        p->len += h;
        p->tot_len += h;
        return 1;
    }
    else
        return 0;
}

/*
 * Deallocate the buffer. If the buf is a chain all bufs in the
 * chain are deallocated. Return the number of deallocated segments.
 */
small_int_t
buf_free (buf_t *p)
{
	unsigned char count;
	buf_t *q;

	if (! p)
		return 0;

	count = 0;
	while (p) {
		q = p->next;
		mem_free (p);
		p = q;
		count++;
	}
	return count;
}

/*
 * Chain the two bufs h and t together. The ->tot_len field of the
 * first buf (h) is adjusted.
 */
void
buf_chain (buf_t *h, buf_t *t)
{
	buf_t *p;

	p = h;
	while (p->next)
		p = p->next;
	p->next = t;
	h->tot_len += t->tot_len;
}

/*
 * Adjusts the ->tot_len field of the buf and returns the tail (if
 * any) of the buf chain.
 */
buf_t *
buf_dechain (buf_t *p)
{
	buf_t *q;

	q = p->next;
	p->tot_len = p->len;
	p->next = 0;
	return q;
}

/*
 * Convert a bufer to a single continuous memory chunk.
 */
buf_t *
buf_make_continuous (buf_t *p)
{
	buf_t *q, *next;
	int header_size;

	if (! p || ! p->next)
		return p;

	/*
	 * TODO:
	 * Функция mem_realloc () вернет 0 и освободит блок, если не хватит
	 * памяти. Поэтому, если mem_realloc () вернула 0 освободим следующие
	 * в цепочке блоки.
	 * Альтернативный вариант: mem_realloc () при нехватке памяти
	 * возвращает 0, но память не освобождает.
	 */
	next = p->next;
	/* Reallocate the first chunk, to make it big enough. */
	header_size = (p->payload - (unsigned char*) p);
	p = (buf_t *)mem_realloc (p, p->tot_len + header_size);
	if (! p) {
		/* Free all other chunks to the end of the first chunk. */
		for (q = next; q; q = next) {
			next = q->next;
			mem_free (q);
		}
		return 0;
	}

	/* Set up internal structure of the buf. */
	p->payload = (unsigned char*) p + header_size;

	/* Copy all other chunks to the end of the first chunk. */
	for (q = p->next; q; q = next) {
		next = q->next;
		memcpy (p->payload + p->len, q->payload, q->len);
		p->len += q->len;
		mem_free (q);
	}
	p->next = 0;
	return p;
}

/* copy  len bytes from buffer src to single memory chunk dst 
 * */
unsigned buf_copy_continous(void* dst, buf_t *src, unsigned len)
{
    if ( !src || !dst)
        return 0;

    const buf_t * null = (buf_t *)0;

    buf_t *q = src;
    buf_t *next;
    unsigned char* pd = (unsigned char*)dst;
    /* Copy all other chunks to the end of the first chunk. */
    for (; (q != null) && (len > 0); q = next) {
        next = q->next;
        if (q->len <= 0)
            continue;
        unsigned l = (q->len > len)?len:q->len;
        memcpy (pd, q->payload, l);
        len -= l;
        pd  += l;
    }
    return pd - (unsigned char*)dst;
}
