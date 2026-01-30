#ifndef IPC_H
#define IPC_H

#include "shared_structs.h"
extern shm_t *shm;
shm_t *ipc_create_shared_memory(void);
shm_t *ipc_attach_shared_memory(void);

void ipc_destroy_shared_memory(void);

void ipc_create_semaphores(void);
void ipc_init_table_semaphore(void);
void ipc_destroy_semaphores(void);

void ipc_create_message_queues(void);
void ipc_destroy_message_queues(void);

void ipc_signal_ready(void);
void ipc_wait_barrier(void);
void ipc_release_barrier(void);

#endif