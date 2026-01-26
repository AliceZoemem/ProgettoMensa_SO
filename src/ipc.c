#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <unistd.h>
#include <errno.h>
#include "shared_structs.h"
#include "ipc.h"


/* ---------------------------------------------------------
   Variabili IPC globali interne al modulo
   --------------------------------------------------------- */
static int shm_id = -1;
shm_t *shm = NULL;

/* ---------------------------------------------------------
   SHARED MEMORY
   --------------------------------------------------------- */
shm_t *ipc_create_shared_memory(void) {

    shm_id = shmget(IPC_PRIVATE, sizeof(shm_t), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("[IPC] shmget");
        exit(EXIT_FAILURE);
    }

    shm_t *ptr = shmat(shm_id, NULL, 0);
    if (ptr == (void *) -1) {
        perror("[IPC] shmat");
        exit(EXIT_FAILURE);
    }

    memset(ptr, 0, sizeof(shm_t));

    /* Salviamo l’ID in shared memory per gli altri processi */
    ptr->shm_id = shm_id;

    return ptr;
}

shm_t *ipc_attach_shared_memory(void) {

    if (shm_id < 0) {
        // Gli altri processi leggono l’ID dalla variabile globale
        // che mensa.c passa tramite execve
        // oppure da una variabile d’ambiente
        char *env = getenv("MENSA_SHMID");
        if (!env) {
            fprintf(stderr, "[IPC] Errore: MENSA_SHMID non impostato\n");
            exit(EXIT_FAILURE);
        }
        shm_id = atoi(env);
    }

    shm_t *ptr = shmat(shm_id, NULL, 0);
    if (ptr == (void *) -1) {
        perror("[IPC] shmat attach");
        exit(EXIT_FAILURE);
    }

    return ptr;
}

void ipc_destroy_shared_memory(void) {
    if (shm_id >= 0)
        shmctl(shm_id, IPC_RMID, NULL);
}

/* ---------------------------------------------------------
   SEMAFORI
   --------------------------------------------------------- */
static void init_station_semaphore(station_t *st) {
    if (sem_init(&st->mutex, 1, 1) < 0) {
        perror("[IPC] sem_init station mutex");
        exit(EXIT_FAILURE);
    }
}

void ipc_create_semaphores(void) {

    /* Semaforo tavoli */
    if (sem_init(&shm->sem_tavoli, 1, shm->NOFTABLESEATS) < 0) {
        perror("[IPC] sem_init tavoli");
        exit(EXIT_FAILURE);
    }

    /* Barriera */
    if (sem_init(&shm->sem_barrier, 1, 0) < 0) {
        perror("[IPC] sem_init barrier");
        exit(EXIT_FAILURE);
    }

    /* Semaforo inizio giornata */
    if (sem_init(&shm->sem_day_start, 1, 0) < 0) {
        perror("[IPC] sem_init day_start");
        exit(EXIT_FAILURE);
    }

    /* Inizializza flag simulazione */
    shm->simulation_running = 0;

    /* Mutex stazioni */
    init_station_semaphore(&shm->st_primi);
    init_station_semaphore(&shm->st_secondi);
    init_station_semaphore(&shm->st_coffee);
    init_station_semaphore(&shm->st_cassa);
}

void ipc_destroy_semaphores(void) {
    sem_destroy(&shm->sem_tavoli);
    sem_destroy(&shm->sem_barrier);
    sem_destroy(&shm->sem_day_start);

    sem_destroy(&shm->st_primi.mutex);
    sem_destroy(&shm->st_secondi.mutex);
    sem_destroy(&shm->st_coffee.mutex);
    sem_destroy(&shm->st_cassa.mutex);
}

/* ---------------------------------------------------------
   MESSAGE QUEUES
   --------------------------------------------------------- */
void ipc_create_message_queues(void) {

    shm->msgid_primi   = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
    shm->msgid_secondi = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
    shm->msgid_coffee  = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
    shm->msgid_cassa   = msgget(IPC_PRIVATE, IPC_CREAT | 0666);

    if (shm->msgid_primi < 0 || shm->msgid_secondi < 0 ||
        shm->msgid_coffee < 0 || shm->msgid_cassa < 0) {
        perror("[IPC] msgget");
        exit(EXIT_FAILURE);
    }
}

void ipc_destroy_message_queues(void) {
    msgctl(shm->msgid_primi,   IPC_RMID, NULL);
    msgctl(shm->msgid_secondi, IPC_RMID, NULL);
    msgctl(shm->msgid_coffee,  IPC_RMID, NULL);
    msgctl(shm->msgid_cassa,   IPC_RMID, NULL);
}

/* ---------------------------------------------------------
   BARRIERA DI AVVIO
   --------------------------------------------------------- */
void ipc_signal_ready(void) {
    __sync_fetch_and_add(&shm->ready_count, 1);
}

void ipc_wait_barrier(void) {

    int total = shm->NOFWORKERS + shm->NOFUSERS;

    /* Attesa attiva evitata: nanosleep */
    while (shm->ready_count < total) {
        struct timespec t = {0, 1000000}; // 1ms
        nanosleep(&t, NULL);
    }
}

void ipc_release_barrier(void) {
    int total = shm->NOFWORKERS + shm->NOFUSERS;
    
    /* Sblocca tutti i processi */
    for (int i = 0; i < total; i++)
        sem_post(&shm->sem_barrier);
}