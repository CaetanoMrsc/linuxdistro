/*
 * thread_runner - Tutorial 3.1 (SCHED_LOW_IDLE)
 *
 * Cria uma thread por politica informada. Todas disputam a mesma CPU e
 * gravam a sua letra (A, B, C...) num buffer compartilhado ate ele encher.
 * No final mostra a ordem de execucao (compactada) e quanto cada thread
 * conseguiu escrever.
 *
 * Uso:   thread_runner <tamanho_buffer> <politica>[:prioridade] ...
 * Politicas: NORMAL, BATCH, IDLE, LOW_IDLE, FIFO, RR
 * Exemplo: thread_runner 200000 IDLE LOW_IDLE
 */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SCHED_IDLE
#define SCHED_IDLE 5
#endif
#ifndef SCHED_LOW_IDLE
#define SCHED_LOW_IDLE 7
#endif

#define MAX_THREADS 26

struct thread_info {
	pthread_t tid;
	char letter;
	int policy;
	int priority;
	const char *name;
	long count;
};

static char *buffer;
static long buffer_size;
static long buffer_pos;
static pthread_mutex_t buffer_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_barrier_t start_barrier;

static const struct {
	const char *name;
	int policy;
} policies[] = {
	{ "NORMAL", SCHED_OTHER },
	{ "BATCH", SCHED_BATCH },
	{ "IDLE", SCHED_IDLE },
	{ "LOW_IDLE", SCHED_LOW_IDLE },
	{ "FIFO", SCHED_FIFO },
	{ "RR", SCHED_RR },
};

static int parse_policy(const char *s, const char **name)
{
	size_t i;

	for (i = 0; i < sizeof(policies) / sizeof(policies[0]); i++) {
		if (strcmp(s, policies[i].name) == 0) {
			*name = policies[i].name;
			return policies[i].policy;
		}
	}
	return -1;
}

static void *run(void *arg)
{
	struct thread_info *t = arg;
	struct sched_param param = { .sched_priority = t->priority };

	/* sched_setscheduler(0, ...) muda apenas a thread que chama. */
	if (sched_setscheduler(0, t->policy, &param) != 0) {
		fprintf(stderr, "thread %c: sched_setscheduler(%s) falhou: %s\n",
			t->letter, t->name, strerror(errno));
		exit(1);
	}

	pthread_barrier_wait(&start_barrier);

	for (;;) {
		pthread_mutex_lock(&buffer_lock);
		if (buffer_pos >= buffer_size) {
			pthread_mutex_unlock(&buffer_lock);
			break;
		}
		buffer[buffer_pos++] = t->letter;
		pthread_mutex_unlock(&buffer_lock);
		t->count++;
	}
	return NULL;
}

static void print_sequence(void)
{
	long i = 0;

	printf("Sequencia de execucao (letra x quantidade):\n");
	while (i < buffer_size) {
		long j = i;

		while (j < buffer_size && buffer[j] == buffer[i])
			j++;
		printf("%c%ld ", buffer[i], j - i);
		i = j;
	}
	printf("\n");
}

int main(int argc, char *argv[])
{
	struct thread_info threads[MAX_THREADS];
	int n, i;
	cpu_set_t cpus;

	if (argc < 3) {
		fprintf(stderr,
			"Uso: %s <tamanho_buffer> <politica>[:prioridade] ...\n"
			"Politicas: NORMAL BATCH IDLE LOW_IDLE FIFO RR\n"
			"Exemplo: %s 200000 IDLE LOW_IDLE\n", argv[0], argv[0]);
		return 1;
	}

	buffer_size = atol(argv[1]);
	n = argc - 2;
	if (buffer_size <= 0 || n > MAX_THREADS) {
		fprintf(stderr, "Parametros invalidos\n");
		return 1;
	}
	buffer = malloc(buffer_size);
	if (!buffer) {
		perror("malloc");
		return 1;
	}

	/* Todas as threads na mesma CPU, para disputarem o mesmo processador. */
	CPU_ZERO(&cpus);
	CPU_SET(0, &cpus);
	if (sched_setaffinity(0, sizeof(cpus), &cpus) != 0)
		perror("sched_setaffinity");

	printf("sched_get_priority_max/min(SCHED_LOW_IDLE) = %d/%d\n",
	       sched_get_priority_max(SCHED_LOW_IDLE),
	       sched_get_priority_min(SCHED_LOW_IDLE));

	pthread_barrier_init(&start_barrier, NULL, n + 1);

	for (i = 0; i < n; i++) {
		char spec[64];
		char *colon;

		strncpy(spec, argv[i + 2], sizeof(spec) - 1);
		spec[sizeof(spec) - 1] = '\0';
		colon = strchr(spec, ':');
		threads[i].priority = 0;
		if (colon) {
			*colon = '\0';
			threads[i].priority = atoi(colon + 1);
		}
		threads[i].policy = parse_policy(spec, &threads[i].name);
		if (threads[i].policy < 0) {
			fprintf(stderr, "Politica desconhecida: %s\n", spec);
			return 1;
		}
		threads[i].letter = 'A' + i;
		threads[i].count = 0;
		if (pthread_create(&threads[i].tid, NULL, run, &threads[i]) != 0) {
			perror("pthread_create");
			return 1;
		}
	}

	pthread_barrier_wait(&start_barrier);

	for (i = 0; i < n; i++)
		pthread_join(threads[i].tid, NULL);

	print_sequence();
	printf("\nResumo:\n");
	for (i = 0; i < n; i++)
		printf("  %c  %-9s %8ld  (%5.1f%%)\n", threads[i].letter,
		       threads[i].name, threads[i].count,
		       100.0 * threads[i].count / buffer_size);

	free(buffer);
	return 0;
}
