/*
 * sched_profiler - Laboratorio de Sistemas Operacionais
 *
 * Cria N threads que escrevem o seu caractere (A, B, C, ...) num buffer
 * global compartilhado. Cada thread escreve na posicao corrente do ponteiro
 * global e o incrementa; o acesso ao buffer e ao ponteiro e sincronizado
 * com um semaforo binario compartilhado entre as threads. No final o
 * programa imprime o buffer, o buffer pos-processado (sequencias iguais
 * agrupadas em um caractere) e quantas vezes cada thread foi escalonada.
 *
 * Uso: sched_profiler <tamanho_buffer> <num_threads> <policy>
 *   policy: SCHED_LOW_IDLE | SCHED_IDLE | SCHED_FIFO | SCHED_RR
 *
 * Exemplo: sched_profiler 10000 4 SCHED_RR
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
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
#define RT_PRIORITY 1		/* prioridade usada por SCHED_FIFO e SCHED_RR */

static char *buffer;		/* buffer global */
static char *pointer;		/* ponteiro global: proxima posicao livre */
static char *buffer_end;
static sem_t mutex;		/* semaforo binario que protege buffer e ponteiro */
static pthread_barrier_t start;

static const struct {
	const char *name;
	int policy;
} policies[] = {
	{ "SCHED_LOW_IDLE", SCHED_LOW_IDLE },
	{ "SCHED_IDLE", SCHED_IDLE },
	{ "SCHED_FIFO", SCHED_FIFO },
	{ "SCHED_RR", SCHED_RR },
};

static int parse_policy(const char *arg)
{
	size_t i;

	for (i = 0; i < sizeof(policies) / sizeof(policies[0]); i++)
		if (strcmp(arg, policies[i].name) == 0)
			return policies[i].policy;
	return -1;
}

static void *run(void *arg)
{
	char c = (char)(long)arg;
	int done = 0;

	/* espera todas as threads estarem criadas e configuradas */
	pthread_barrier_wait(&start);

	while (!done) {
		sem_wait(&mutex);
		if (pointer < buffer_end) {
			*pointer = c;	/* escreve na posicao corrente */
			pointer++;	/* e incrementa o ponteiro global */
		} else {
			done = 1;
		}
		sem_post(&mutex);
	}
	return NULL;
}

static void print_results(long size, int nthreads)
{
	long runs[MAX_THREADS] = { 0 };
	long i;
	int t;

	/* saida sem pos-processamento */
	fwrite(buffer, 1, size, stdout);
	printf("\n\n");

	/* pos-processamento: agrupa caracteres consecutivos iguais */
	for (i = 0; i < size; i++) {
		if (i == 0 || buffer[i] != buffer[i - 1]) {
			putchar(buffer[i]);
			runs[buffer[i] - 'A']++;
		}
	}
	printf("\n");

	/* quantas vezes cada thread foi escalonada */
	for (t = 0; t < nthreads; t++)
		printf("%c = %ld\n", 'A' + t, runs[t]);
}

int main(int argc, char *argv[])
{
	pthread_t threads[MAX_THREADS];
	struct sched_param param;
	long size;
	int nthreads, policy, t, ret;

	if (argc != 4) {
		fprintf(stderr,
			"Uso: %s <tamanho_buffer> <num_threads> <policy>\n"
			"policy: SCHED_LOW_IDLE | SCHED_IDLE | SCHED_FIFO | SCHED_RR\n",
			argv[0]);
		return 1;
	}

	size = atol(argv[1]);
	nthreads = atoi(argv[2]);
	policy = parse_policy(argv[3]);
	if (size <= 0 || nthreads <= 0 || nthreads > MAX_THREADS) {
		fprintf(stderr, "tamanho_buffer > 0 e 1 <= num_threads <= %d\n",
			MAX_THREADS);
		return 1;
	}
	if (policy < 0) {
		fprintf(stderr, "Politica desconhecida: %s\n", argv[3]);
		return 1;
	}

	buffer = malloc(size);
	if (!buffer) {
		perror("malloc");
		return 1;
	}
	pointer = buffer;		/* ponteiro global no inicio do buffer */
	buffer_end = buffer + size;

	sem_init(&mutex, 0, 1);		/* semaforo binario (valor inicial 1) */
	pthread_barrier_init(&start, NULL, nthreads + 1);

	param.sched_priority =
		(policy == SCHED_FIFO || policy == SCHED_RR) ? RT_PRIORITY : 0;

	for (t = 0; t < nthreads; t++) {
		if (pthread_create(&threads[t], NULL, run, (void *)(long)('A' + t)) != 0) {
			perror("pthread_create");
			return 1;
		}
		/* aplica a politica de escalonamento na thread criada */
		ret = pthread_setschedparam(threads[t], policy, &param);
		if (ret != 0) {
			fprintf(stderr, "pthread_setschedparam(%s): %s\n",
				argv[3], strerror(ret));
			return 1;
		}
	}

	/* libera todas as threads ao mesmo tempo */
	pthread_barrier_wait(&start);

	for (t = 0; t < nthreads; t++)
		pthread_join(threads[t], NULL);

	print_results(size, nthreads);

	sem_destroy(&mutex);
	free(buffer);
	return 0;
}
