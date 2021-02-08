#include "thread_pool.h"
#include "cyclic_buffer.h"
#include "allocation_constants.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

struct thread_pool {
    unsigned long *id_of_thread;
    long *actor_num;
    pthread_t *threads;
    int size;
};

int create_thread_pool(thread_pool_t *thread_pool, int pool_size, void* (*thread_instructions)(void*)) {
    *thread_pool = malloc(sizeof(struct thread_pool));
    if(*thread_pool == NULL) {
        return ALLOC_ERROR;
    }
    (*thread_pool)->size = pool_size;
    (*thread_pool)->threads = malloc(pool_size * sizeof(pthread_t));
    (*thread_pool)->id_of_thread = malloc(pool_size * sizeof(unsigned long));
    (*thread_pool)->actor_num = malloc(pool_size * sizeof(long));
    if((*thread_pool)->threads == NULL)
        return ALLOC_ERROR;
    for(int i = 0; i < pool_size; ++i) {
        pthread_create(&(*thread_pool)->threads[i], NULL, thread_instructions, NULL); //<SAFETY CHECK
        (*thread_pool)->id_of_thread[i] = (*thread_pool)->threads[i];
    }
    return ALLOC_SUCCESS;
}

unsigned int threads_actor(thread_pool_t thread_pool) {
    unsigned long thread_id = pthread_self();
    for(int i = 0; i < thread_pool->size; ++i) {
        if(thread_pool->id_of_thread[i] == thread_id) {
            return thread_pool->actor_num[i];
        }
    }
    exit(1);
}

void set_actor(thread_pool_t thread_pool, long actor_id) {
    unsigned long thread_id = pthread_self();
    for(int i = 0; i < thread_pool->size; ++i) {
        if(thread_pool->id_of_thread[i] == thread_id) {
            thread_pool->actor_num[i] = actor_id;
            return;
        }
    }
    exit(1);
}

void thread_pool_destroy(thread_pool_t *thread_pool) {
    if((*thread_pool) != NULL) {
        //printf("BEFORE JOIN\n"); fflush(stdout);
        for(int i = 0; i < (*thread_pool)->size; ++i) {
            pthread_join((*thread_pool)->threads[i], NULL); //<SAFETY CHECK
        }
        //printf("EVERYONE JOINED, WE HOME\n"); fflush(stdout);
        free((*thread_pool)->threads);
        free((*thread_pool)->id_of_thread);
        free((*thread_pool)->actor_num);
        free(*thread_pool);
        *thread_pool = NULL;
    }
}
