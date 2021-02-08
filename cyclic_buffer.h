#ifndef CYCLIC_BUFFER_H
#define CYCLIC_BUFFER_H

typedef struct cyclic_buffer* cyclic_buffer_t;

int create_cyclic_buffer(cyclic_buffer_t *cyclic_buffer, unsigned int buffer_size);

void write_to_buffer(cyclic_buffer_t cyclic_buffer, void *task_to_add);

void* read_from_buffer(cyclic_buffer_t cyclic_buffer);

void kill_buffor(cyclic_buffer_t cyclic_buffer);

void cyclic_buffer_destroy(cyclic_buffer_t *cyclic_buffer_ptr);

#endif //CYCLIC_BUFFER_H
