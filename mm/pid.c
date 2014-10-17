#include <linux/hashtable.h>

extern int do_tm;

struct pid_s {
	int pid;
	int tid;
};

#define kmaf_PID_HASH_BITS 14UL
#define kmaf_PID_HASH_SIZE (1UL << kmaf_PID_HASH_BITS)

static struct pid_s kmaf_pid[kmaf_PID_HASH_SIZE];
static struct pid_s kmaf_pid_reverse[kmaf_PID_HASH_SIZE];

static atomic_t kmaf_num_threads = ATOMIC_INIT(0);
static atomic_t kmaf_active_threads = ATOMIC_INIT(0);

int kmaf_get_pid(int tid)
{
	return kmaf_pid_reverse[tid].pid;
}


int kmaf_get_tid(int pid)
{
	unsigned h = hash_32(pid, kmaf_PID_HASH_BITS);
	if (kmaf_pid[h].pid == pid)
		return kmaf_pid[h].tid;
	return -1;
}


int kmaf_get_num_threads(void)
{
	return atomic_read(&kmaf_num_threads);
}


int kmaf_get_active_threads(void)
{
	return atomic_read(&kmaf_active_threads);
}


int kmaf_delete_pid(int pid)
{
	int at = -1;

	unsigned h = hash_32(pid, kmaf_PID_HASH_BITS);
	int tid = kmaf_pid[h].tid;

	if (tid != -1 && kmaf_pid[h].pid == pid) {
		kmaf_pid[h].tid = -1;
		kmaf_pid[h].pid = -1;
		/* TODO: need to delete from kmaf_pid_reverse? */
		at = atomic_dec_return(&kmaf_active_threads);
	}

	return at;
}

extern void set_aff(int pid, int tid);

int kmaf_add_pid(int pid)
{
	unsigned h = hash_32(pid, kmaf_PID_HASH_BITS);

	if (kmaf_pid[h].pid == -1) {
		kmaf_pid[h].pid = pid;
		kmaf_pid[h].tid = atomic_inc_return(&kmaf_num_threads) - 1;
		atomic_inc_return(&kmaf_active_threads);

		kmaf_pid_reverse[kmaf_pid[h].tid] = kmaf_pid[h];

		if (do_tm)
			set_aff(pid, kmaf_pid[h].tid);

		return kmaf_pid[h].tid;
	} else {
		printk("kmaf BUG: XXX pid collision: %d->%d\n", kmaf_pid[h].pid, kmaf_pid[h].tid);
		return -1;
	}
}


void kmaf_pid_init(void)
{
	memset(kmaf_pid, -1, sizeof(kmaf_pid));
	memset(kmaf_pid_reverse, -1, sizeof(kmaf_pid_reverse));
	atomic_set(&kmaf_num_threads, 0);
	atomic_set(&kmaf_active_threads, 0);
}
