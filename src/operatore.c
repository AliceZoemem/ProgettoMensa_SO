#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/msg.h>
#include <stddef.h>
#include "shared_structs.h"
#include "ipc.h"
#include "util.h"

extern shm_t *shm;
static int operator_id = -1;
static int station_type = -1;   // 0=primi, 1=secondi, 2=coffee, 3=cassa

static int pause_count = 0;

void operator_init(int id, int st_type);
void operator_loop(void);
int  acquire_station_post(void);
void release_station_post(void);
int  handle_pause(void);
void serve_user(void);
int  get_msg_queue(void);
long get_service_time_ns(void);
void update_stats_on_service(msg_request_t *req, msg_response_t *res);

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "[OPERATORE] Uso: operatore <id> <station_type>\n");
        exit(EXIT_FAILURE);
    }
    operator_id  = atoi(argv[1]);
    station_type = atoi(argv[2]);

    shm = ipc_attach_shared_memory();
    ipc_signal_ready();

    sem_wait(&shm->sem_barrier);

    operator_init(operator_id, station_type);
    operator_loop();

    return 0;
}

void operator_init(int id, int st_type) {
    printf("[OPERATORE %d] Avviato su stazione %d\n", id, st_type);
    srand(time(NULL) ^ (getpid()<<16));
}

void operator_loop(void) {
    while (1) {
        sem_wait(&shm->sem_day_start);

        if (!shm->simulation_running) {
            break;
        }

        pause_count = 0;

        printf("[OPERATORE %d] Competizione per postazione alla stazione %d\n", 
               operator_id, station_type);
        
        if (!acquire_station_post()) {
            continue;
        }
        
        printf("[OPERATORE %d] Postazione acquisita, inizio turno\n", operator_id);

        while (shm->simulation_running) {
            serve_user();

            if (pause_count < shm->NOFPAUSE) {
                if (handle_pause()) {
                    if (!acquire_station_post()) {
                        break;
                    }
                    printf("[OPERATORE %d] Rientrato dalla pausa\n", operator_id);
                }
            }
        }
        
        printf("[OPERATORE %d] Fine turno giornaliero\n", operator_id);
        release_station_post();
    }
}

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
        if (!shm->simulation_running) {
            return 0;
        }

        sem_wait(&st->mutex);

        if (st->postazioni_occupate < st->postazioni_totali) {
            st->postazioni_occupate++;
            sem_post(&st->mutex);
            return 1;
        }

        sem_post(&st->mutex);

        nanosleep(&(struct timespec){0, 10000000}, NULL); 
    }
}

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
   Criteri per andare in pausa:
   - Massimo NOF_PAUSE pause al giorno
   - Probabilità casuale basata sul carico (più utenti = meno pause)
   - Almeno un operatore deve rimanere attivo sulla stazione
   Ritorna 1 se è andato in pausa, 0 altrimenti
   --------------------------------------------------------- */
int handle_pause(void) {
    station_t *st = NULL;
    switch (station_type) {
        case 0: st = &shm->st_primi; break;
        case 1: st = &shm->st_secondi; break;
        case 2: st = &shm->st_coffee; break;
        case 3: st = &shm->st_cassa; break;
        default: return 0;
    }

    /* Probabilità di pausa: 5% base, ridotta se ci sono utenti in attesa */
    int pausa_probabilita = 20; // 1/20 = 5%
    if (st->utenti_in_coda > 5) {
        pausa_probabilita = 50; // 1/50 = 2% se c'è carico
    }
    
    if (rand_range(1, pausa_probabilita) != 1)
        return 0;

    /* Verifica che NON sia l'unico operatore sulla stazione */
    sem_wait(&st->mutex);
    
    /* Deve esserci almeno un altro operatore attivo (postazioni_occupate > 1) */
    if (st->postazioni_occupate <= 1) {
        sem_post(&st->mutex);
        /* Non può andare in pausa: è l'unico operatore */
        return 0;
    }
    
    st->postazioni_occupate--;
    
    sem_post(&st->mutex);

    pause_count++;
    shm->stats_giorno.pause_totali++;

    printf("[OPERATORE %d] Pausa %d/%d (postazioni ora: %d/%d)\n", 
           operator_id, pause_count, shm->NOFPAUSE,
           st->postazioni_occupate, st->postazioni_totali);

    /* Durata pausa: tra 2 e 5 secondi */
    long pausa_sec = rand_range(2, 5);
    nanosleep(&(struct timespec){ .tv_sec = pausa_sec, .tv_nsec = 0 }, NULL);

    return 1;
}

void serve_user(void) {
    int msgid = get_msg_queue();
    msg_request_t req;
    msg_response_t res;

    memset(&req, 0, sizeof(req));
    ssize_t received = msgrcv(msgid, &req, MSG_REQ_SIZE, 1, IPC_NOWAIT | MSG_NOERROR);
    
    if (received < 0) {
        if (errno == ENOMSG) {
            nanosleep(&(struct timespec){0, 10000000}, NULL); 
            return;
        }
        if (!shm->simulation_running) {
            return;
        }
        perror("[OPERATORE] msgrcv");
        return;
    }
    
    /*if (station_type == 3) {
        printf("[CASSIERE %d] Ricevuto richiesta da utente %d (Primo:%d Secondo:%d Coffee:%d)\n",
               operator_id, req.user_id, req.ha_primo, req.ha_secondo, req.ha_coffee);
    }*/
    
    if (req.user_id < 0 || req.user_id >= shm->NOFUSERS || 
        req.richiesta_tipo < 0 || req.richiesta_tipo > 3) {
        printf("[OPERATORE %d] ERRORE: Messaggio corrotto! user_id=%d, tipo=%d\n", 
               operator_id, req.user_id, req.richiesta_tipo);
        return;
    }

    station_t *st = NULL;
    if (station_type == 0) st = &shm->st_primi;
    if (station_type == 1) st = &shm->st_secondi;

    if (st != NULL) {
        sem_wait(&st->mutex);
        if (st->porzioni[req.piatto_scelto] <= 0) {
            sem_post(&st->mutex);
            memset(&res, 0, sizeof(res));
            res.mtype = req.user_id+ 10000;   // così l'utente filtra per user_id
            res.user_id = req.user_id;
            res.esito = 1; // piatto terminato
            size_t res_size = MSG_RES_SIZE;
            if (msgsnd(msgid, &res, res_size, 0) < 0) {
                fprintf(stderr, "[OPERATORE DEBUG] msgsnd fallita: msgid=%d, res_size=%zu, mtype=%ld, errno=%d\n",
        msgid, (size_t)MSG_RES_SIZE, res.mtype, errno);
                perror("[OPERATORE] msgsnd");
            }
            return;
        }
        st->porzioni[req.piatto_scelto]--;
        sem_post(&st->mutex);
    }
    struct timespec t_inizio_servizio;
    clock_gettime(CLOCK_REALTIME, &t_inizio_servizio);

    long t_ns = get_service_time_ns();
    nanosleep(&(struct timespec){ .tv_sec = t_ns / 1000000000,
                                  .tv_nsec = t_ns % 1000000000 }, NULL);

    memset(&res, 0, sizeof(res));
    res.mtype         = req.user_id+ 10000;   
    res.user_id = req.user_id;
    res.esito = 0;
    res.piatto_servito = req.piatto_scelto;
    res.t_servizio = t_inizio_servizio;

    /*if (station_type == 3) {
        printf("[CASSIERE %d] Invio risposta a utente %d: esito=%d, mtype=%ld\n",
               operator_id, req.user_id, res.esito, res.mtype);
    }*/

    size_t res_size = MSG_RES_SIZE;
    if (msgsnd(msgid, &res, res_size, 0) < 0) {
                fprintf(stderr, "[OPERATORE DEBUG] msgsnd fallita: msgid=%d, res_size=%zu, mtype=%ld, errno=%d\n",
        msgid, (size_t)MSG_RES_SIZE, res.mtype, errno);
        perror("[OPERATORE] msgsnd");
    }
    update_stats_on_service(&req, &res);
}

int get_msg_queue(void) {
    switch (station_type) {
        case 0: return shm->msgid_primi;
        case 1: return shm->msgid_secondi;
        case 2: return shm->msgid_coffee;
        case 3: return shm->msgid_cassa;
    }
    return -1;
}

long get_service_time_ns(void) {
    long avg = 0;

    switch (station_type) {
        case 0: avg = shm->AVGSRVCPRIMI; break;
        case 1: avg = shm->AVGSRVCMAINCOURSE; break;
        case 2: avg = shm->AVGSRVCCOFFEE; break;
        case 3: avg = shm->AVGSRVCCASSA; break;
    }

    int perc = 50;
    if (station_type == 2) perc = 80;   // coffee
    if (station_type == 3) perc = 20;   // cassa

    long min = avg - (avg * perc / 100);
    long max = avg + (avg * perc / 100);

    long ms = rand_range(min, max);
    return ms * 1000000L;
}

void update_stats_on_service(msg_request_t *req, msg_response_t *res) {

    stats_t *day = &shm->stats_giorno;

    long wait_ns =
        (res->t_servizio.tv_sec - req->t_arrivo.tv_sec) * 1000000000L +
        (res->t_servizio.tv_nsec - req->t_arrivo.tv_nsec);

    sem_wait(&shm->sem_stats);  // Mutua esclusione per aggiornamento statistiche

    switch (station_type) {
        case 0:
            day->tempo_attesa_primi_ns += wait_ns;
            day->piatti_primi_serviti++;
            break;

        case 1:
            day->tempo_attesa_secondi_ns += wait_ns;
            day->piatti_secondi_serviti++;
            break;

        case 2:
            day->tempo_attesa_coffee_ns += wait_ns;
            day->piatti_coffee_serviti++;
            break;

        case 3:
            /* CASSA: calcola il totale in base ai piatti presi dall'utente */
            day->tempo_attesa_cassa_ns += wait_ns;
            
            double totale = 0.0;
            
            if (req->ha_primo) {
                totale += shm->PRICEPRIMI;
            }
            if (req->ha_secondo) {
                totale += shm->PRICESECONDI;
            }
            if (req->ha_coffee) {
                totale += shm->PRICECOFFEE;
            }
            
            day->ricavo_giornaliero += totale;
            
            /*printf("[CASSIERE %d] Utente %d - Primo:%d Secondo:%d Coffee:%d - Totale: %.2f€\n",
                   operator_id, req->user_id, req->ha_primo, req->ha_secondo, 
                   req->ha_coffee, totale);
            */
            break;
    }

    sem_post(&shm->sem_stats);
}