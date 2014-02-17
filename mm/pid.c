#include <linux/hashtable.h>

struct pid_s {
	int pid;
	int tid;
};

#define SPCD_PID_HASH_BITS 14UL
#define SPCD_PID_HASH_SIZE (1UL << SPCD_PID_HASH_BITS)

static struct pid_s spcd_pid[SPCD_PID_HASH_SIZE];
static struct pid_s spcd_pid_reverse[SPCD_PID_HASH_SIZE];

static atomic_t spcd_num_threads = ATOMIC_INIT(0);
static atomic_t spcd_active_threads = ATOMIC_INIT(0);

int spcd_get_pid(int tid)
{
	return spcd_pid_reverse[tid].pid;
}


int spcd_get_tid(int pid)
{
	unsigned h = hash_32(pid, SPCD_PID_HASH_BITS);
	if (spcd_pid[h].pid == pid)
		return spcd_pid[h].tid;
	return -1;
}


int spcd_get_num_threads(void)
{
	return atomic_read(&spcd_num_threads);
}


int spcd_get_active_threads(void)
{
	return atomic_read(&spcd_active_threads);
}


int spcd_delete_pid(int pid)
{
	int at = -1;

	unsigned h = hash_32(pid, SPCD_PID_HASH_BITS);
	int tid = spcd_pid[h].tid;

	if (tid != -1 && spcd_pid[h].pid == pid) {
		spcd_pid[h].tid = -1;
		spcd_pid[h].pid = -1;
		/* TODO: need to delete from spcd_pid_reverse? */
		at = atomic_dec_return(&spcd_active_threads);
	}

	return at;
}

extern void set_aff(int pid, int tid);

int spcd_add_pid(int pid)
{
	unsigned h = hash_32(pid, SPCD_PID_HASH_BITS);

	if (spcd_pid[h].pid == -1) {
		spcd_pid[h].pid = pid;
		spcd_pid[h].tid = atomic_inc_return(&spcd_num_threads) - 1;
		atomic_inc_return(&spcd_active_threads);

		spcd_pid_reverse[spcd_pid[h].tid] = spcd_pid[h];

		// set_aff(pid, spcd_pid[h].tid);

		return spcd_pid[h].tid;
	} else {
		printk("SPCD BUG: XXX pid collision: %d->%d\n", spcd_pid[h].pid, spcd_pid[h].tid);
		return -1;
	}
}


void spcd_pid_init(void)
{
	memset(spcd_pid, -1, sizeof(spcd_pid));
	memset(spcd_pid_reverse, -1, sizeof(spcd_pid_reverse));
	atomic_set(&spcd_num_threads, 0);
	atomic_set(&spcd_active_threads, 0);
}
