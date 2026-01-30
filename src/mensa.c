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
#include <sys/msg.h>

extern shm_t *shm;
int *operator_pids = NULL;
int *user_pids = NULL;

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

int main(int argc, char *argv[]) {
    printf("[MENSA] Avvio del processo responsabile...\n");
    init_ipc();

    if (argc > 1) {
        printf("[MENSA] Usando file di configurazione: %s\n", argv[1]);
        if (load_config_from_file(argv[1]) < 0) {
            fprintf(stderr, "[MENSA] Errore caricamento configurazione\n");
            exit(EXIT_FAILURE);
        }
    } else {
        if (load_config() < 0) {
            fprintf(stderr, "[MENSA] Errore caricamento configurazione\n");
            exit(EXIT_FAILURE);
        }
    }

    if (load_menu() < 0) {
        fprintf(stderr, "[MENSA] Errore caricamento menu\n");
        exit(EXIT_FAILURE);
    }

    /* Inizializza semaforo tavoli dopo aver caricato la configurazione */
    ipc_init_table_semaphore();

    create_stations();

    spawn_workers();

    spawn_users();

    wait_all_ready();

    shm->giorno_corrente = 1;
    stats_reset_day(&shm->stats_giorno);
    stations_assign_workers(shm);
    stations_refill_day(shm);

    ipc_release_barrier();

    simulate_days();

    cleanup_and_exit(EXIT_SUCCESS);
    return 0;
}

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
            char idbuf[16], stbuf[16];
            sprintf(idbuf, "%d", i);
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
    printf("[MENSA] Attesa inizializzazione di operatori e utenti...\n");

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
           Refill periodico ogni 10 minuti
        */
        long total_minutes = 240;  // 4 ore = 240 minuti
        long refill_interval = 10;
        long minute_ns = shm->NNANOSECS * 60;
        long refill_interval_ns = minute_ns * refill_interval;

        for (long elapsed_minutes = 0; elapsed_minutes < total_minutes; elapsed_minutes += refill_interval) {
            nanosleep(&(struct timespec){
                .tv_sec = refill_interval_ns / 1000000000,
                .tv_nsec = refill_interval_ns % 1000000000
            }, NULL);

            if (shm->simulation_running) {
                stations_refill_periodic(shm);
            }
        }

        end_day(day);
        
        /* Controllo overload: troppi utenti in attesa */
        if (shm->stats_giorno.utenti_in_attesa > shm->OVERLOADTHRESHOLD) {
            printf("\n[MENSA] OVERLOAD RILEVATO!\n");
            printf("[MENSA] Utenti in attesa: %d, Soglia: %d\n", 
                   shm->stats_giorno.utenti_in_attesa, shm->OVERLOADTHRESHOLD);
            terminate_simulation(1);  // 1 = OVERLOAD
            return;
        }
    }

    /* Completamento normale: timeout raggiunto */
    terminate_simulation(0);  // 0 = TIMEOUT
}

void clear_message_queues(void) {
    struct {
        long mtype;
        char mtext[1024];
    } dummy_msg;
    
    int queues[] = {shm->msgid_primi, shm->msgid_secondi, shm->msgid_coffee, shm->msgid_cassa};
    
    for (int i = 0; i < 4; i++) {
        while (msgrcv(queues[i], &dummy_msg, sizeof(dummy_msg.mtext), 0, IPC_NOWAIT | MSG_NOERROR) >= 0) {
        }
    }
}

void start_new_day(int day) {
    printf("\n[MENSA] --- Inizio giorno %d ---\n", day);
    clear_message_queues();

    if (day > 1) {
        shm->giorno_corrente = day;
        stats_reset_day(&shm->stats_giorno);
        stations_assign_workers(shm);
        stations_refill_day(shm);
    }

    shm->stats_giorno.operatori_attivi = shm->NOFWORKERS;
    shm->day_barrier_count = 0;  
    shm->simulation_running = 1;
    int total = shm->NOFWORKERS + shm->NOFUSERS;
    for (int i = 0; i < total; i++) {
        sem_post(&shm->sem_day_start);
    }
}

void end_day(int day) {
    printf("[MENSA] Fine giorno %d\n", day);
    shm->simulation_running = 0;
    
    /* Attende con timeout per evitare deadlock */
    int wait_cycles = 0;
    while (shm->day_barrier_count < shm->NOFUSERS && wait_cycles < 100) {
        nanosleep(&(struct timespec){0, 50000000}, NULL);
        wait_cycles++;
    }
    
    printf("[MENSA] Tutti gli utenti hanno completato il giorno\n");
    nanosleep(&(struct timespec){0, 100000000}, NULL);
    
    printf("[MENSA] Utenti in attesa a fine giornata: %d\n", shm->stats_giorno.utenti_in_attesa);
    
    if (shm->stats_giorno.utenti_in_attesa > 0) {
        printf("[MENSA] ATTENZIONE: %d utenti non hanno completato il servizio\n", 
               shm->stats_giorno.utenti_in_attesa);
    }
    stations_compute_leftovers(shm);
    
    stats_print_day(&shm->stats_giorno, day);
    stats_update_totals(&shm->stats_tot, &shm->stats_giorno);
}

void terminate_simulation(int cause) {

    shm->terminazione_causa = cause;
    shm->simulation_running = 0;

    printf("\n========================================================\n");
    printf("          TERMINAZIONE SIMULAZIONE\n");
    printf("========================================================\n");
    
    if (cause == 0) {
        printf("CAUSA: TIMEOUT - Durata simulazione completata\n");
        printf("Giorni simulati: %d/%d\n", shm->giorno_corrente, shm->SIMDURATION);
    } else {
        printf("CAUSA: OVERLOAD - Troppi utenti in attesa\n");
        printf("Utenti in attesa: %d (soglia: %d)\n", 
               shm->stats_giorno.utenti_in_attesa, shm->OVERLOADTHRESHOLD);
        printf("Giorno di interruzione: %d/%d\n", 
               shm->giorno_corrente, shm->SIMDURATION);
    }
    
    printf("========================================================\n\n");

    stats_print_final(&shm->stats_tot, shm->giorno_corrente);

    /* Sblocca eventuali processi in attesa */
    int total = shm->NOFWORKERS + shm->NOFUSERS;
    for (int i = 0; i < total; i++) {
        sem_post(&shm->sem_day_start);
    }

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