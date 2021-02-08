#include "cacti.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define MSG_FROM_SON 1
#define MSG_SPAWN_RECURSIVELY 2
#define MSG_START_CALCULATION 3
#define MSG_CREATE_ROW_STRUCTURES 4

typedef struct to_calculate {
    long value;
    int sleep;
} to_calculate_t;

typedef struct actor_info {
    actor_id_t next;
    int col_num;
} actor_info_t;

typedef struct row_info {
    int row_num;
    long sum;
} row_info_t;

message_t go_die = {
        .message_type = MSG_GODIE,
        .nbytes = 0,
        .data = NULL
};

to_calculate_t **matrix;
int k, n;
actor_id_t main_actor;

void main_actor_hello(void **stateptr, size_t size, void *data);

void assign_son(void **stateptr, size_t size, void *data);

void spawn_recursively(void **stateptr, size_t size, void *data);

void derived_actor_hello(void **stateptr, size_t size, void *data);

void calculate_sum(void **stateptr, size_t size, void *data);

void create_row_structures(void **stateptr, size_t size, void *data);

act_t derived_actor_prompts[4] = {derived_actor_hello, assign_son, spawn_recursively, calculate_sum};

role_t role = {
        .prompts = derived_actor_prompts,
        .nprompts = 4
};

act_t main_actor_prompts[5] = {main_actor_hello, assign_son, spawn_recursively, calculate_sum, create_row_structures};

role_t role_father = {
        .prompts = main_actor_prompts,
        .nprompts = 5
};

void derived_actor_hello(void **stateptr, size_t size, void *data) {
    *stateptr = malloc(sizeof(actor_info_t));
    actor_id_t father_id = (actor_id_t) data;
    message_t message_from_son = {
            .message_type = MSG_FROM_SON,
            .data = (void *) actor_id_self()
    };
    send_message(father_id, message_from_son);
}

void main_actor_hello(void **stateptr, size_t size, void *data) {
    *stateptr = malloc(sizeof(actor_info_t));
    actor_info_t *this_actor_info = *stateptr;
    this_actor_info->col_num = -1;
    message_t spawn_son = {
            .message_type = MSG_SPAWN_RECURSIVELY,
            .data = this_actor_info
    };
    send_message(actor_id_self(), spawn_son);
}

void assign_son(void **stateptr, size_t size, void *data) {
    actor_info_t *actor_info = *stateptr;
    actor_info->next = (actor_id_t) data;
    message_t spawn_son_message = {
            .message_type = MSG_SPAWN_RECURSIVELY,
            .data = *stateptr
    };
    send_message((actor_id_t) data, spawn_son_message);
}

void spawn_recursively(void **stateptr, size_t size, void *data) {
    actor_info_t *this_actor_info = *stateptr;
    actor_info_t *main_actor_info = data;
    this_actor_info->col_num = main_actor_info->col_num + 1;
    if (this_actor_info->col_num < n - 1) {
        message_t spawn_message = {
                .message_type = MSG_SPAWN,
                .nbytes = sizeof(role_t),
                .data = &role,
        };
        send_message(actor_id_self(), spawn_message);
    } else {
        message_t create_row_structures_message = {
                .message_type = MSG_CREATE_ROW_STRUCTURES,
                .nbytes = 0,
                .data = NULL,
        };
        send_message(main_actor, create_row_structures_message);
    }
}

void calculate_sum(void **stateptr, size_t size, void *data) {
    row_info_t *row_info = data;
    actor_info_t *actor_info = *stateptr;
    row_info->sum += matrix[row_info->row_num][actor_info->col_num].value;
    usleep(matrix[row_info->row_num][actor_info->col_num].sleep * 1000);
    if (actor_info->col_num + 1 < n) {
        message_t calculate = {
                .data = row_info,
                .message_type = MSG_START_CALCULATION
        };
        actor_id_t next = actor_info->next;
        send_message(next, calculate);
        if (row_info->row_num == k - 1) {
            free(*stateptr);

            send_message(actor_id_self(), go_die);
        }
    } else {
        printf("%ld\n", row_info->sum);
        if (row_info->row_num == k - 1) {
            free(*stateptr);
            send_message(actor_id_self(), go_die);
        }
        free(row_info);
    }
}

void create_row_structures(void **stateptr, size_t size, void *data) {
    for (int i = 0; i < k; ++i) {
        row_info_t *row_info = malloc(sizeof(row_info_t));
        row_info->row_num = i;
        row_info->sum = 0;
        message_t calculate = {
                .message_type = MSG_START_CALCULATION,
                .data = row_info
        };
        send_message(actor_id_self(), calculate);
    }
}

int main() {
    scanf("%d", &k);
    scanf("%d", &n);
    matrix = malloc(sizeof(to_calculate_t *) * k);
    if(matrix == NULL) {
        exit(1);
    }
    for (int i = 0; i < k; i++) {
        matrix[i] = malloc(sizeof(to_calculate_t) * n);
        if(matrix[i] == NULL) {
            exit(1);
        }
        for (int j = 0; j < n; j++) {
            scanf("%ld %d", &matrix[i][j].value, &matrix[i][j].sleep);
        }
    }
    actor_id_t main_actor;
    actor_system_create(&main_actor, &role_father);
    actor_system_join(main_actor);
    for (int i = 0; i < k; ++i) {
        free(matrix[i]);
    }
    free(matrix);
    return 0;
}