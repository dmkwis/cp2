#include <stdio.h>
#include "cacti.h"


#define MSG_ACT_PROCESS 1
#define MSG_FROM_SON 2

typedef struct factorial {
    unsigned long long int k_fact;
    unsigned long long int k;
    unsigned long long int n;
} factorial_t;

void main_actor_hello(void **stateptr, size_t nbytes, void *data);
void derived_actor_hello(void **stateptr, size_t nbytes, void *data);
void actor_process(void **stateptr, size_t nbytes, void *data);
void inductive_step(void **stateptr, size_t nbytes, void *data);
act_t main_actor_prompts[3] = {main_actor_hello, actor_process, inductive_step};
act_t derived_actor_prompts[3] = {derived_actor_hello, actor_process, inductive_step};
role_t main_actor_role = {
        .prompts = main_actor_prompts,
        .nprompts = 3
};

role_t derived_actor_role = {
        .prompts = derived_actor_prompts,
        .nprompts = 3
};

message_t go_die_message = {
        .message_type = MSG_GODIE,
        .nbytes = 0,
        .data = NULL,
};

void main_actor_hello(void **stateptr, size_t nbytes, void *data) { }

void derived_actor_hello(void **stateptr, size_t nbytes, void *data) {
    actor_id_t father_id = (actor_id_t)data;
    message_t SON_MESSAGE = {
            .message_type = MSG_FROM_SON,
            .data = (void*)actor_id_self(),
            .nbytes = sizeof(actor_id_t),
    };
    send_message(father_id, SON_MESSAGE);
}

void actor_process(void **stateptr, size_t nbytes, void *data) {
    factorial_t* f = (factorial_t*) data;
    (*stateptr) = data;
    if(f->k == f->n) {
        printf("%llu\n", f->k_fact);
        send_message(actor_id_self(), go_die_message);
    }
    else {
        message_t SPAWN_MESSAGE = {
                .message_type = MSG_SPAWN,
                .nbytes = sizeof(derived_actor_role),
                .data = &derived_actor_role
        };
        send_message(actor_id_self(), SPAWN_MESSAGE);
    }
}

void inductive_step(void **stateptr, size_t nbytes, void *data) {
    actor_id_t son_id = (actor_id_t)data;
    factorial_t* next_factorial = (factorial_t*)(*stateptr);
    next_factorial->k++;
    next_factorial->k_fact *= next_factorial->k;
    message_t inductive_message = {
            .message_type = MSG_ACT_PROCESS,
            .nbytes = sizeof(factorial_t),
            .data = next_factorial,
    };
    send_message(son_id, inductive_message);
    send_message(actor_id_self(), go_die_message);
}

int main() {
    int n;
    scanf("%d", &n);
    actor_id_t main_actor;
    factorial_t factorial_set_up = {
            .k_fact = 1,
            .k = 0,
            .n = n,
    };
    actor_system_create(&main_actor, &main_actor_role);
    message_t message = {
            .data = &factorial_set_up,
            .nbytes = sizeof(factorial_t),
            .message_type = MSG_ACT_PROCESS,
    };
    send_message(main_actor, message);
    actor_system_join(main_actor);
}