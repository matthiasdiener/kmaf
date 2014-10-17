#include "libmapping.h"

#if defined(DEBUG) && 1
	#undef DEBUG
	#undef dprintf
	#define dprintf(...)
#endif

typedef enum group_type_t {
	GROUP_TYPE_THREAD,
	GROUP_TYPE_GROUP
} group_type_t;

#ifdef DEBUG
	static const char *group_type_str[] = {
		"THREAD",
		"GROUP"
	};
#endif

struct thread_group_t;

typedef struct thread_group_t {
	group_type_t type;
	uint32_t id;
	uint32_t nelements;
	struct thread_group_t *elements[MAX_THREADS];
} thread_group_t;

static uint32_t *arities;
static uint32_t *exec_el_in_level;
static uint32_t levels_n = 0;
static thread_group_t **groups; // one for each level
static comm_matrix_t matrix_[2];
static topology_t *hardware_topology;

static void generate_group(comm_matrix_t *m, uint32_t total_elements, uint32_t group_elements, thread_group_t *group, uint32_t level, uint32_t *chosen)
{
	static uint32_t winners[MAX_THREADS];
	weight_t w, wmax;
	uint32_t winner = 0;
	uint32_t i, j, k;

	for (i=0; i<group_elements; i++) { // in each iteration, I will find one element of the group
		wmax = -1;
/*		libmapping_print_matrix(m, stdout);*/
		for (j=0; j<total_elements; j++) { // I will iterate over all elements to find the one that maximizes the communication relative to the elements that are already in the group
			if (!chosen[j]) {
				w = 0;
				for (k=0; k<i; k++) {
					w += comm_matrix_ptr_el(m, j, winners[k]);
/*					lm_printf("   m[%u][%u] = %llu", j, winners[k], m->values[j][ winners[k] ]);*/
				}
/*				lm_printf("\n");*/
/*				if (i > 0)*/
/*					dprintf("   max[%u] = %llu\n", j, w);*/
				if (w > wmax) {
					wmax = w;
					winner = j;
				}
			}
		}
		dprintf("   winner[%u] is %u (wmax "PRINTF_UINT64")\n", i, winner, wmax);//getchar();
		chosen[winner] = 1;
		winners[i] = winner;
		group->elements[i] = &groups[level-1][winner];
	}
}

static uint32_t generate_groups(comm_matrix_t *m, uint32_t nelements, uint32_t level)
{
	static uint32_t chosen[MAX_THREADS];
	uint32_t el_per_group, done, leftover, ngroups, avl_groups, group_i, in_group, i;
	thread_group_t *group;

	avl_groups = exec_el_in_level[level];

	ngroups = (nelements > avl_groups) ? avl_groups : nelements;

	el_per_group = nelements / ngroups;
	leftover = nelements % ngroups;

	dprintf("arity %u, avl_groups %u, ngroups %u, nelements %u, el_per_group %u, leftover %u\n", arities[level], avl_groups, ngroups, nelements, el_per_group, leftover);

	group_i = 0;

	for (i=0; i<nelements; i++)
		chosen[i] = 0;

	for (done=0; done<nelements; done+=in_group) {
		in_group = el_per_group;
		if (leftover) {
			in_group++;
			leftover--;
		}
		dprintf("group %u with %u elements\n", group_i, in_group);
		group = &groups[level][group_i];
		group->type = GROUP_TYPE_GROUP;
		group->nelements = in_group;
		group->id = group_i;
		generate_group(m, nelements, in_group, group, level, chosen);
		group_i++;
	}

	return ngroups;
}

static void map_groups_to_topology_ (vertex_t *v, vertex_t *from, thread_group_t *g, uint32_t *map, uint32_t level)
{
	uint32_t i;

	dprintf("group type %s with %u elements, vertex type %s arity %u\n", group_type_str[g->type], g->nelements, libmapping_graph_eltype_str(v->type), (v->type == GRAPH_ELTYPE_ROOT) ? v->arity : v->arity-1);

	if (v->type == GRAPH_ELTYPE_PU) {
		// LM_ASSERT(g->nelements > 0);
		// LM_ASSERT(g->elements[0]->type == GROUP_TYPE_THREAD);

		dprintf("found pu %u\n", v->id);//getchar();

		for (i=0; i<g->nelements; i++) {
			map[ g->elements[i]->id ] = v->id;
			dprintf("   mapping thread %u to pu %u\n", g->elements[i]->id, v->id);
		}
	}
	else if ((v->type == GRAPH_ELTYPE_ROOT && v->arity >= 2) || v->arity > 2) { // is shared level
		// LM_ASSERT_PRINTF((v->type == GRAPH_ELTYPE_ROOT && g->nelements <= v->arity) || (g->nelements <= (v->arity-1)), "g->nelements=%u v->arity=%u\n", g->nelements, v->arity)
		for (i=0; i<g->nelements; i++) {
			if (v->linked[i].v != from) {
				dprintf("found level %u arity %u\n", level, (v->type == GRAPH_ELTYPE_ROOT) ? v->arity : v->arity-1);//getchar();
				map_groups_to_topology_(v->linked[i].v, v, g->elements[i], map, level+1);
			}
			else {
				LM_ASSERT(0)
			}
		}
	}
	else {
		if (v->type == GRAPH_ELTYPE_ROOT) { // non-shared root, arity is 1
			// LM_ASSERT(v->arity == 1);
			dprintf("akipping root node (level %u) because it is non-shared\n", level);//getchar();
			map_groups_to_topology_(v->linked[0].v, v, g, map, level+1);
		}
		else { // non-shared intermediate node, arity is 2
			// LM_ASSERT(v->arity == 2);
			dprintf("akipping level %u because it is non-shared\n", level);//getchar();
			if (v->linked[0].v != from)
				map_groups_to_topology_(v->linked[0].v, v, g, map, level+1);
			else
				map_groups_to_topology_(v->linked[1].v, v, g, map, level+1);
		}
	}
}

static void map_groups_to_topology (topology_t *t, thread_group_t *g, uint32_t *map)
{
	map_groups_to_topology_(t->root, NULL, g, map, 0);
}

static void recreate_matrix (comm_matrix_t *old, thread_group_t *group_set, uint32_t ngroups, comm_matrix_t *m)
{
	uint32_t i, j, k, z;
	weight_t w;

	m->nthreads = ngroups;

	for (i=0; i<ngroups-1; i++) {
		for (j=i+1; j<ngroups; j++) {
			w = 0;
			for (k=0; k<group_set[i].nelements; k++) {
				for (z=0; z<group_set[j].nelements; z++) {
					w += comm_matrix_ptr_el(old, group_set[i].elements[k]->id, group_set[j].elements[z]->id );
				}
			}

			comm_matrix_ptr_write(m, i, j, w);
		}
	}
}

static int detect_levels_with_sharers (void *data, vertex_t *v, vertex_t *previous_vertex, edge_t *edge, uint32_t level)
{
	if ((v->type == GRAPH_ELTYPE_ROOT && v->arity >= 2) || v->arity > 2) // is shared level
		levels_n++;
	if (v->type == GRAPH_ELTYPE_PU)
		return 0;
	else
		return 1;
}

static int detect_arity_of_levels_with_sharers (void *data, vertex_t *v, vertex_t *previous_vertex, edge_t *edge, uint32_t level)
{
	uint32_t *pos = (uint32_t*)data;

	if (v->type == GRAPH_ELTYPE_ROOT && v->arity >= 2) { // is shared level
		arities[*pos] = v->arity;
		(*pos)--;
	}
	else if (v->arity > 2) { // is shared level
		arities[*pos] = v->arity - 1;
		(*pos)--;
	}
	if (v->type == GRAPH_ELTYPE_PU)
		return 0;
	else
		return 1;
}

void* libmapping_mapping_algorithm_greedy_init (thread_map_alg_init_t *data)
{
	uint32_t pos, i;

	hardware_topology = data->topology;
	levels_n = 2;
	libmapping_topology_walk_pre_order(data->topology, detect_levels_with_sharers, NULL);

	lm_printf(PRINTF_PREFIX "allocating space for %u levels\n", levels_n);

	arities = (uint32_t*)lm_calloc(levels_n, sizeof(uint32_t));
	LM_ASSERT(arities != NULL);

	arities[0] = arities[1] = 1; // first arities should always be 1
	pos = levels_n - 1;
	libmapping_topology_walk_pre_order(data->topology, detect_arity_of_levels_with_sharers, &pos);

	exec_el_in_level = (uint32_t*)lm_calloc(levels_n, sizeof(uint32_t));
	LM_ASSERT(exec_el_in_level != NULL);

	exec_el_in_level[0] = data->topology->pu_number;
	for (i=1; i<levels_n; i++) {
		exec_el_in_level[i] = exec_el_in_level[i - 1] / arities[i];
	}

	for (i=levels_n-1; i>0; i--) {
		if (exec_el_in_level[i] == 1) {
			levels_n = i;
			lm_printf(PRINTF_PREFIX "truncating to level %u\n", levels_n-1);
		}
	}

	lm_printf(PRINTF_PREFIX "used %u levels\n", levels_n);

	groups = libmapping_matrix_malloc(levels_n, MAX_THREADS, sizeof(thread_group_t));

#ifndef _kmaf
	libmapping_comm_matrix_init(&matrix_[0], MAX_THREADS);
	libmapping_comm_matrix_init(&matrix_[1], MAX_THREADS);
#else
{
	extern int max_threads;
	libmapping_comm_matrix_init(&matrix_[0], max_threads);
	libmapping_comm_matrix_init(&matrix_[1], max_threads);
}
#endif


#if 1
	lm_printf(PRINTF_PREFIX "there are %u shared levels\n", (levels_n >= 2) ? levels_n - 2 : 0);
	for (i=0; i<levels_n; i++) {
		lm_printf(PRINTF_PREFIX "   level[%u]  arity: %u, exec_el_in_level: %u\n", i, arities[i], exec_el_in_level[i]);
	}//getchar();
#endif

	return NULL;
}

void libmapping_mapping_algorithm_greedy_destroy (void *data)
{
	libmapping_matrix_free(groups);
	libmapping_comm_matrix_destroy(&matrix_[0]);
	libmapping_comm_matrix_destroy(&matrix_[1]);
	lm_free(arities);
	lm_free(exec_el_in_level);
}

void libmapping_mapping_algorithm_greedy_map (thread_map_alg_map_t *data)
{
	uint32_t nelements, i, ngroups, matrix_i;
	comm_matrix_t *m, *oldm;
	static thread_group_t root_group;

	matrix_i = 0;

	for (i=0; i<data->m_init->nthreads; i++) {
		groups[0][i].type = GROUP_TYPE_THREAD;
		groups[0][i].id = i;
		groups[0][i].nelements = 0;
	}

	nelements = data->m_init->nthreads;
	m = data->m_init;

	for (i=1; i<levels_n; i++) {
		dprintf("generating level %u\n", i);

	#ifdef DEBUG
		libmapping_print_matrix(m, stdout);
	#endif

		ngroups = generate_groups(m, nelements, i);
		dprintf("levels %u with %u groups\n", i, ngroups);

		oldm = m;
		m = &matrix_[matrix_i];
		matrix_i ^= 1; // switch buffers on every iteration

		if (i < (levels_n-1)) {
			recreate_matrix(oldm, groups[i], ngroups, m);
		}

		nelements = ngroups;
		//getchar();
	}

	root_group.type = GROUP_TYPE_GROUP;
	root_group.nelements = nelements;

	for (i=0; i<nelements; i++)
		root_group.elements[i] = &groups[levels_n-1][i];

	map_groups_to_topology(hardware_topology, &root_group, data->map);

#ifdef DEBUG
	dprintf("mapping: ");
	for (i=0; i<m_init->nthreads; i++) {
		lm_printf(" %u", map[i]);
	}
	lm_printf("\n");
#endif
}
