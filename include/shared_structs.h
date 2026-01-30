#ifndef SHARED_STRUCTS_H
#define SHARED_STRUCTS_H

#include <semaphore.h>
#include <time.h>

#define MAX_PRIMI_TYPES     4
#define MAX_SECONDI_TYPES   4
#define MAX_COFFEE_TYPES    4

typedef struct {
    long mtype;              // tipo messaggio (stazione o utente)
    int user_id;             
    int richiesta_tipo;      // 0=primo, 1=secondo, 2=coffee, 3=cassa
    int piatto_scelto;       

    int ha_primo;
    int ha_secondo;
    int ha_coffee;

    struct timespec t_arrivo;
} msg_request_t;

typedef struct {
    long mtype;              
    int user_id;
    int esito;               // 0=ok, 1=piatto terminato, 2=nessun piatto disponibile
    int piatto_servito;
    struct timespec t_servizio;
} msg_response_t;

#define MSG_REQ_SIZE  (sizeof(msg_request_t)  - sizeof(long))
#define MSG_RES_SIZE  (sizeof(msg_response_t) - sizeof(long))

typedef struct {
    int postazioni_totali;      
    int postazioni_occupate;    

    int porzioni[MAX_PRIMI_TYPES]; 

    long tempo_attesa_totale_ns; 
    int utenti_serviti;
    int utenti_in_coda;

    int msgid;                  

    sem_t mutex;               
} station_t;

typedef struct {
    int utenti_serviti;
    int utenti_non_serviti;

    int piatti_primi_serviti;
    int piatti_secondi_serviti;
    int piatti_coffee_serviti;

    int piatti_primi_avanzati;
    int piatti_secondi_avanzati;

    long tempo_attesa_primi_ns;
    long tempo_attesa_secondi_ns;
    long tempo_attesa_coffee_ns;
    long tempo_attesa_cassa_ns;

    int operatori_attivi;
    int pause_totali;

    int utenti_in_attesa;    

    double ricavo_giornaliero;
} stats_t;

typedef struct {
    int shm_id;

    int NOFWORKERS;
    int NOFUSERS;
    int SIMDURATION;
    long NNANOSECS;
    int OVERLOADTHRESHOLD;

    int NOFTABLESEATS;
    int NOFWKSEATSPRIMI;
    int NOFWKSEATSSECONDI;
    int NOFWKSEATSCOFFEE;
    int NOFWKSEATSCASSA;

    int AVGREFILLPRIMI;
    int AVGREFILLSECONDI;
    int MAXPORZIONIPRIMI;
    int MAXPORZIONISECONDI;

    int AVGSRVCPRIMI;
    int AVGSRVCMAINCOURSE;
    int AVGSRVCCOFFEE;
    int AVGSRVCCASSA;

    int NOFPAUSE;

    double PRICEPRIMI;
    double PRICESECONDI;
    double PRICECOFFEE;

    station_t st_primi;
    station_t st_secondi;
    station_t st_coffee;
    station_t st_cassa;

    int tavoli_liberi;
    sem_t sem_tavoli;

    char *menu_primi[MAX_PRIMI_TYPES];
    int menu_primi_count;

    char *menu_secondi[MAX_SECONDI_TYPES];
    int menu_secondi_count;

    char *menu_coffee[MAX_COFFEE_TYPES];
    int menu_coffee_count;

    stats_t stats_tot;
    stats_t stats_giorno;
    sem_t sem_stats;            // mutex per accesso alle statistiche

    int giorno_corrente;
    int terminazione_causa; // 0=timeout, 1=overload

    /* Barriera di avvio */
    int ready_count;
    sem_t sem_barrier;

    int simulation_running;     // 1=in corso, 0=terminata
    sem_t sem_day_start;        // segnala inizio giornata

    int msgid_primi;
    int msgid_secondi;
    int msgid_coffee;
    int msgid_cassa;

} shm_t;

#endif