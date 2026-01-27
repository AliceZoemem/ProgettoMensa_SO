#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

#include "ipc.h"
#include "config.h"
#include "stations.h"
#include "stats.h"
#include "util.h"

extern shm_t *shm;
int *operator_pids = NULL;
int *user_pids = NULL;

/* ---------------------------------------------------------
   Funzioni interne
   --------------------------------------------------------- */
void init_ipc(void);
void destroy_ipc(void);
void create_stations(void);
void spawn_workers(void);
void spawn_users(void);
void wait_all_ready(void);
void simulate_days(void);
void start_new_day(int day);
void end_day(int day);
void terminate_simulation(int cause);
void cleanup_and_exit(int code);

/* ---------------------------------------------------------
   MAIN
   --------------------------------------------------------- */
int main() {
    printf("[MENSA] Avvio del processo responsabile...\n");

    /* 2. Inizializza IPC */
    init_ipc();

    /* 1. Carica configurazione */
    if (load_config() < 0) {
        fprintf(stderr, "[MENSA] Errore caricamento configurazione\n");
        exit(EXIT_FAILURE);
    }

    /* 1b. Carica menu */
    if (load_menu() < 0) {
        fprintf(stderr, "[MENSA] Errore caricamento menu\n");
        exit(EXIT_FAILURE);
    }

    /* 3. Crea stazioni */
    create_stations();

    /* 4. Crea operatori */
    spawn_workers();

    /* 5. Crea utenti */
    spawn_users();

    /* 6. Attende che tutti siano pronti */
    wait_all_ready();

    /* 6b. Inizializza il primo giorno prima di sbloccare */
    shm->giorno_corrente = 1;
    stats_reset_day(&shm->stats_giorno);
    stations_assign_workers(shm);
    stations_refill_day(shm);

    /* 6c. Sblocca barriera */
    ipc_release_barrier();

    /* 7. Avvia simulazione */
    simulate_days();

    /* 8. Terminazione */
    cleanup_and_exit(EXIT_SUCCESS);
    return 0;
}

/* ---------------------------------------------------------
   IMPLEMENTAZIONI
   --------------------------------------------------------- */

void init_ipc(void) {
    shm = ipc_create_shared_memory();
    char buf[32];
    sprintf(buf, "%d", shm->shm_id);
    setenv("MENSA_SHMID", buf, 1);

    ipc_create_semaphores();

    ipc_create_message_queues();
}

void destroy_ipc(void) {
    printf("[MENSA] Deallocazione IPC...\n");
    ipc_destroy_message_queues();
    ipc_destroy_semaphores();
    ipc_destroy_shared_memory();
}

void create_stations(void) {
    stations_init(shm);
}

void spawn_workers(void) {
    operator_pids = calloc(shm->NOFWORKERS, sizeof(int));

    for (int i = 0; i < shm->NOFWORKERS; i++) {

        pid_t pid = fork();
        if (pid == 0) {

            /* Figlio → exec operatore */
            char idbuf[16], stbuf[16];
            sprintf(idbuf, "%d", i);

            /* Per ora assegniamo round‑robin, poi stations_assign_workers() farà il mapping reale */
            sprintf(stbuf, "%d", i % 4);

            char *args[] = { "operatore", idbuf, stbuf, NULL };
            execve("./operatore", args, environ);

            perror("exec operatore");
            exit(EXIT_FAILURE);
        }

        operator_pids[i] = pid;
    }
}

void spawn_users(void) {
    //printf("[MENSA] Creazione utenti...\n");
    user_pids = calloc(shm->NOFUSERS, sizeof(int));

    for (int i = 0; i < shm->NOFUSERS; i++) {

        pid_t pid = fork();
        if (pid == 0) {

            char idbuf[16];
            sprintf(idbuf, "%d", i);

            char *args[] = { "utente", idbuf, NULL };
            execve("./utente", args, environ);

            perror("exec utente");
            exit(EXIT_FAILURE);
        }

        user_pids[i] = pid;
    }
}

void wait_all_ready(void) {
    //printf("[MENSA] Attesa inizializzazione di operatori e utenti...\n");

    ipc_wait_barrier();

    printf("[MENSA] Tutti i processi sono pronti. Avvio simulazione.\n");
}

void simulate_days(void) {

    printf("[MENSA] Simulazione per %d giorni...\n", shm->SIMDURATION);

    for (int day = 1; day <= shm->SIMDURATION; day++) {

        start_new_day(day);

        /* Simulazione del giorno:
           1 minuto = NNANOSECS nanosecondi
           un giorno = 240 minuti -> 4h di lavoro
        */
        long day_ns = shm->NNANOSECS * 60 * 240;

        nanosleep(&(struct timespec){
            .tv_sec = day_ns / 1000000000,
            .tv_nsec = day_ns % 1000000000
        }, NULL);

        end_day(day);

        /* Controllo overload */
        if (shm->stats_giorno.utenti_in_attesa > shm->OVERLOADTHRESHOLD) {
            terminate_simulation(1);
        }
    }

    terminate_simulation(0);
}

void start_new_day(int day) {
    printf("\n[MENSA] --- Inizio giorno %d ---\n", day);

    /* Per i giorni successivi al primo */
    if (day > 1) {
        shm->giorno_corrente = day;
        stats_reset_day(&shm->stats_giorno);
        stations_assign_workers(shm);
        stations_refill_day(shm);
    }

    /* Attiva simulazione e sblocca operatori/utenti */
    shm->simulation_running = 1;
    int total = shm->NOFWORKERS + shm->NOFUSERS;
    for (int i = 0; i < total; i++) {
        sem_post(&shm->sem_day_start);
    }
}

void end_day(int day) {
    printf("[MENSA] Fine giorno %d\n", day);
    stats_print_day(&shm->stats_giorno, day);

    stats_update_totals(&shm->stats_tot, &shm->stats_giorno);
    shm->simulation_running = 0;
}

void terminate_simulation(int cause) {

    shm->terminazione_causa = cause;
    shm->simulation_running = 0;

    printf("\n[MENSA] Terminazione simulazione: %s\n",
           cause == 0 ? "COMPLETATA" : "OVERLOAD");

    stats_print_final(&shm->stats_tot);

    /* Sblocca eventuali processi in attesa */
    int total = shm->NOFWORKERS + shm->NOFUSERS;
    for (int i = 0; i < total; i++) {
        sem_post(&shm->sem_day_start);
    }

    /* Breve pausa per permettere ai processi di terminare */
    sleep(1);

    cleanup_and_exit(EXIT_SUCCESS);
}

void cleanup_and_exit(int code) {
    printf("[MENSA] Terminazione processi figli...\n");

    for (int i = 0; i < shm->NOFWORKERS; i++)
        kill(operator_pids[i], SIGTERM);

    for (int i = 0; i < shm->NOFUSERS; i++)
        kill(user_pids[i], SIGTERM);

    while (wait(NULL) > 0);

    destroy_ipc();
    exit(code);
}