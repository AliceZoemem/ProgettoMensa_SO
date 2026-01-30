#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/msg.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
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

/* Piatti effettivamente ottenuti (per il pagamento) */
static int got_primo   = 0;
static int got_secondo = 0;
static int got_coffee  = 0;

/* ---------------------------------------------------------
   Prototipi
   --------------------------------------------------------- */
static void user_init();
static void user_loop(void);
static int  go_to_station(int station_type, int piatto);
static int  try_all_dishes_of_type(int station_type, int max_types);
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
    printf("[UTENTE] sizeof(msg_request_t) = %zu, sizeof(msg_response_t) = %zu\n",
       sizeof(msg_request_t), sizeof(msg_response_t));
printf("[UTENTE] MSG_REQ_SIZE = %zu, MSG_RES_SIZE = %zu\n",
       (size_t)MSG_REQ_SIZE, (size_t)MSG_RES_SIZE);
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
static void user_init() {
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
    while (1) {
        /* Attendi inizio giornata */
        sem_wait(&shm->sem_day_start);

        /* Se la simulazione è terminata, esci */
        if (!shm->simulation_running) {
            break;
        }

        /* Reinizializza il "menu" per il nuovo giorno */
        /* Stabilisce il menu desiderato per oggi */
        /* Criterio: ogni giorno vuole un primo e un secondo (obbligatori) */
        /* Il caffè è opzionale (decisione casuale) */
        want_primo   = 1;
        want_secondo = 1;
        want_coffee  = rand_range(0, 1); // coffee opzionale ogni giorno
        
        /* Reset piatti ottenuti */
        got_primo   = 0;
        got_secondo = 0;
        got_coffee  = 0;

        /* 1. PRIMO - prova tutti i tipi disponibili */
        if (want_primo) {
            if (!try_all_dishes_of_type(0, shm->menu_primi_count)) {
                printf("[UTENTE %d] Nessun primo disponibile, continuo...\n", user_id);
                want_primo = 0;
            } else {
                got_primo = 1;  /* Ha ottenuto un primo */
            }
        }

        /* Controlla se il giorno è finito mentre era in coda */
        if (!shm->simulation_running) {
            printf("[UTENTE %d] Giornata terminata mentre ero in coda, abbandono\n", user_id);
            sem_wait(&shm->sem_stats);
            shm->stats_giorno.utenti_non_serviti++;
            sem_post(&shm->sem_stats);
            continue;
        }

        /* 2. SECONDO - prova tutti i tipi disponibili */
        if (want_secondo) {
            if (!try_all_dishes_of_type(1, shm->menu_secondi_count)) {
                printf("[UTENTE %d] Nessun secondo disponibile, continuo...\n", user_id);
                want_secondo = 0;
            } else {
                got_secondo = 1;  /* Ha ottenuto un secondo */
            }
        }

        /* Controlla se il giorno è finito mentre era in coda */
        if (!shm->simulation_running) {
            printf("[UTENTE %d] Giornata terminata mentre ero in coda, abbandono\n", user_id);
            sem_wait(&shm->sem_stats);
            shm->stats_giorno.utenti_non_serviti++;
            sem_post(&shm->sem_stats);
            continue;
        }

        /* Se non ha ottenuto nulla → abbandona il giorno, ma resta per i successivi */
        if (!want_primo && !want_secondo) {
            printf("[UTENTE %d] Nessun piatto disponibile (primi e secondi esauriti), abbandono il giorno\n", user_id);
            sem_wait(&shm->sem_stats);
            shm->stats_giorno.utenti_non_serviti++;
            sem_post(&shm->sem_stats);
            continue;
        }

        /* 3. COFFEE (opzionale) */
        if (want_coffee) {
            if (go_to_station(2, 0)) {
                got_coffee = 1;  /* Ha ottenuto il caffè */
            }
        }

        /* Controlla se il giorno è finito mentre era in coda */
        if (!shm->simulation_running) {
            printf("[UTENTE %d] Giornata terminata mentre ero in coda, abbandono\n", user_id);
            sem_wait(&shm->sem_stats);
            shm->stats_giorno.utenti_non_serviti++;
            sem_post(&shm->sem_stats);
            continue;
        }

        /* 4. CASSA */
        if (!go_to_cassa()) {
            printf("[UTENTE %d] Impossibile pagare, abbandono il giorno\n", user_id);
            sem_wait(&shm->sem_stats);
            shm->stats_giorno.utenti_non_serviti++;
            sem_post(&shm->sem_stats);
            continue;
        }

        /* Controlla se il giorno è finito */
        if (!shm->simulation_running) {
            printf("[UTENTE %d] Giornata terminata, abbandono\n", user_id);
            sem_wait(&shm->sem_stats);
            shm->stats_giorno.utenti_non_serviti++;
            sem_post(&shm->sem_stats);
            continue;
        }

        /* 5. TAVOLO */
        go_to_tavolo_and_eat();

        printf("[UTENTE %d] Ha finito e lascia la mensa per oggi\n", user_id);
    }
}

/* ---------------------------------------------------------
   Richiesta piatto a una stazione
   --------------------------------------------------------- */
static int go_to_station(int station_type, int piatto) {
    msg_request_t  req;
    msg_response_t res;

    int msgid = get_msg_queue(station_type);

    memset(&req, 0, sizeof(req));
    req.mtype         = 1;          // tipo generico per la stazione
    req.user_id       = user_id;
    req.richiesta_tipo= station_type;
    req.piatto_scelto = piatto;
    clock_gettime(CLOCK_REALTIME, &req.t_arrivo);

    size_t req_size = MSG_REQ_SIZE;
    if (msgsnd(msgid, &req, req_size, 0) < 0) {
        perror("[UTENTE] msgsnd");
        return 0;
    }

    size_t res_size = MSG_RES_SIZE;
    ssize_t received;

    while (1) {
        if (!shm->simulation_running) {
            printf("[UTENTE %d] Simulazione terminata mentre ero in attesa\n", user_id);
            return 0;
        }

        /* QUI: mtype = user_id + 1 */
        received = msgrcv(msgid, &res, res_size, user_id + 1, IPC_NOWAIT | MSG_NOERROR);

        if (received >= 0)
            break;

        if (errno != ENOMSG) {
            fprintf(stderr, "[UTENTE DEBUG] msgrcv fallita: msgid=%d, res_size=%zu, mtype=%d, errno=%d\n",
        msgid, (size_t)MSG_RES_SIZE, user_id, errno);
            perror("[UTENTE] msgrcv");
            return 0;
        }

        nanosleep(&(struct timespec){0, 50000000}, NULL);
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
   Prova tutti i piatti di un tipo (primi o secondi)
   Ritorna 1 se riesce ad ottenere almeno un piatto, 0 altrimenti
   --------------------------------------------------------- */
static int try_all_dishes_of_type(int station_type, int max_types) {
    
    /* Crea lista casuale di piatti da provare */
    int dishes[MAX_PRIMI_TYPES];
    int count = (max_types < MAX_PRIMI_TYPES) ? max_types : MAX_PRIMI_TYPES;
    
    for (int i = 0; i < count; i++) {
        dishes[i] = i;
    }
    
    /* Mescola l'ordine */
    for (int i = count - 1; i > 0; i--) {
        int j = rand_range(0, i);
        int temp = dishes[i];
        dishes[i] = dishes[j];
        dishes[j] = temp;
    }
    
    /* Prova ogni piatto finché non ne ottiene uno */
    for (int i = 0; i < count; i++) {
        
        /* Controlla se la simulazione è ancora attiva */
        if (!shm->simulation_running) {
            return 0;
        }
        
        int result = go_to_station(station_type, dishes[i]);
        
        if (result == 1) {
            /* Piatto ottenuto con successo */
            return 1;
        }
        
        /* Se esito = 1 (piatto terminato), prova il prossimo */
        /* Se esito = 2 (tutti terminati), prova comunque gli altri per sicurezza */
    }
    
    /* Nessun piatto disponibile */
    return 0;
}

/* ---------------------------------------------------------
   Pagamento alla cassa
   --------------------------------------------------------- */
static int go_to_cassa(void) {

    msg_request_t  req;
    msg_response_t res;

    int msgid = get_msg_queue(3);

    memset(&req, 0, sizeof(req));
    req.mtype        = 1;
    req.user_id        = user_id;
    req.richiesta_tipo = 3;   // cassa
    req.piatto_scelto  = 0;
    
    /* Indica quali piatti l'utente ha preso */
    req.ha_primo   = got_primo;
    req.ha_secondo = got_secondo;
    req.ha_coffee  = got_coffee;
    
    clock_gettime(CLOCK_REALTIME, &req.t_arrivo);
    
    printf("[UTENTE %d] Va alla cassa per pagare (Primo:%d Secondo:%d Coffee:%d)\n", 
           user_id, got_primo, got_secondo, got_coffee);

    size_t req_size = MSG_REQ_SIZE;
    if (msgsnd(msgid, &req, req_size, 0) < 0) {
        perror("[UTENTE] msgsnd cassa");
        return 0;
    }

    /* Attende risposta con controllo periodico */
    size_t res_size = MSG_RES_SIZE;
    ssize_t received;
    
    while (1) {
        /* Controlla se la simulazione è ancora attiva */
        if (!shm->simulation_running) {
            printf("[UTENTE %d] Simulazione terminata mentre ero in coda alla cassa\n", user_id);
            return 0;
        }
        
        /* Prova a ricevere con IPC_NOWAIT e MSG_NOERROR */
        received = msgrcv(msgid, &res, res_size, user_id + 1, IPC_NOWAIT | MSG_NOERROR);
        
        if (received >= 0) {
            /* Messaggio ricevuto */
            printf("[UTENTE %d] Ricevuto risposta dalla cassa: esito=%d, received=%zd bytes\n",
                   user_id, res.esito, received);
            break;
        }
        
        if (errno != ENOMSG) {
            /* Errore diverso da "nessun messaggio" */
            perror("[UTENTE] msgrcv cassa");
            return 0;
        }
        
        /* Breve attesa prima di riprovare */
        nanosleep(&(struct timespec){0, 50000000}, NULL); // 50ms
    }

    /* Verifica esito */
    if (res.esito == 0) {
        printf("[UTENTE %d] Ha pagato alla cassa\n", user_id);
        return 1;
    }
    
    printf("[UTENTE %d] Errore al pagamento alla cassa (esito=%d)\n", user_id, res.esito);
    return 0;
}

/* ---------------------------------------------------------
   Tavolo e consumo pasto
   --------------------------------------------------------- */
static void go_to_tavolo_and_eat(void) {

    /* Attende posto libero */
    printf("[UTENTE %d] Cerca un tavolo libero...\n", user_id);
    sem_wait(&shm->sem_tavoli);
    shm->tavoli_liberi--;

    printf("[UTENTE %d] Si è seduto a mangiare (Primo:%d Secondo:%d Coffee:%d)\n", 
           user_id, got_primo, got_secondo, got_coffee);

    /* Tempo di consumo proporzionale ai piatti effettivamente ottenuti */
    int piatti = got_primo + got_secondo + got_coffee;
    int eat_ms = piatti * 1500; // 1.5 secondi per piatto

    struct timespec t;
    t.tv_sec  = eat_ms / 1000;
    t.tv_nsec = (eat_ms % 1000) * 1000000L;
    nanosleep(&t, NULL);

    printf("[UTENTE %d] Ha finito di mangiare, lascia il tavolo\n", user_id);
    shm->tavoli_liberi++;
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