// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

#define NUM_THREADS 4

static int suma;
static os_graph_t *graph;
static os_threadpool_t *tp;
/* TODO: Define graph synchronization mechanisms. */
pthread_mutex_t graph_blocked = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sum_blocked = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t graph_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t sum_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t sleep_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t sleep_cond = PTHREAD_COND_INITIALIZER;
int should_sleep;

/* TODO: Define graph task argument. */
typedef struct {
	os_graph_t *graph;
	unsigned int index_nod;
} ArgGraph;
struct timespec ts = {
	.tv_sec = 0,
	.tv_nsec = 1000000/NUM_THREADS
};
void *sleeping_thread(void)
{
	nanosleep(&ts, NULL);
	return NULL;
}
static void process_node(unsigned int idx);

void process_node_wrapper(void *argument)
{
	/*
	 * Functia create_task primeste ca parametru (*)(void *), dar functia process node
	 * are ca parametru void (*)(unsigned int). Aceasta functie face conversia
	 */
	unsigned int node_index = *((unsigned int *)argument);

	process_node(node_index);
}

static void process_node(unsigned int index)
{
	/* TODO: Implement thread-pool based processing of graph. */
	os_node_t *nod;

	pthread_mutex_lock(&graph_blocked);

	if (graph == NULL || graph->nodes == NULL || graph->visited == NULL) {
		pthread_mutex_unlock(&graph_blocked);
		return;
	}

	nod = graph->nodes[index];
	pthread_mutex_unlock(&graph_blocked);

	pthread_mutex_lock(&sum_blocked);
	suma = suma + nod->info;
	pthread_mutex_unlock(&sum_blocked);

	pthread_mutex_lock(&graph_blocked);
	graph->visited[index] = 1;
	pthread_cond_broadcast(&graph_cond); // Semnal ca un nod a fost procesat
	pthread_mutex_unlock(&graph_blocked);

	for (unsigned int i = 0; i < nod->num_neighbours; i++) {
		if (graph->visited[nod->neighbours[i]] == 0) {
			unsigned int index_vecin = nod->neighbours[i];
			ArgGraph *arg_task_vecin = malloc(sizeof(ArgGraph));

			if (arg_task_vecin == NULL) {
				perror("malloc");
				return;
			}
			arg_task_vecin->graph = graph;
			arg_task_vecin->index_nod = index_vecin;

			os_task_t *task_vecin = create_task(process_node_wrapper, arg_task_vecin, free);

			if (task_vecin != NULL) {
				//sleeping_thread(); //folosit doar pentru testare
				enqueue_task(tp, task_vecin);
			} else {
				free(arg_task_vecin);
			}

			process_node(nod->neighbours[i]);
		}
	}
}

int main(int argc, char *argv[])
{
	FILE *input_file;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s input_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	input_file = fopen(argv[1], "r");
	DIE(input_file == NULL, "fopen");

	graph = create_graph_from_file(input_file);

	/* TODO: Initialize graph synchronization mechanisms. */
	tp = create_threadpool(NUM_THREADS);

	process_node(0);
	wait_for_completion(tp);
	destroy_threadpool(tp);
	printf("%d", suma);

	return 0;
}

