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
static int want_primo   = 1;
static int want_secondo = 1;
static int want_coffee  = 0;

/* ---------------------------------------------------------
   Prototipi
   --------------------------------------------------------- */
static void user_init(int id);
static void user_loop(void);
static int  go_to_station(int station_type, int piatto);
static int  go_to_cassa(void);
static void go_to_tavolo_and_eat(void);
static int  get_msg_queue(int station_type);

/* ---------------------------------------------------------
   MAIN
   --------------------------------------------------------- */
int main(int argc, char *argv[]) {

    if (argc < 2) {
        fprintf(stderr, "[UTENTE] Uso: utente <id>\n");
        exit(EXIT_FAILURE);
    }

    user_id = atoi(argv[1]);

    /* 1. Attacco alla shared memory */
    shm = ipc_attach_shared_memory();

    /* 2. Segnalo che sono pronto */
    ipc_signal_ready();

    /* 3. Attendo la barriera */
    sem_wait(&shm->sem_barrier);

    /* 4. Logica utente */
    user_init(user_id);
    user_loop();

    return 0;
}

/* ---------------------------------------------------------
   Inizializzazione utente
   --------------------------------------------------------- */
static void user_init(int id) {
    // printf("[UTENTE %d] Avviato\n", id);
    srand(time(NULL) ^ (getpid() << 16));

    want_primo   = 1;
    want_secondo = 1;
    want_coffee  = rand_range(0, 1); // opzionale
}

/* ---------------------------------------------------------
   Ciclo principale utente
   --------------------------------------------------------- */
static void user_loop(void) {

    /* Attendi inizio giornata */
    sem_wait(&shm->sem_day_start);

    /* Controlla se la simulazione è ancora attiva */
    if (!shm->simulation_running) {
        return;
    }

    /* 1. PRIMO */
    if (want_primo) {
        if (!go_to_station(0, rand_range(0, MAX_PRIMI_TYPES - 1))) {
            printf("[UTENTE %d] Nessun primo disponibile, continuo...\n", user_id);
            want_primo = 0;
        }
    }

    /* 2. SECONDO */
    if (want_secondo) {
        if (!go_to_station(1, rand_range(0, MAX_SECONDI_TYPES - 1))) {
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
static int go_to_station(int station_type, int piatto) {

    msg_request_t  req;
    msg_response_t res;

    int msgid = get_msg_queue(station_type);

    /* Prepara richiesta */
    req.mtype         = 1;              // tipo generico per la coda della stazione
    req.user_id       = user_id;
    req.richiesta_tipo= station_type;   // 0=primo,1=secondo,2=coffee
    req.piatto_scelto = piatto;
    clock_gettime(CLOCK_REALTIME, &req.t_arrivo);

    printf("[UTENTE %d] Richiede piatto %d alla stazione %s con richiesta tipo %d\n",
           user_id, piatto, station_type == 0 ? "primo" : station_type == 1 ? "secondo" : station_type == 2 ? "coffee" : "cassa", req.richiesta_tipo);

    /* Invia richiesta */
    size_t req_size = sizeof(msg_request_t) - sizeof(long);
    if (msgsnd(msgid, &req, req_size, 0) < 0) {
        perror("[UTENTE] msgsnd");
        return 0;
    }

    /* Attende risposta (mtype = user_id) */
    size_t res_size = sizeof(msg_response_t) - sizeof(long);
    ssize_t received = msgrcv(msgid, &res, res_size, user_id, 0);
    if (received < 0) {
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
static int go_to_cassa(void) {

    msg_request_t  req;
    msg_response_t res;

    int msgid = get_msg_queue(3);

    req.mtype          = 1;
    req.user_id        = user_id;
    req.richiesta_tipo = 3;   // cassa
    req.piatto_scelto  = 0;
    clock_gettime(CLOCK_REALTIME, &req.t_arrivo);

    size_t req_size = sizeof(msg_request_t) - sizeof(long);
    if (msgsnd(msgid, &req, req_size, 0) < 0) {
        perror("[UTENTE] msgsnd cassa");
        return 0;
    }

    size_t res_size = sizeof(msg_response_t) - sizeof(long);
    if (msgrcv(msgid, &res, res_size, user_id, 0) < 0) {
        perror("[UTENTE] msgrcv cassa");
        return 0;
    }

    printf("[UTENTE %d] Ha pagato alla cassa\n", user_id);
    return 1;
}

/* ---------------------------------------------------------
   Tavolo e consumo pasto
   --------------------------------------------------------- */
static void go_to_tavolo_and_eat(void) {

    /* Attende posto libero */
    sem_wait(&shm->sem_tavoli);

    printf("[UTENTE %d] Si è seduto a mangiare\n", user_id);

    int piatti = want_primo + want_secondo + want_coffee;
    int eat_ms = piatti * 1500; // 1.5 secondi per piatto

    struct timespec t;
    t.tv_sec  = eat_ms / 1000;
    t.tv_nsec = (eat_ms % 1000) * 1000000L;
    nanosleep(&t, NULL);

    sem_post(&shm->sem_tavoli);
}

/* ---------------------------------------------------------
   Ottiene ID coda messaggi della stazione
   --------------------------------------------------------- */
static int get_msg_queue(int station_type) {
    switch (station_type) {
        case 0: return shm->msgid_primi;
        case 1: return shm->msgid_secondi;
        case 2: return shm->msgid_coffee;
        case 3: return shm->msgid_cassa;
    }
    return -1;
}