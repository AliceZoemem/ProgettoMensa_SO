#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "shared_structs.h"
#include "stations.h"
#include "util.h"

void stations_init(shm_t *shm) {
    printf("[STATIONS] Inizializzazione stazioni...\n");
    shm->st_primi.postazioni_totali   = 0;
    shm->st_primi.postazioni_occupate = 0;
    memset(shm->st_primi.porzioni, 0, sizeof(shm->st_primi.porzioni));
    shm->st_primi.tempo_attesa_totale_ns = 0;
    shm->st_primi.utenti_serviti = 0;
    shm->st_primi.utenti_in_coda = 0;

    shm->st_secondi.postazioni_totali   = 0;
    shm->st_secondi.postazioni_occupate = 0;
    memset(shm->st_secondi.porzioni, 0, sizeof(shm->st_secondi.porzioni));
    shm->st_secondi.tempo_attesa_totale_ns = 0;
    shm->st_secondi.utenti_serviti = 0;
    shm->st_secondi.utenti_in_coda = 0;

    shm->st_coffee.postazioni_totali   = 0;
    shm->st_coffee.postazioni_occupate = 0;
    for (int i = 0; i < MAX_COFFEE_TYPES; i++)
        shm->st_coffee.porzioni[i] = -1; // infinito
    shm->st_coffee.tempo_attesa_totale_ns = 0;
    shm->st_coffee.utenti_serviti = 0;
    shm->st_coffee.utenti_in_coda = 0;

    shm->st_cassa.postazioni_totali   = 0;
    shm->st_cassa.postazioni_occupate = 0;
    for (int i = 0; i < MAX_PRIMI_TYPES; i++)
        shm->st_cassa.porzioni[i] = -1;
    shm->st_cassa.tempo_attesa_totale_ns = 0;
    shm->st_cassa.utenti_serviti = 0;
    shm->st_cassa.utenti_in_coda = 0;

    shm->tavoli_liberi = shm->NOFTABLESEATS;
}

void stations_refill_day(shm_t *shm) {
    printf("[STATIONS] Refill iniziale del giorno...\n");
    for (int i = 0; i < shm->menu_primi_count; i++)
        shm->st_primi.porzioni[i] = shm->AVGREFILLPRIMI;

    for (int i = 0; i < shm->menu_secondi_count; i++)
        shm->st_secondi.porzioni[i] = shm->AVGREFILLSECONDI;
}

/* ---------------------------------------------------------
   Refill periodico (ogni 10 minuti simulati)
   Chiamata da mensa.c durante il giorno
   Incrementa di 1 porzione fino a MAX_PORZIONI per ogni tipo di piatto
   --------------------------------------------------------- */
void stations_refill_periodic(shm_t *shm) {
    for (int i = 0; i < shm->menu_primi_count; i++) {
        if (shm->st_primi.porzioni[i] < shm->MAXPORZIONIPRIMI)
            shm->st_primi.porzioni[i]++;
    }

    for (int i = 0; i < shm->menu_secondi_count; i++) {
        if (shm->st_secondi.porzioni[i] < shm->MAXPORZIONISECONDI)
            shm->st_secondi.porzioni[i]++;
    }
}

/* ---------------------------------------------------------
   Assegnazione operatori alle stazioni
   Regole:
   - almeno 1 operatore per stazione
   - operatori extra assegnati in base ai tempi medi (più lento → più operatori)
   --------------------------------------------------------- */
void stations_assign_workers(shm_t *shm) {
    printf("[STATIONS] Assegnazione operatori alle stazioni...\n");

    const int NUM_STATIONS = 4;
    int operatori_disponibili = shm->NOFWORKERS;

    if (operatori_disponibili < NUM_STATIONS) {
        printf("[STATIONS] ERRORE: Servono almeno %d operatori (1 per stazione), disponibili: %d\n",
               NUM_STATIONS, operatori_disponibili);
        exit(EXIT_FAILURE);
    }

    shm->st_primi.postazioni_totali = 1;
    shm->st_secondi.postazioni_totali = 1;
    shm->st_coffee.postazioni_totali = 1;
    shm->st_cassa.postazioni_totali = 1;
    operatori_disponibili -= NUM_STATIONS;

    printf("[STATIONS] Tempi medi di servizio (ns):\n");
    printf("  PRIMI:   %d ns\n", shm->AVGSRVCPRIMI);
    printf("  SECONDI: %d ns\n", shm->AVGSRVCMAINCOURSE);
    printf("  COFFEE:  %d ns\n", shm->AVGSRVCCOFFEE);
    printf("  CASSA:   %d ns\n", shm->AVGSRVCCASSA);

    int tempo_totale = shm->AVGSRVCPRIMI + shm->AVGSRVCMAINCOURSE + 
                       shm->AVGSRVCCOFFEE + shm->AVGSRVCCASSA;

    if (tempo_totale <= 0) {
        printf("[STATIONS] ATTENZIONE: tempi di servizio non validi, uso distribuzione uniforme\n");
        tempo_totale = 1;
    }

    /* Distribuisce gli operatori rimanenti in modo proporzionale ai tempi medi */
    if (operatori_disponibili > 0) {
        double ratio_primi = (double)shm->AVGSRVCPRIMI / tempo_totale;
        double ratio_secondi = (double)shm->AVGSRVCMAINCOURSE / tempo_totale;
        double ratio_coffee = (double)shm->AVGSRVCCOFFEE / tempo_totale;
        double ratio_cassa = (double)shm->AVGSRVCCASSA / tempo_totale;

        int extra_primi = (int)(ratio_primi * operatori_disponibili);
        int extra_secondi = (int)(ratio_secondi * operatori_disponibili);
        int extra_coffee = (int)(ratio_coffee * operatori_disponibili);
        int extra_cassa = (int)(ratio_cassa * operatori_disponibili);

        /* Assegna gli operatori extra */
        shm->st_primi.postazioni_totali += extra_primi;
        shm->st_secondi.postazioni_totali += extra_secondi;
        shm->st_coffee.postazioni_totali += extra_coffee;
        shm->st_cassa.postazioni_totali += extra_cassa;

        /* Gestisce eventuali operatori rimanenti per arrotondamento */
        int assegnati = extra_primi + extra_secondi + extra_coffee + extra_cassa;
        int rimanenti = operatori_disponibili - assegnati;

        /* Assegna i rimanenti alla stazione più lenta */
        if (rimanenti > 0) {
            int max_time = shm->AVGSRVCPRIMI;
            station_t *max_station = &shm->st_primi;

            if (shm->AVGSRVCMAINCOURSE > max_time) {
                max_time = shm->AVGSRVCMAINCOURSE;
                max_station = &shm->st_secondi;
            }
            if (shm->AVGSRVCCOFFEE > max_time) {
                max_time = shm->AVGSRVCCOFFEE;
                max_station = &shm->st_coffee;
            }
            if (shm->AVGSRVCCASSA > max_time) {
                max_station = &shm->st_cassa;
            }

            max_station->postazioni_totali += rimanenti;
        }
    }

    int total_assigned = shm->st_primi.postazioni_totali + 
                         shm->st_secondi.postazioni_totali +
                         shm->st_coffee.postazioni_totali + 
                         shm->st_cassa.postazioni_totali;

    printf("[STATIONS] Postazioni assegnate (su %d operatori):\n", shm->NOFWORKERS);
    printf("  PRIMI:   %d postazioni (%.1f%%)\n", shm->st_primi.postazioni_totali,
           100.0 * shm->st_primi.postazioni_totali / shm->NOFWORKERS);
    printf("  SECONDI: %d postazioni (%.1f%%)\n", shm->st_secondi.postazioni_totali,
           100.0 * shm->st_secondi.postazioni_totali / shm->NOFWORKERS);
    printf("  COFFEE:  %d postazioni (%.1f%%)\n", shm->st_coffee.postazioni_totali,
           100.0 * shm->st_coffee.postazioni_totali / shm->NOFWORKERS);
    printf("  CASSA:   %d postazioni (%.1f%%)\n", shm->st_cassa.postazioni_totali,
           100.0 * shm->st_cassa.postazioni_totali / shm->NOFWORKERS);
    printf("  TOTALE:  %d postazioni\n", total_assigned);
}

void stations_compute_leftovers(shm_t *shm) {
    shm->stats_giorno.piatti_primi_avanzati = 0;
    shm->stats_giorno.piatti_secondi_avanzati = 0;
    for (int i = 0; i < shm->menu_primi_count; i++)
        shm->stats_giorno.piatti_primi_avanzati += shm->st_primi.porzioni[i];

    for (int i = 0; i < shm->menu_secondi_count; i++)
        shm->stats_giorno.piatti_secondi_avanzati += shm->st_secondi.porzioni[i];
}