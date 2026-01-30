#ifndef SHARED_STRUCTS_H
#define SHARED_STRUCTS_H

#include <semaphore.h>
#include <time.h>

/* ---------------------------------------------------------
   Costanti generali
   --------------------------------------------------------- */
#define MAX_PRIMI_TYPES     4
#define MAX_SECONDI_TYPES   4
#define MAX_COFFEE_TYPES    4

/* ---------------------------------------------------------
   Messaggi per le code di messaggi
   --------------------------------------------------------- */
typedef struct {
    long mtype;              // tipo messaggio (stazione o utente)
    int user_id;             // id utente
    int richiesta_tipo;      // 0=primo, 1=secondo, 2=coffee, 3=cassa
    int piatto_scelto;       // indice piatto richiesto

    int ha_primo;
    int ha_secondo;
    int ha_coffee;

    struct timespec t_arrivo;
} msg_request_t;

typedef struct {
    long mtype;              // tipo messaggio (utente)
    int user_id;
    int esito;               // 0=ok, 1=piatto terminato, 2=nessun piatto disponibile
    int piatto_servito;
    struct timespec t_servizio;
} msg_response_t;

#define MSG_REQ_SIZE  (sizeof(msg_request_t)  - sizeof(long))
#define MSG_RES_SIZE  (sizeof(msg_response_t) - sizeof(long))
/* ---------------------------------------------------------
   Struttura stazione (primi, secondi, coffee, cassa)
   --------------------------------------------------------- */
typedef struct {
    int postazioni_totali;      // numero di postazioni disponibili
    int postazioni_occupate;    // quante sono occupate dagli operatori

    int porzioni[MAX_PRIMI_TYPES]; // per primi/secondi, -1 per coffee/cassa

    long tempo_attesa_totale_ns; // somma tempi attesa utenti
    int utenti_serviti;
    int utenti_in_coda;

    int msgid;                  // ID coda messaggi associata alla stazione

    sem_t mutex;                // mutua esclusione sulla stazione
} station_t;

/* ---------------------------------------------------------
   Statistiche giornaliere e totali
   --------------------------------------------------------- */
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

    int utenti_in_attesa;       // per overload

    double ricavo_giornaliero;
} stats_t;

/* ---------------------------------------------------------
   Struttura principale della memoria condivisa
   --------------------------------------------------------- */
typedef struct {

    /* ID shared memory (per attach nei figli) */
    int shm_id;

    /* Parametri configurazione */
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

    /* Stazioni */
    station_t st_primi;
    station_t st_secondi;
    station_t st_coffee;
    station_t st_cassa;

    /* Tavoli */
    int tavoli_liberi;
    sem_t sem_tavoli;

    /* Menu */
    char *menu_primi[MAX_PRIMI_TYPES];
    int menu_primi_count;

    char *menu_secondi[MAX_SECONDI_TYPES];
    int menu_secondi_count;

    char *menu_coffee[MAX_COFFEE_TYPES];
    int menu_coffee_count;

    /* Statistiche */
    stats_t stats_tot;
    stats_t stats_giorno;
    sem_t sem_stats;            // mutex per accesso alle statistiche

    /* Stato simulazione */
    int giorno_corrente;
    int terminazione_causa; // 0=timeout, 1=overload

    /* Barriera di avvio */
    int ready_count;
    sem_t sem_barrier;

    /* Controllo simulazione */
    int simulation_running;     // 1=in corso, 0=terminata
    sem_t sem_day_start;        // segnala inizio giornata

    /* Code di messaggi */
    int msgid_primi;
    int msgid_secondi;
    int msgid_coffee;
    int msgid_cassa;

} shm_t;

#endif