#ifndef __LIBMAPPING_LIB_H__
#define __LIBMAPPING_LIB_H__


#define LM_PRINTF_OUT NULL
#define lm_printf(...) printk(__VA_ARGS__)
#define lm_fprintf(out, ...) printk(__VA_ARGS__)

#define lm_calloc(count, tsize) vmalloc((count) * (tsize))
#define lm_free(p) vfree(p)


#define LM_ASSERT(V) LM_ASSERT_PRINTF(V, "bye!\n")

#define LM_ASSERT_PRINTF(V, ...) \
	if (unlikely(!(V))) { \
		lm_printf(PRINTF_PREFIX "sanity error!\nfile %s at line %u assertion failed!\n%s\n", __FILE__, __LINE__, #V); \
		lm_printf(__VA_ARGS__); \
	}

#ifdef DEBUG
	#define dprintf(...) lm_printf(PRINTF_PREFIX __VA_ARGS__)
#else
	#define dprintf(...)
#endif

struct kmaf_comm_matrix {
	unsigned *matrix;
	spinlock_t lock;
	uint32_t nthreads;
};

typedef struct kmaf_comm_matrix comm_matrix_t;
static int max_threads_bits = ilog2(MAX_THREADS);

static inline
unsigned get_matrix(struct kmaf_comm_matrix *m, int i, int j)
{
	// return i > j ? m->matrix[(i<<max_threads_bits) + j] : m->matrix[(j<<max_threads_bits) + i];
	return i > j ? m->matrix[(i<<max_threads_bits) + j] : m->matrix[(j<<max_threads_bits) + i];
	// printk("%p %d, %d\n", m->matrix, i, j);
	// return m->matrix[i*MAX_THREADS+j];
}

static inline
void set_matrix(struct kmaf_comm_matrix *m, int i, int j, unsigned v)
{
	// m->matrix[i*MAX_THREADS+j] = v;
	// m->matrix[j*MAX_THREADS+i] = v;
	if (i > j)
		m->matrix[(i << max_threads_bits) + j] = v;
	else
		m->matrix[(j << max_threads_bits) + i] = v;
}

#define comm_matrix_el(m, row, col) get_matrix(&(m), row, col)
#define comm_matrix_ptr_el(m, row, col) get_matrix(m, row, col)
#define comm_matrix_ptr_write(m, row, col, v) set_matrix(m, row, col, v)


void* libmapping_matrix_malloc (uint32_t nrows, uint32_t ncols, uint32_t type_size);
void libmapping_matrix_free (void *m);
void libmapping_comm_matrix_init (comm_matrix_t *m, uint32_t nthreads);
void libmapping_comm_matrix_destroy (comm_matrix_t *m);

void libmapping_set_aff_thread (pid_t pid, uint32_t cpu);
char* libmapping_strtok(char *str, char *tok, char del, uint32_t bsize);


uint64_t libmapping_get_next_power_of_two (uint64_t v);
uint8_t libmapping_get_log2 (uint64_t v);

#endif
