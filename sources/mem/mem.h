#ifndef __MEM_H_
#define	__MEM_H_ 1

#include <kernel/uos.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _mem_pool_t {
	mutex_t lock;		/* Lock used to avoid corruption problems. */

	size_t free_size;	/* The amount of free memory. */

	void *free_list;	/* Linked list of memory holes,
				 * ordered lowest-addressed block first. */
#if MEM_DEBUG
	void*   store;
    size_t  size;
#endif
};

/**
 * A memory pool object.
 * The system can have several independent memory pools.
 */
typedef struct _mem_pool_t mem_pool_t;

/*
 * Memory allocation functions.
 */
void mem_init (mem_pool_t *region, size_t start, size_t stop) __cpp_decls;
void mem_check32 (uint32_t start, uint32_t end, void (*error_callback) (void)) __cpp_decls;
void *mem_alloc (mem_pool_t *region, size_t bytes) __cpp_decls;
void *mem_xalloc (mem_pool_t *region, size_t bytes, const char *title);
void *mem_alloc_dirty (mem_pool_t *region, size_t bytes) __cpp_decls;
void *mem_realloc (void *block, size_t bytes) __cpp_decls;
void mem_truncate (void *block, size_t bytes) __cpp_decls;
void mem_free (void *block) __cpp_decls;
size_t mem_available (mem_pool_t *region) __cpp_decls;
size_t mem_size (void *block) __cpp_decls;
mem_pool_t *mem_pool (void *block) __cpp_decls;
unsigned char *mem_strdup (mem_pool_t *region, const unsigned char *s) __cpp_decls;
unsigned char *mem_strndup (mem_pool_t *region, const unsigned char *s, size_t n) __cpp_decls;



#if MEM_DEBUG
void mem_print_free_list (mem_pool_t *m);
void mem_dump(mem_pool_t *m);
void mem_validate(mem_pool_t *m);
void mem_validate_block(void *p);
#else
#define mem_validate_block(x)
#define mem_validate(m);
#endif

#ifdef __cplusplus
}
#endif

#endif /* !__MEM_H_ */
