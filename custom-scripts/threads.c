/*
 * Tutorial 3.2 - Atividade 2: cinco threads imprimindo letras.
 *
 * Cada thread i imprime a letra 'a' + i em laco infinito. Use com
 * trace-cmd (-e sched_switch) e KernelShark para observar o escalonador.
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define THREADS 5

static void *run(void *arg)
{
	long tid = (long)arg;

	for (;;)
		putchar('a' + tid);

	return NULL;
}

int main(void)
{
	pthread_t threads[THREADS];
	long t;

	for (t = 0; t < THREADS; t++) {
		if (pthread_create(&threads[t], NULL, run, (void *)t) != 0) {
			perror("pthread_create");
			exit(1);
		}
	}

	pthread_exit(NULL);
}
