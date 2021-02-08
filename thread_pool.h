#ifndef THREAD_POOL_H
#define THREAD_POOL_H

typedef struct thread_pool* thread_pool_t;

int create_thread_pool(thread_pool_t *thread_pool, int pool_size, void* (*thread_function)(void*));

unsigned int threads_actor(thread_pool_t thread_pool);

void set_actor(thread_pool_t thread_pool, long actor_id);

void thread_pool_destroy(thread_pool_t *thread_pool);

#endif //THREAD_POOL_H
