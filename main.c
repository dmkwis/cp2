#include "cacti.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define ACTOR_SIZE sizeof(actor)
#define UNUSED(x) (void)(x)
#define MSG_SON 1
#define MSG_CALCULATE 3
#define MSG_SAVE_COLUMN_AND_SPAWN 2
#define MSG_CREATE_SYSTEM 4
#define MSG_CALCULATE_ALL 5

void hello_origin(void **stateptr, size_t size, void *data);

void get_son_id(void **stateptr, size_t size, void *data);

void save_col_and_spawn(void **stateptr, size_t size, void *data);

void hello(void **stateptr, size_t size, void *data);

void calculate(void **stateptr, size_t size, void *data);

void create_system(void **stateptr, size_t size, void *data);

void calculate_all(void **stateptr, size_t size, void *data);

typedef long element_t;

act_t propmts[4] = {hello, get_son_id, save_col_and_spawn, calculate};

role_t role = {
        .prompts = propmts,
        .nprompts = 4
};

act_t prompts_origin[6] = {hello_origin, get_son_id, save_col_and_spawn, calculate, create_system,
                           calculate_all};

role_t role_father = {
        .prompts = prompts_origin,
        .nprompts = 6
};

typedef struct actor_info {
    actor_id_t next;
    int col;
    actor_id_t origin;
    element_t **macierz;
    int **milisec;
    int n;
    int k;
} actor;

typedef struct row_info {
    int row;
    element_t sum;
} row_t;

void hello(void **stateptr, size_t size, void *data) {
    UNUSED(size);
    *stateptr = malloc(ACTOR_SIZE);
    message_t get_son = {
            .message_type = MSG_SON,
            .data = (void *) actor_id_self()
    };
    send_message((actor_id_t) data, get_son);
}

void hello_origin(void **stateptr, size_t size, void *data) {
    UNUSED(stateptr);
    UNUSED(size);
    UNUSED(data);
}

void get_son_id(void **stateptr, size_t size, void *data) {
    UNUSED(size);
    actor *act = *stateptr;
    // printf("Hello i am %ldth, my son is %ldth\n", act->col, act->next);
    act->next = (actor_id_t) data;
    message_t spawn_son = {
            .message_type = MSG_SAVE_COLUMN_AND_SPAWN,
            .data = *stateptr
    };
    send_message((actor_id_t) data, spawn_son);
}

// Dostaje numer kolumny w data
void save_col_and_spawn(void **stateptr, size_t size, void *data) {
    UNUSED(size);

    actor *act = *stateptr;
    actor *father = data;
    act->col = father->col + 1;
    act->milisec = father->milisec;
    act->macierz = father->macierz;
    act->n = father->n;
    act->k = father->k;
    act->origin = father->origin;
    if (act->col < father->n - 1) {
        message_t spawn = {
                .message_type = MSG_SPAWN,
                .data = &role
        };
        send_message(actor_id_self(), spawn);
    } else {
        message_t calc_all = {
                .message_type = MSG_CALCULATE_ALL
        };
        send_message(act->origin, calc_all);
    }
}

void create_system(void **stateptr, size_t size, void *data) {
    UNUSED(size);

    *stateptr = malloc(ACTOR_SIZE);
    actor *init = data;
    actor *act = *stateptr;
    act->macierz = init->macierz;
    act->milisec = init->milisec;
    act->n = init->n;
    act->k = init->k;
    act->origin = init->origin;
    init->col = -1;
    message_t spawn_son = {
            .message_type = MSG_SAVE_COLUMN_AND_SPAWN,
            .data = init
    };
    send_message(actor_id_self(), spawn_son);
}

void calculate(void **stateptr, size_t size, void *data) {
    UNUSED(size);

    row_t *r = data;
    actor *act = *stateptr;
    r->sum += act->macierz[r->row][act->col];
    // printf("CALCULATE jestem %ld liczę wiersz %d który ma sumę %ld\n", act->col, r->row, r->sum);
    usleep(act->milisec[r->row][act->col]);
    if (act->col + 1 < act->n) {
        message_t calculate = {
                .data = r,
                .message_type = MSG_CALCULATE
        };
        actor_id_t next = act->next;
        send_message(next, calculate);
        if (r->row == act->k - 1) {
            free(*stateptr);
            message_t godie = {
                    .message_type = MSG_GODIE,
            };
            send_message(actor_id_self(), godie);
        }
    } else {
        printf("%ld\n", r->sum);
        if (r->row == act->k - 1) {
            free(*stateptr);
            message_t godie = {
                    .message_type = MSG_GODIE,
            };
            send_message(actor_id_self(), godie);
        }
        free(r);
    }
}

void calculate_all(void **stateptr, size_t size, void *data) {
    UNUSED(size);
    UNUSED(data);
    // printf("Wywołano calculate all!\n");
    actor *act = *stateptr;
    for (int i = 0; i < act->k; ++i) {
        row_t *row = malloc(sizeof(row_t));
        row->row = i;
        row->sum = 0;
        message_t calculate = {
                .message_type = MSG_CALCULATE,
                .data = row
        };
        send_message(actor_id_self(), calculate);
    }
}

int main() {
    element_t **macierz;
    int **milisec;

    int k, n;

    scanf("%d", &k);
    scanf("%d", &n);

    macierz = malloc(sizeof(element_t *) * k);
    milisec = malloc(sizeof(int *) * k);
    for (int i = 0; i < k; i++) {
        macierz[i] = malloc(sizeof(element_t) * n);
        milisec[i] = malloc(sizeof(int) * n);
        for (int j = 0; j < n; j++) {
            scanf("%ld", &(macierz[i][j]));
            scanf("%d", &(milisec[i][j]));
        }
    }

    // printf("Dupa\n");

    actor_id_t origin;
    actor_system_create(&origin, &role_father);
    actor init = {
            .macierz = macierz,
            .milisec = milisec,
            .n = n,
            .k = k,
            .origin = origin
    };
    message_t msg = {
            .message_type = MSG_CREATE_SYSTEM,
            .data = &init
    };

    send_message(origin, msg);
    actor_system_join(origin);
    for (int i = 0; i < k; ++i) {
        free(macierz[i]);
        free(milisec[i]);
    }
    if (macierz != NULL)
        free(macierz);
    if (milisec != NULL)
        free(milisec);
}