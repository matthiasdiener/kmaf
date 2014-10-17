#include <linux/sched.h>
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include "libmapping/libmapping.h"

extern int do_tm;

static int turing_vec[] = {0,32,4,36,8,40,12,44,16,48,20,52,24,56,28,60,1,33,5,37,9,41,13,45,17,49,21,53,25,57,29,61,2,34,6,38,10,42,14,46,18,50,22,54,26,58,30,62,3,35,7,39,11,43,15,47,19,51,23,55,27,59,31,63};

static const int turing_len = sizeof(turing_vec)/sizeof(turing_vec[0]);

void set_aff(int pid, int tid);
int kmaf_get_active_threads(void);
extern unsigned matrix[MAX_THREADS*MAX_THREADS];

#define for_each_sibling(s, cpu) for_each_cpu(s, cpu_sibling_mask(cpu))
#define for_each_core(s, cpu) for_each_cpu(s, cpu_core_mask(cpu))
#define for_each_node_cpu(s, node) for_each_cpu(s, cpumask_of_node(node))

int num_nodes = 0, num_cpus = 0, num_cores = 0, num_threads = 0;
int pu[256];

void topo_init(void)
{
	int node, cpu, sibling, core;
	int index = 0, i;
	char seen[256] = {};

	printk("kmaf: detected hardware topology:\n");

	for_each_online_node(node) {
		printk("  node: %d\n", node);
		num_nodes++;

		for_each_node_cpu(cpu, node) {
			if (seen[cpu])
				continue;
			printk("    processor: %d\n", cpu);
			num_cpus++;

			for_each_core(core, cpu) {
				if (seen[core])
					continue;
				printk ("      core: %d", core);
				seen[core] = 1;
				num_cores++; num_threads++; pu[index++] = core;
				for_each_sibling(sibling, core) {
					if (seen[sibling])
						continue;
					seen[sibling] = 1;
					num_threads++;
					printk(", %d", sibling);
					pu[index++] = sibling;
				}
				printk("\n");
			}
		}
	}

	printk("PU: ");
	for (i=0; i<num_threads; i++) {
		printk("%d ", pu[i]);
	}

	num_threads /= num_cores;
	num_cores /= num_cpus;
	num_cpus /= num_nodes;

	printk("\nkmaf: %d nodes, %d processors per node, %d cores per processor, %d threads per core\n", num_nodes, num_cpus, num_cores, num_threads);
}

int kmaf_map_func(void* v)
{
	topo_init();
	struct kmaf_comm_matrix kmaf_matrix = {.matrix = NULL, .nthreads = 0};

	int nt, i;
	int arities[] = {num_nodes, num_cpus, num_cores, num_threads};
	int nlevels =  sizeof(arities)/sizeof(arities[0]);
	int npus = 0, nvertices = 0;
	static int map[MAX_THREADS];
	static int oldmap[MAX_THREADS];
	topology_t *topo = libmapping_topology_get();
	thread_map_alg_init_t data;

	libmapping_get_n_pus_fake_topology(arities, nlevels, &npus, &nvertices);
	printk("npus: %d, nvertices: %d\n", npus, nvertices);

	libmapping_graph_init(&topo->graph, nvertices, nvertices-1);

	topo->root = libmapping_create_fake_topology(arities, nlevels, pu, NULL);
	topo->root->weight = 0;
	topo->root->type = GRAPH_ELTYPE_ROOT;

	data.topology = topo;

	libmapping_topology_analysis(topo);

	libmapping_topology_print(topo);

	libmapping_mapping_algorithm_greedy_init(&data);
	// printk("MATRIX::::::::::::::: %p\n", matrix);
	while (1) {
		if (kthread_should_stop())
			break;

		nt = kmaf_get_active_threads();
		if (nt >= 4) {
			thread_map_alg_map_t mapdata;
			mapdata.m_init = &kmaf_matrix;
			mapdata.map = map;

			kmaf_matrix.matrix = matrix;
			kmaf_matrix.nthreads = nt;

			libmapping_mapping_algorithm_greedy_map (&mapdata);
			printk("MAP \"");
			for (i=0; i<nt; i++){
				printk("%d", map[i]);
				if (i != nt-1)
					printk(",");
				if (oldmap[i] != map[i]) {
					oldmap[i] = map[i];
					set_aff(i, map[i]);
				}
			}
			printk("\"\n");
		} else {
			for (i=0; i<MAX_THREADS; i++)
				oldmap[i] = -1;
		}
		msleep_interruptible(100);
	}

	libmapping_graph_destroy(&topo->graph);
	libmapping_mapping_algorithm_greedy_destroy(NULL);
	return 0;
}

void set_aff(int pid, int tid)
{
	struct cpumask mask;

	int core;

	if (do_tm == 1) // Oracle
		core = turing_vec[tid%turing_len];
	else
		core = tid; // Compact

	cpumask_clear(&mask);
	cpumask_set_cpu(core, &mask);

	sched_setaffinity(pid, &mask);
	// printk("map tid %d to core %d\n", tid, core);
}
