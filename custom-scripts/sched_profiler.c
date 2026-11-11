/*
 * sched_profiler - Laboratorio de Sistemas Operacionais
 *
 * Cria N threads que escrevem o seu caractere (A, B, C, ...) num buffer
 * global compartilhado. Cada thread escreve na posicao corrente do ponteiro
 * global e o incrementa, entao o buffer final mostra a ordem em que as
 * threads foram executadas. No final o programa imprime o buffer, o buffer
 * pos-processado (sequencias iguais agrupadas em um caractere) e quantas
 * vezes cada thread foi escalonada.
 *
 * Uso: sched_profiler <tamanho_buffer> <num_threads> [politica] [prioridade]
 *   politica:   SCHED_LOW_IDLE | SCHED_IDLE | SCHED_FIFO | SCHED_RR | SCHED_OTHER
 *               (padrao: SCHED_OTHER)
 *   prioridade: usada por SCHED_FIFO e SCHED_RR (padrao: 1)
 *
 * Exemplo: sched_profiler 10000 4 SCHED_RR
 */
#define _GNU_SOURCE
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

static char *buffer;
static long buffer_size;
static long position;		/* ponteiro global para a posicao corrente */
static pthread_barrier_t start;

static const struct {
	const char *name;
	int policy;
} policies[] = {
	{ "SCHED_LOW_IDLE", SCHED_LOW_IDLE },
	{ "SCHED_IDLE", SCHED_IDLE },
	{ "SCHED_FIFO", SCHED_FIFO },
	{ "SCHED_RR", SCHED_RR },
	{ "SCHED_OTHER", SCHED_OTHER },
};

static int parse_policy(const char *arg)
{
	size_t i;

	for (i = 0; i < sizeof(policies) / sizeof(policies[0]); i++) {
		/* aceita "SCHED_RR" ou so "RR" */
		if (strcmp(arg, policies[i].name) == 0 ||
		    strcmp(arg, policies[i].name + strlen("SCHED_")) == 0)
			return policies[i].policy;
	}
	return -1;
}

static const char *policy_name(int policy)
{
	size_t i;

	for (i = 0; i < sizeof(policies) / sizeof(policies[0]); i++)
		if (policies[i].policy == policy)
			return policies[i].name;
	return "?";
}

static void *run(void *arg)
{
	char c = (char)(long)arg;
	long pos;

	/* espera todas as threads estarem configuradas */
	pthread_barrier_wait(&start);

	for (;;) {
		/* pega a posicao corrente e incrementa o ponteiro (atomico) */
		pos = __sync_fetch_and_add(&position, 1);
		if (pos >= buffer_size)
			break;
		buffer[pos] = c;
	}
	return NULL;
}

static void print_results(int nthreads)
{
	long runs[MAX_THREADS] = { 0 };
	long i;
	int t;

	/* saida sem pos-processamento */
	fwrite(buffer, 1, buffer_size, stdout);
	printf("\n\n");

	/* pos-processamento: agrupa caracteres consecutivos iguais */
	for (i = 0; i < buffer_size; i++) {
		if (i == 0 || buffer[i] != buffer[i - 1]) {
			putchar(buffer[i]);
			runs[buffer[i] - 'A']++;
		}
	}
	printf("\n");

	/* quantas vezes cada thread foi escalonada */
	for (t = 0; t < nthreads; t++)
		printf("%c = %ld ", 'A' + t, runs[t]);
	printf("\n");
}

int main(int argc, char *argv[])
{
	pthread_t threads[MAX_THREADS];
	struct sched_param param;
	int nthreads, policy = SCHED_OTHER, priority = 0;
	int t, ret;

	if (argc < 3) {
		fprintf(stderr,
			"Uso: %s <tamanho_buffer> <num_threads> [politica] [prioridade]\n"
			"Politicas: SCHED_LOW_IDLE SCHED_IDLE SCHED_FIFO SCHED_RR SCHED_OTHER\n",
			argv[0]);
		return 1;
	}

	buffer_size = atol(argv[1]);
	nthreads = atoi(argv[2]);
	if (buffer_size <= 0 || nthreads <= 0 || nthreads > MAX_THREADS) {
		fprintf(stderr, "tamanho_buffer > 0 e 1 <= num_threads <= %d\n",
			MAX_THREADS);
		return 1;
	}
	if (argc > 3) {
		policy = parse_policy(argv[3]);
		if (policy < 0) {
			fprintf(stderr, "Politica desconhecida: %s\n", argv[3]);
			return 1;
		}
	}
	if (policy == SCHED_FIFO || policy == SCHED_RR)
		priority = argc > 4 ? atoi(argv[4]) : 1;

	buffer = malloc(buffer_size);
	if (!buffer) {
		perror("malloc");
		return 1;
	}

	fprintf(stderr, "sched_profiler: buffer=%ld threads=%d politica=%s prioridade=%d\n",
		buffer_size, nthreads, policy_name(policy), priority);

	pthread_barrier_init(&start, NULL, nthreads + 1);

	for (t = 0; t < nthreads; t++) {
		if (pthread_create(&threads[t], NULL, run, (void *)(long)('A' + t)) != 0) {
			perror("pthread_create");
			return 1;
		}
		/* aplica a politica de escalonamento na thread criada */
		param.sched_priority = priority;
		ret = pthread_setschedparam(threads[t], policy, &param);
		if (ret != 0) {
			fprintf(stderr, "pthread_setschedparam(%s): %s\n",
				policy_name(policy), strerror(ret));
			return 1;
		}
	}

	/* libera todas as threads ao mesmo tempo */
	pthread_barrier_wait(&start);

	for (t = 0; t < nthreads; t++)
		pthread_join(threads[t], NULL);

	print_results(nthreads);

	free(buffer);
	return 0;
}
