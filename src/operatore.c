#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/msg.h>

#include "shared_structs.h"
#include "ipc.h"
#include "util.h"

extern shm_t *shm;
/* Parametri operatore */
static int operator_id = -1;
static int station_type = -1;   // 0=primi, 1=secondi, 2=coffee, 3=cassa

/* Stato operatore */
static int pause_count = 0;

/* ---------------------------------------------------------
   Funzioni interne
   --------------------------------------------------------- */
void operator_init(int id, int st_type);
void operator_loop(void);
int  acquire_station_post(void);
void release_station_post(void);
void handle_pause(void);
void serve_user(void);
int  get_msg_queue(void);
long get_service_time_ns(void);
void update_stats_on_service(msg_request_t *req, msg_response_t *res);

/* ---------------------------------------------------------
   MAIN
   --------------------------------------------------------- */
int main(int argc, char *argv[]) {

    if (argc < 3) {
        fprintf(stderr, "[OPERATORE] Uso: operatore <id> <station_type>\n");
        exit(EXIT_FAILURE);
    }

    operator_id  = atoi(argv[1]);
    station_type = atoi(argv[2]);

    /* Attacca shared memory */
    shm = ipc_attach_shared_memory();

    /* Segnala che l’operatore è pronto */
    ipc_signal_ready();

    /* Attende la barriera */
    sem_wait(&shm->sem_barrier);

    operator_init(operator_id, station_type);
    operator_loop();

    return 0;
}

/* ---------------------------------------------------------
   Inizializzazione operatore
   --------------------------------------------------------- */
void operator_init(int id, int st_type) {
    printf("[OPERATORE %d] Avviato su stazione %d\n", id, st_type);
    srand(time(NULL) ^ (getpid()<<16));
}

/* ---------------------------------------------------------
   Ciclo principale dell’operatore
   --------------------------------------------------------- */
void operator_loop(void) {
    /* Attendi inizio giornata */
    sem_wait(&shm->sem_day_start);

    while (shm->simulation_running) {

        /* 1. Acquisisce una postazione */
        if (!acquire_station_post())
            continue;

        /* 2. Serve utenti finché non arriva la fine della giornata */
        serve_user();

        /* 3. Gestione pause */
        if (pause_count < shm->NOFPAUSE)
            handle_pause();

        /* 4. Rilascia postazione */
        release_station_post();
    }
}

/* ---------------------------------------------------------
   Acquisizione postazione
   --------------------------------------------------------- */
int acquire_station_post(void) {

    station_t *st = NULL;

    switch (station_type) {
        case 0: st = &shm->st_primi; break;
        case 1: st = &shm->st_secondi; break;
        case 2: st = &shm->st_coffee; break;
        case 3: st = &shm->st_cassa; break;
        default: return 0;
    }

    while (1) {
        sem_wait(&st->mutex);

        if (st->postazioni_occupate < st->postazioni_totali) {
            /* Se è il primo operatore attivo, aggiorna statistiche */
            if (st->postazioni_occupate == 0)
                shm->stats_giorno.operatori_attivi++;

            st->postazioni_occupate++;
            sem_post(&st->mutex);
            return 1;
        }

        sem_post(&st->mutex);

        /* Nessuna postazione libera → attesa leggera */
        nanosleep(&(struct timespec){0, 5000000}, NULL); // 5ms
    }
}

/* ---------------------------------------------------------
   Rilascio postazione
   --------------------------------------------------------- */
void release_station_post(void) {

    station_t *st = NULL;

    switch (station_type) {
        case 0: st = &shm->st_primi; break;
        case 1: st = &shm->st_secondi; break;
        case 2: st = &shm->st_coffee; break;
        case 3: st = &shm->st_cassa; break;
        default: return;
    }

    sem_wait(&st->mutex);
    st->postazioni_occupate--;
    sem_post(&st->mutex);
}

/* ---------------------------------------------------------
   Gestione pause
   --------------------------------------------------------- */
void handle_pause(void) {

    /* Criterio semplice: pausa casuale con probabilità 1/20 */
    if (rand_range(1, 20) != 1)
        return;

    pause_count++;
    shm->stats_giorno.pause_totali++;

    printf("[OPERATORE %d] Pausa %d\n", operator_id, pause_count);

    /* Durata pausa */
    nanosleep(&(struct timespec){ .tv_sec = 1, .tv_nsec = 0 }, NULL);
}

/* ---------------------------------------------------------
   Servizio utenti
   --------------------------------------------------------- */
void serve_user(void) {

    int msgid = get_msg_queue();
    msg_request_t req;
    msg_response_t res;

    /* Attende un utente con timeout per controllare periodicamente simulation_running */
    size_t req_size = sizeof(msg_request_t) - sizeof(long);
    ssize_t received = msgrcv(msgid, &req, req_size, 0, IPC_NOWAIT);
    
    if (received < 0) {
        if (errno == ENOMSG) {
            /* Nessun messaggio disponibile, breve pausa */
            nanosleep(&(struct timespec){0, 10000000}, NULL); // 10ms
            return;
        }
        /* Altri errori */
        if (!shm->simulation_running) {
            return;
        }
        perror("[OPERATORE] msgrcv");
        return;
    }

    /* Controllo disponibilità piatto (solo primi/secondi) */
    station_t *st = NULL;
    if (station_type == 0) st = &shm->st_primi;
    if (station_type == 1) st = &shm->st_secondi;

    if (st != NULL) {
        sem_wait(&st->mutex);
        if (st->porzioni[req.piatto_scelto] <= 0) {
            /* Piatto terminato */
            sem_post(&st->mutex);

            res.mtype = req.user_id;
            res.esito = 1; // piatto terminato
            size_t res_size = sizeof(msg_response_t) - sizeof(long);
            msgsnd(msgid, &res, res_size, 0);
            return;
        }

        /* Consuma una porzione */
        st->porzioni[req.piatto_scelto]--;

        sem_post(&st->mutex);
    }

    /* Calcola tempo di servizio */
    long t_ns = get_service_time_ns();
    nanosleep(&(struct timespec){ .tv_sec = t_ns / 1000000000,
                                  .tv_nsec = t_ns % 1000000000 }, NULL);

    /* Risposta */
    res.mtype = req.user_id;
    res.esito = 0;
    res.piatto_servito = req.piatto_scelto;
    clock_gettime(CLOCK_REALTIME, &res.t_servizio);

    size_t res_size = sizeof(msg_response_t) - sizeof(long);
    msgsnd(msgid, &res, res_size, 0);

    /* Aggiorna statistiche */
    update_stats_on_service(&req, &res);
}

/* ---------------------------------------------------------
   Ottiene ID coda messaggi della stazione
   --------------------------------------------------------- */
int get_msg_queue(void) {
    switch (station_type) {
        case 0: return shm->msgid_primi;
        case 1: return shm->msgid_secondi;
        case 2: return shm->msgid_coffee;
        case 3: return shm->msgid_cassa;
    }
    return -1;
}

/* ---------------------------------------------------------
   Tempo di servizio casuale
   --------------------------------------------------------- */
long get_service_time_ns(void) {

    long avg = 0;

    switch (station_type) {
        case 0: avg = shm->AVGSRVCPRIMI; break;
        case 1: avg = shm->AVGSRVCMAINCOURSE; break;
        case 2: avg = shm->AVGSRVCCOFFEE; break;
        case 3: avg = shm->AVGSRVCCASSA; break;
    }

    /* Percentuale variabile */
    int perc = 50;
    if (station_type == 2) perc = 80;   // coffee
    if (station_type == 3) perc = 20;   // cassa

    long min = avg - (avg * perc / 100);
    long max = avg + (avg * perc / 100);

    long ms = rand_range(min, max);
    return ms * 1000000L; // converti in nanosecondi
}

/* ---------------------------------------------------------
   Aggiornamento statistiche
   --------------------------------------------------------- */
void update_stats_on_service(msg_request_t *req, msg_response_t *res) {

    stats_t *day = &shm->stats_giorno;

    long wait_ns =
        (res->t_servizio.tv_sec - req->t_arrivo.tv_sec) * 1000000000L +
        (res->t_servizio.tv_nsec - req->t_arrivo.tv_nsec);

    switch (station_type) {
        case 0:
            day->tempo_attesa_primi_ns += wait_ns;
            day->piatti_primi_serviti++;
            day->ricavo_giornaliero += shm->PRICEPRIMI;
            break;

        case 1:
            day->tempo_attesa_secondi_ns += wait_ns;
            day->piatti_secondi_serviti++;
            day->ricavo_giornaliero += shm->PRICESECONDI;
            break;

        case 2:
            day->tempo_attesa_coffee_ns += wait_ns;
            day->piatti_coffee_serviti++;
            day->ricavo_giornaliero += shm->PRICECOFFEE;
            break;

        case 3:
            day->tempo_attesa_cassa_ns += wait_ns;
            break;
    }

    day->utenti_serviti++;
}