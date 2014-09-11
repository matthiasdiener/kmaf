#ifndef __LIBMAPPING_MAPPING_H__
#define __LIBMAPPING_MAPPING_H__

#define UNDEFINED_MAPPING 0xFFFFFFFF

typedef uint16_t thread_stat_t;

enum {
	THREAD_AVL,
	THREAD_RUNNING,
	THREAD_WAITING,
	THREAD_DEAD
};

typedef struct thread_t {
	pid_t kernel_tid;
	uint32_t order_id;
	uint32_t app_id;
	uint32_t mapping;
	uint32_t pos;
	uint32_t alive_pos;
	thread_stat_t stat;


#ifdef LIBMAPPING_STATS_THREAD_LOAD
	uint64_t time_start;
	uint64_t time_runstart;
	uint64_t time_end;
	uint64_t time_running;
#endif
}  thread_t;

typedef struct pu_t {
	uint32_t id;
	thread_t *threads[MAX_THREADS]; // threads currently running in pu
	uint32_t nthreads; // total number of threads currently running in pu
	uint32_t nthreads_runnable;
	double load; // load considering only THREAD_RUNNING
	double total_load; // load considering THREAD_RUNNING and THREAD_WAITING
} pu_t;

static inline thread_t* libmapping_get_thread_by_order_id (uint32_t order)
{
	extern thread_t *libmapping_threads_by_order[MAX_THREADS];
	return libmapping_threads_by_order[order];
}

static inline thread_t* libmapping_get_thread_by_alive_pos (uint32_t pos)
{
	extern thread_t *libmapping_alive_threads[MAX_THREADS];
	return libmapping_alive_threads[pos];
}

static inline uint8_t libmapping_thread_exists (thread_t *t)
{
	return (t->stat == THREAD_RUNNING || t->stat == THREAD_WAITING);
}

void libmapping_init(void);
void libmapping_end(void);

static inline uint32_t libmapping_get_total_nthreads (void)
{
	extern uint32_t libmapping_nthreads_total;
	return libmapping_nthreads_total;
}

static inline uint32_t libmapping_get_alive_nthreads (void)
{
	extern uint32_t libmapping_alive_nthreads;
	return libmapping_alive_nthreads;
}

thread_t* libmapping_thread_created(uint32_t order);
void libmapping_thread_destroyed(thread_t *t);
int libmapping_check_migrate (comm_matrix_t *new_matrix);
double libmapping_get_thread_load (thread_t *t);

void libmapping_unmap_thread_from_pu (thread_t *t);
void libmapping_map_thread_to_pu (thread_t *t, pu_t *pu);

void libmapping_thread_wait (thread_t *t);
void libmapping_thread_run (thread_t *t);

#ifdef LIBMAPPING_SUMMON_THREAD
	void libmapping_thread (void);
#endif

#endif
