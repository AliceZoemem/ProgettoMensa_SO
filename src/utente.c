#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/msg.h>
#include <string.h>

#include "shared_structs.h"
#include "ipc.h"
#include "util.h"

extern shm_t *shm;
/* Parametri utente */
static int user_id = -1;

/* Menu scelto */
static int want_primo = 1;
static int want_secondo = 1;
static int want_coffee = 0;

/* ---------------------------------------------------------
   Funzioni interne
   --------------------------------------------------------- */
void user_init(int id);
void user_loop(void);
int  request_plate(int station_type, int piatto);
int  go_to_station(int station_type, int piatto);
int  go_to_cassa(void);
void go_to_tavolo_and_eat(void);
int  get_msg_queue(int station_type);

/* ---------------------------------------------------------
   MAIN
   --------------------------------------------------------- */
int main(int argc, char *argv[]) {

    if (argc < 2) {
        fprintf(stderr, "[UTENTE] Uso: utente <id>\n");
        exit(EXIT_FAILURE);
    }

    user_id = atoi(argv[1]);

    /* Attacca shared memory */
    shm = ipc_attach_shared_memory();

    /* Segnala che l’utente è pronto */
    ipc_signal_ready();

    /* Attende la barriera */
    sem_wait(&shm->sem_barrier);

    user_init(user_id);
    user_loop();

    return 0;
}

/* ---------------------------------------------------------
   Inizializzazione utente
   --------------------------------------------------------- */
void user_init(int id) {
    //printf("[UTENTE %d] Avviato\n", id);
    srand(time(NULL) ^ (getpid()<<16));

    /* Scelta menu (per ora casuale) */
    want_primo   = 1;
    want_secondo = 1;
    want_coffee  = rand_range(0, 1); // opzionale
}

/* ---------------------------------------------------------
   Ciclo principale utente
   --------------------------------------------------------- */
void user_loop(void) {

    /* 1. PRIMO */
    if (want_primo) {
        if (!go_to_station(0, rand_range(0, MAX_PRIMI_TYPES-1))) {
            printf("[UTENTE %d] Nessun primo disponibile, continuo...\n", user_id);
            want_primo = 0;
        }
    }

    /* 2. SECONDO */
    if (want_secondo) {
        if (!go_to_station(1, rand_range(0, MAX_SECONDI_TYPES-1))) {
            printf("[UTENTE %d] Nessun secondo disponibile, continuo...\n", user_id);
            want_secondo = 0;
        }
    }

    /* Se non ha ottenuto nulla → abbandona */
    if (!want_primo && !want_secondo) {
        printf("[UTENTE %d] Nessun piatto disponibile, abbandono\n", user_id);
        return;
    }

    /* 3. COFFEE (opzionale) */
    if (want_coffee) {
        go_to_station(2, 0); // coffee non ha tipi
    }

    /* 4. CASSA */
    if (!go_to_cassa()) {
        printf("[UTENTE %d] Impossibile pagare, abbandono\n", user_id);
        return;
    }

    /* 5. TAVOLO */
    go_to_tavolo_and_eat();

    printf("[UTENTE %d] Ha finito e lascia la mensa\n", user_id);
}

/* ---------------------------------------------------------
   Richiesta piatto a una stazione
   --------------------------------------------------------- */
int go_to_station(int station_type, int piatto) {

    msg_request_t req;
    msg_response_t res;

    int msgid = get_msg_queue(station_type);

    /* Prepara richiesta */
    req.mtype = 1; // tipo generico
    req.user_id = user_id;
    req.richiesta_tipo = station_type;
    req.piatto_scelto = piatto;
    clock_gettime(CLOCK_REALTIME, &req.t_arrivo);

    /* Invia richiesta */
    if (msgsnd(msgid, &req, sizeof(req) - sizeof(long), 0) < 0) {
        perror("[UTENTE] msgsnd");
        return 0;
    }

    /* Attende risposta */
    if (msgrcv(msgid, &res, sizeof(res) - sizeof(long), user_id, 0) < 0) {
        perror("[UTENTE] msgrcv");
        return 0;
    }

    /* Gestione esito */
    if (res.esito == 0) {
        printf("[UTENTE %d] Servito alla stazione %d\n", user_id, station_type);
        return 1;
    }

    if (res.esito == 1) {
        printf("[UTENTE %d] Piatto terminato alla stazione %d\n", user_id, station_type);
        return 0;
    }

    if (res.esito == 2) {
        printf("[UTENTE %d] Nessun piatto disponibile alla stazione %d\n", user_id, station_type);
        return 0;
    }

    return 0;
}

/* ---------------------------------------------------------
   Pagamento alla cassa
   --------------------------------------------------------- */
int go_to_cassa(void) {

    msg_request_t req;
    msg_response_t res;

    int msgid = get_msg_queue(3);

    req.mtype = 1;
    req.user_id = user_id;
    req.richiesta_tipo = 3;
    req.piatto_scelto = 0;
    clock_gettime(CLOCK_REALTIME, &req.t_arrivo);

    if (msgsnd(msgid, &req, sizeof(req) - sizeof(long), 0) < 0) {
        perror("[UTENTE] msgsnd cassa");
        return 0;
    }

    if (msgrcv(msgid, &res, sizeof(res) - sizeof(long), user_id, 0) < 0) {
        perror("[UTENTE] msgrcv cassa");
        return 0;
    }

    printf("[UTENTE %d] Ha pagato alla cassa\n", user_id);
    return 1;
}

/* ---------------------------------------------------------
   Tavolo e consumo pasto
   --------------------------------------------------------- */
void go_to_tavolo_and_eat(void) {

    /* Attende posto libero */
    sem_wait(&shm->sem_tavoli);

    printf("[UTENTE %d] Si è seduto a mangiare\n", user_id);

    /* Tempo di consumo proporzionale ai piatti */
    int piatti = want_primo + want_secondo + want_coffee;
    int eat_ms = piatti * 1500; // 1.5 secondi per piatto

    nanosleep(&(struct timespec){ .tv_sec = eat_ms / 1000,
                                  .tv_nsec = (eat_ms % 1000) * 1000000 }, NULL);

    sem_post(&shm->sem_tavoli);
}

/* ---------------------------------------------------------
   Ottiene ID coda messaggi della stazione
   --------------------------------------------------------- */
int get_msg_queue(int station_type) {
    switch (station_type) {
        case 0: return shm->msgid_primi;
        case 1: return shm->msgid_secondi;
        case 2: return shm->msgid_coffee;
        case 3: return shm->msgid_cassa;
    }
    return -1;
}