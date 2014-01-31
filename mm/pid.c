// #include "spcd.h"

#include <linux/hashtable.h>
#include <linux/slab.h>

struct pid_s {
	int pid;
	int tid;
	struct hlist_node node;
};

static DECLARE_HASHTABLE(spcd_pid, 12);
static DECLARE_HASHTABLE(spcd_pid_reverse, 12);

static atomic_t spcd_num_threads = ATOMIC_INIT(0);
static atomic_t spcd_active_threads = ATOMIC_INIT(0);

int spcd_get_pid(int tid)
{
	struct pid_s *p;
	struct hlist_node *n;
	hash_for_each_possible(spcd_pid_reverse, p, n, node, tid)
		if (p->tid == tid)
			return p->pid;
	return -1;
}


int spcd_get_tid(int pid)
{
	struct pid_s *p;
	struct hlist_node *n;
	hash_for_each_possible(spcd_pid, p, n, node, pid)
		if (p->pid == pid)
			return p->tid;
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
	struct pid_s *p;
	struct hlist_node *n;

	int tid = spcd_get_tid(pid);

	int at = atomic_dec_return(&spcd_active_threads);

	hash_for_each_possible(spcd_pid_reverse, p, n, node, tid)
		if (p->tid == tid)
			hash_del(&p->node);

	hash_for_each_possible(spcd_pid, p, n, node, pid)
		if (p->pid == pid) {
			hash_del(&p->node);
			kfree(p);
		}

	return at;
}


int spcd_add_pid(int pid)
{
	struct pid_s *p = kmalloc(sizeof(struct pid_s), GFP_ATOMIC);
	p->pid = pid;
	p->tid = atomic_inc_return(&spcd_num_threads) - 1;
	atomic_inc_return(&spcd_active_threads);

	hash_add(spcd_pid, &p->node, pid);
	hash_add(spcd_pid_reverse, &p->node, p->tid);

	return p->tid;
}


void spcd_pid_init(void)
{
	hash_init(spcd_pid);
	hash_init(spcd_pid_reverse);
	atomic_set(&spcd_num_threads, 0);
	atomic_set(&spcd_active_threads, 0);
}
