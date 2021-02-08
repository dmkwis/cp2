#include <stdlib.h>
#include "cyclic_buffer.h"
#include <pthread.h>
#include "allocation_constants.h"
#include <stdbool.h>
#include <stdio.h>

struct cyclic_buffer {
    void **elements;

    pthread_mutex_t mutex;

    pthread_cond_t want_to_read;
    pthread_cond_t want_to_write;

    pthread_mutex_t sys_mutex;

    int free_places;
    int write_ptr;
    int read_ptr;
    int size;

    bool is_dead;
};

int create_cyclic_buffer(cyclic_buffer_t *cyclic_buffer, unsigned int buffer_size) {
    (*cyclic_buffer) = malloc(sizeof(struct cyclic_buffer));
    (*cyclic_buffer)->elements = calloc(buffer_size, sizeof(void*));

    if((*cyclic_buffer)->elements == NULL) {
        return ALLOC_ERROR;
    }

    if(pthread_mutex_init(&(*cyclic_buffer)->sys_mutex, NULL) != 0) {
        return ALLOC_ERROR;
    }

    if(pthread_mutex_init(&(*cyclic_buffer)->mutex, NULL) != 0) {
        return ALLOC_ERROR;
    }

    if(pthread_cond_init(&(*cyclic_buffer)->want_to_read, NULL) != 0) {
        return ALLOC_ERROR;
    }

    if(pthread_cond_init(&(*cyclic_buffer)->want_to_write, NULL) != 0) {
        return ALLOC_ERROR;
    }
    (*cyclic_buffer)->size = buffer_size;
    (*cyclic_buffer)->free_places = buffer_size;
    (*cyclic_buffer)->write_ptr = 0;
    (*cyclic_buffer)->read_ptr = 0;
    (*cyclic_buffer)->is_dead = false;
    return ALLOC_SUCCESS;
}

void kill_buffor(cyclic_buffer_t cyclic_buffer) {
    pthread_mutex_lock(&cyclic_buffer->mutex);
    if(cyclic_buffer != NULL) {
        cyclic_buffer->is_dead = true;
        //printf("BROADCAST COND\n");
        pthread_cond_broadcast(&cyclic_buffer->want_to_read);
        pthread_cond_broadcast(&cyclic_buffer->want_to_write);
    }
    pthread_mutex_unlock(&cyclic_buffer->mutex);
}

void write_to_buffer(cyclic_buffer_t cyclic_buffer, void *task_to_add) {
    pthread_mutex_lock(&cyclic_buffer->mutex);
    while(cyclic_buffer->free_places == 0 && !cyclic_buffer->is_dead) {
        pthread_cond_wait(&cyclic_buffer->want_to_write, &cyclic_buffer->mutex);
    }

    if(cyclic_buffer->is_dead) {
        pthread_mutex_unlock(&cyclic_buffer->mutex);
        free(task_to_add);
        return;
    }


    cyclic_buffer->elements[cyclic_buffer->write_ptr] = task_to_add;
    cyclic_buffer->write_ptr++;
    cyclic_buffer->write_ptr %= cyclic_buffer->size;
    cyclic_buffer->free_places--;

    pthread_cond_signal(&cyclic_buffer->want_to_read);
    pthread_mutex_unlock(&cyclic_buffer->mutex);
}

void* read_from_buffer(cyclic_buffer_t cyclic_buffer) {
    void* ptr_to_return;
    pthread_mutex_lock(&cyclic_buffer->mutex);
    while(cyclic_buffer->free_places == cyclic_buffer->size && !cyclic_buffer->is_dead) {
        pthread_cond_wait(&cyclic_buffer->want_to_read, &cyclic_buffer->mutex);
    }

    if(cyclic_buffer->free_places == cyclic_buffer->size) {
        pthread_mutex_unlock(&cyclic_buffer->mutex);
        return NULL;
    }


    ptr_to_return = cyclic_buffer->elements[cyclic_buffer->read_ptr];
    cyclic_buffer->elements[cyclic_buffer->read_ptr] = NULL;
    cyclic_buffer->read_ptr++;
    cyclic_buffer->read_ptr %= cyclic_buffer->size;
    cyclic_buffer->free_places++;

    pthread_cond_signal(&cyclic_buffer->want_to_write);
    pthread_mutex_unlock(&cyclic_buffer->mutex);

    return ptr_to_return;
}

void cyclic_buffer_destroy(cyclic_buffer_t *cyclic_buffer) {
    if((*cyclic_buffer) != NULL) {
        for(int i = 0; i < (*cyclic_buffer)->size; ++i) {
            if((*cyclic_buffer)->elements[i] != NULL) {
                free((*cyclic_buffer)->elements);
                (*cyclic_buffer)->elements = NULL;
            }
        }
        free((*cyclic_buffer)->elements);
        if(pthread_cond_destroy(&(*cyclic_buffer)->want_to_read))
            exit(1);
        if(pthread_cond_destroy(&(*cyclic_buffer)->want_to_write))
            exit(1);
        if(pthread_mutex_destroy((&(*cyclic_buffer)->mutex)))
            exit(1);
        if(pthread_mutex_destroy((&(*cyclic_buffer)->sys_mutex)))
            exit(1);
        free((*cyclic_buffer));
        *cyclic_buffer = NULL;
    }
}