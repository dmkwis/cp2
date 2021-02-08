#include <stdlib.h>
#include <semaphore.h>
#include <stdbool.h>
#include <signal.h>
#include "cacti.h"
#include "pthread.h"
#include "allocation_constants.h"
#include "cyclic_buffer.h"
#include "thread_pool.h"

#define CYCLIC_BUFFER_SIZE 100

typedef struct actor {
    cyclic_buffer_t actor_queue;
    void *stateptr;
    role_t *roles;
    int messages_on_queue;
    bool is_dead;
    pthread_mutex_t message_mutex;
    pthread_mutex_t protection_mutex;
    pthread_mutex_t thread_mutex;
    pthread_mutex_t atomic_operation_mutex;
} actor_t;

thread_pool_t thread_pool;
cyclic_buffer_t cyclic_buffer;
actor_t **actors;
actor_id_t actors_ptr;
pthread_mutex_t actor_mutex = PTHREAD_MUTEX_INITIALIZER;
long alive_actors;
pthread_mutex_t alive_actors_mutex;
pthread_cond_t join_condition = PTHREAD_COND_INITIALIZER;
pthread_mutex_t system_destroy_mutex = PTHREAD_MUTEX_INITIALIZER;
bool is_system_joined;
bool can_create_actors;

void kill_actors() {
    for(int i = 0; i < actors_ptr; ++i) {
        message_t die_msg = {
                .message_type = MSG_GODIE,
                .nbytes = 0,
                .data = NULL,
        };
        send_message(i, die_msg);
    }
}

void* signal_thread_function() {
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    int sig;
    sigwait(&sigset, &sig);
    if(sig == SIGINT) {
        pthread_mutex_lock(&actor_mutex);
        can_create_actors = false;
        pthread_mutex_unlock(&actor_mutex);
        kill_actors();
        actor_system_join(0);
    }
    exit(1);
}

static pthread_t signal_thread;


bool is_proper_index(long int index) {
    return index < CAST_LIMIT && index >= 0;
}

void increment_alive_actors() {
    pthread_mutex_lock(&alive_actors_mutex);
    ++alive_actors;
    pthread_mutex_unlock(&alive_actors_mutex);
}

void decrement_alive_actors() {
    pthread_mutex_lock(&alive_actors_mutex);
    --alive_actors;
    pthread_mutex_unlock(&alive_actors_mutex);
}

long check_alive_actors() {
    return alive_actors;
}

int create_actor(role_t *roles, actor_id_t *actor_num) {
    if(pthread_mutex_lock(&actor_mutex) != 0) {
        exit(1);
    }
    if(!can_create_actors) {
        pthread_mutex_unlock(&actor_mutex);
        return ALLOC_INT;
    }
    if(!is_proper_index(actors_ptr)) {
        pthread_mutex_unlock(&actor_mutex);
        return ALLOC_ERROR;
    }
    actors[actors_ptr] = malloc(sizeof(actor_t));
    if(actors[actors_ptr] == NULL) {
        pthread_mutex_unlock(&actor_mutex);
        return ALLOC_ERROR;
    }
    create_cyclic_buffer(&actors[actors_ptr]->actor_queue, ACTOR_QUEUE_LIMIT);
    pthread_mutexattr_t Attr;
    if(pthread_mutexattr_init(&Attr) != 0) {
        exit(1);
    }
    if(pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE) != 0) {
        exit(1);
    }

    if(pthread_mutex_init(&actors[actors_ptr]->thread_mutex, NULL) != 0) {
        exit(1);
    }
    if(pthread_mutex_init(&actors[actors_ptr]->message_mutex, NULL) != 0) {
        exit(1);
    }
    if(pthread_mutex_init(&actors[actors_ptr]->protection_mutex, NULL) != 0) {
        exit(1);
    }
    if(pthread_mutex_init(&actors[actors_ptr]->atomic_operation_mutex, NULL) != 0) {
        exit(1);
    }
    actors[actors_ptr]->roles = roles;
    actors[actors_ptr]->messages_on_queue = 0;
    actors[actors_ptr]->is_dead = false;
    actors[actors_ptr]->stateptr = NULL;
    (*actor_num) = actors_ptr;
    increment_alive_actors();
    ++actors_ptr;
    if(pthread_mutex_unlock(&actor_mutex) != 0) {
        exit(1);
    }
    return ALLOC_SUCCESS;
}

void put_id_to_buffer(actor_id_t id) {
    actor_id_t *new_id = malloc(sizeof(actor_id_t));
    *new_id = id;
    write_to_buffer(cyclic_buffer, new_id);
}

void *thread_instructions() {
    while(true) {
        actor_id_t *actor_id;

        actor_id = read_from_buffer(cyclic_buffer);

        if(actor_id == NULL) {
            return NULL;
        }

        actor_t *current_actor = actors[*actor_id];

        pthread_mutex_lock(&current_actor->thread_mutex);

        set_actor(thread_pool, *actor_id);
       // printf("ZCZYTUJE KOMUNIKAT DO AKTORA %ld\n", *actor_id);
        message_t *message_to_perform = (message_t*)read_from_buffer(current_actor->actor_queue);

        if(message_to_perform == NULL) {
            pthread_mutex_unlock(&current_actor->thread_mutex);
            //printf("pro8l3m ZCZYTANY NULL W AKTORZE %ld\n", *actor_id); fflush(stdout);
            return NULL;
        }

        message_type_t type = message_to_perform->message_type;

        if(type == MSG_GODIE) {
            decrement_alive_actors();
            if(check_alive_actors() == 0) {
                pthread_cond_broadcast(&join_condition);
            }
        }
        else {
            pthread_mutex_lock(&current_actor->atomic_operation_mutex);
            (current_actor->roles->prompts[type])(&current_actor->stateptr, message_to_perform->nbytes, message_to_perform->data);
            pthread_mutex_unlock(&current_actor->atomic_operation_mutex);
        }
        pthread_mutex_lock(&current_actor->protection_mutex);
        current_actor->messages_on_queue--;
        if(current_actor->messages_on_queue > 0) {
            put_id_to_buffer(*actor_id);
        }
        pthread_mutex_unlock(&current_actor->protection_mutex);

        free(actor_id);
        free(message_to_perform);

        pthread_mutex_unlock(&current_actor->thread_mutex);
    }
}

int actor_system_create(actor_id_t *actor, role_t *const role) {
    alive_actors = 0;
    can_create_actors = true;
    actors = calloc(CAST_LIMIT, sizeof(actor_t *));
    if(actors == NULL) {
        exit(1);
    }
    actors_ptr = 0;

    if(pthread_cond_init(&join_condition, NULL) != 0) {
        exit(1);
    }

    int thread_pool_alloc_state = create_thread_pool(&thread_pool, POOL_SIZE, &thread_instructions);
    if(thread_pool_alloc_state == ALLOC_ERROR) {
       exit(1);
    }
    pthread_mutexattr_t Attr;
    if(pthread_mutexattr_init(&Attr) != 0) {
        exit(1);
    }
    if(pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE) != 0) {
        exit(1);
    }

    if(pthread_mutex_init(&alive_actors_mutex, &Attr) != 0) {
        exit(1);
    }
    int cyclic_buffor_alloc_state = create_cyclic_buffer(&cyclic_buffer, CYCLIC_BUFFER_SIZE);
    if(cyclic_buffor_alloc_state == ALLOC_ERROR) {
        exit(1);
    }
    int actor_alloc_state = create_actor(role, actor);
    message_t first_msg = {
            .message_type = MSG_HELLO,
            .nbytes = 0,
            .data = NULL
    };
    send_message(*actor, first_msg);
    if(actor_alloc_state == ALLOC_ERROR) {
        exit(1);
    }

    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigprocmask(SIG_BLOCK, &sigset, NULL);
    if(pthread_create(&signal_thread, NULL, &signal_thread_function, NULL)) {
        exit(1);
    }
    pthread_detach(signal_thread);
    is_system_joined = false;
    return 0;
}

bool is_valid_actor(actor_id_t actor_id) {
    if(actor_id < 0 || actor_id >= CAST_LIMIT || actors[actor_id] == NULL) {
        return false;
    }
    else {
        return true;
    }
}

bool is_dead_actor(actor_id_t actor_id) {
    return actors[actor_id]->is_dead;
}

void create_message(message_t **message_ptr, message_type_t msg_type, size_t nbytes, void* data) {
    (*message_ptr) = malloc(sizeof(message_t));
    if(*message_ptr == NULL) {
        return;
    }
    (*message_ptr)->message_type = msg_type;
    (*message_ptr)->nbytes = nbytes;
    (*message_ptr)->data = data;
}



int send_message(actor_id_t actor, message_t message) {
    if(!is_valid_actor(actor)) {
        return -2;
    }
    else {
        actor_t *current_actor = actors[actor];
        pthread_mutex_lock(&current_actor->message_mutex);
        pthread_mutex_lock(&current_actor->protection_mutex);
        if(is_dead_actor(actor)) {
            pthread_mutex_unlock(&current_actor->message_mutex);
            pthread_mutex_unlock(&current_actor->protection_mutex);
            return -1;
        }
        pthread_mutex_unlock(&current_actor->protection_mutex);

        message_type_t message_type = message.message_type;
        if(message_type == MSG_GODIE) {
            message_t *new_message;
            create_message(&new_message, MSG_GODIE, 0, NULL);
            if(new_message == NULL) {
                exit(1);
            }

            pthread_mutex_lock(&current_actor->protection_mutex);
            current_actor->is_dead = true;
            if(current_actor->messages_on_queue == 0) {
                put_id_to_buffer(actor);
            }
            current_actor->messages_on_queue++;
            pthread_mutex_unlock(&current_actor->protection_mutex);


            write_to_buffer(current_actor->actor_queue, new_message);
        }
        else if(message_type == MSG_SPAWN) {
            actor_id_t actor_num;
            if(create_actor(message.data, &actor_num) == ALLOC_ERROR) {
                exit(1);
            }
            message_t *new_message;
            create_message(&new_message, MSG_HELLO, sizeof(actor_id_t), (void*)actor);
            if(new_message == NULL) {
                exit(1);
            }

            pthread_mutex_lock(&actors[actor_num]->protection_mutex);
            if(actors[actor_num]->messages_on_queue == 0) {
                put_id_to_buffer(actor_num);
            }
            actors[actor_num]->messages_on_queue++;
            pthread_mutex_unlock(&actors[actor_num]->protection_mutex);

            write_to_buffer(actors[actor_num]->actor_queue, new_message);
        }
        else {
            message_t *new_message;
            create_message(&new_message, message_type, message.nbytes, message.data);

            pthread_mutex_lock(&current_actor->protection_mutex);
            if(current_actor->messages_on_queue == 0) {
                put_id_to_buffer(actor);
            }
            current_actor->messages_on_queue++;
            pthread_mutex_unlock(&current_actor->protection_mutex);


            write_to_buffer(current_actor->actor_queue, new_message);
        }
        pthread_mutex_unlock(&current_actor->message_mutex);
        return 0;
    }
}

actor_id_t actor_id_self() {
    return threads_actor(thread_pool);
}

void destroy_actor(actor_id_t actor){
    if(actors != NULL && actors[actor] != NULL) {
        actor_t* current_actor = actors[actor];
        kill_buffor(current_actor->actor_queue);
        cyclic_buffer_destroy(&current_actor->actor_queue);
        pthread_mutex_destroy(&current_actor->protection_mutex);
        pthread_mutex_destroy(&current_actor->thread_mutex);
        pthread_mutex_destroy(&current_actor->message_mutex);
        free(current_actor);
        actors[actor] = NULL;
    }
}
void actor_system_join(actor_id_t actor) {
    //printf(">>>>>>inside actor system join<<<<<<\n"); fflush(stdout);
    pthread_mutex_lock(&alive_actors_mutex);
    if(!(actor >= 0 && actor < actors_ptr)) {
        pthread_mutex_unlock(&alive_actors_mutex);
        return;
    }

    while(check_alive_actors() != 0) {
        if(pthread_cond_wait(&join_condition, &alive_actors_mutex) != 0) {
            exit(1);
        }
    }

    pthread_mutex_lock(&system_destroy_mutex);
    if(!is_system_joined) {
        kill_buffor(cyclic_buffer);
        thread_pool_destroy(&thread_pool);
        cyclic_buffer_destroy(&cyclic_buffer);
        for (int i = 0; i < actors_ptr; ++i) {
            destroy_actor(i);
        }
        if (actors != NULL) {
            free(actors);
            actors = NULL;
        }
        sigset_t sigset;
        sigfillset(&sigset);
        sigprocmask(SIG_UNBLOCK, &sigset, NULL);
        is_system_joined = true;
    }
    pthread_cond_broadcast(&join_condition);
    pthread_mutex_unlock(&alive_actors_mutex);
    pthread_mutex_unlock(&system_destroy_mutex);
}


