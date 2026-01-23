#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "shared_structs.h"
#include "stations.h"
#include "util.h"

/* ---------------------------------------------------------
   Inizializzazione delle stazioni
   --------------------------------------------------------- */
void stations_init(shm_t *shm) {

    printf("[STATIONS] Inizializzazione stazioni...\n");

    /* PRIMI */
    shm->st_primi.postazioni_totali   = 0;
    shm->st_primi.postazioni_occupate = 0;
    memset(shm->st_primi.porzioni, 0, sizeof(shm->st_primi.porzioni));
    shm->st_primi.tempo_attesa_totale_ns = 0;
    shm->st_primi.utenti_serviti = 0;
    shm->st_primi.utenti_in_coda = 0;

    /* SECONDI */
    shm->st_secondi.postazioni_totali   = 0;
    shm->st_secondi.postazioni_occupate = 0;
    memset(shm->st_secondi.porzioni, 0, sizeof(shm->st_secondi.porzioni));
    shm->st_secondi.tempo_attesa_totale_ns = 0;
    shm->st_secondi.utenti_serviti = 0;
    shm->st_secondi.utenti_in_coda = 0;

    /* COFFEE (porzioni infinite) */
    shm->st_coffee.postazioni_totali   = 0;
    shm->st_coffee.postazioni_occupate = 0;
    for (int i = 0; i < MAX_COFFEE_TYPES; i++)
        shm->st_coffee.porzioni[i] = -1; // infinito
    shm->st_coffee.tempo_attesa_totale_ns = 0;
    shm->st_coffee.utenti_serviti = 0;
    shm->st_coffee.utenti_in_coda = 0;

    /* CASSA */
    shm->st_cassa.postazioni_totali   = 0;
    shm->st_cassa.postazioni_occupate = 0;
    for (int i = 0; i < MAX_PRIMI_TYPES; i++)
        shm->st_cassa.porzioni[i] = -1;
    shm->st_cassa.tempo_attesa_totale_ns = 0;
    shm->st_cassa.utenti_serviti = 0;
    shm->st_cassa.utenti_in_coda = 0;

    /* Tavoli */
    shm->tavoli_liberi = shm->NOFTABLESEATS;
}

/* ---------------------------------------------------------
   Refill iniziale del giorno
   --------------------------------------------------------- */
void stations_refill_day(shm_t *shm) {

    printf("[STATIONS] Refill iniziale del giorno...\n");

    /* PRIMI */
    for (int i = 0; i < shm->menu_primi_count; i++)
        shm->st_primi.porzioni[i] = shm->AVGREFILLPRIMI;

    /* SECONDI */
    for (int i = 0; i < shm->menu_secondi_count; i++)
        shm->st_secondi.porzioni[i] = shm->AVGREFILLSECONDI;
}

/* ---------------------------------------------------------
   Refill periodico (ogni 10 minuti simulati)
   Chiamata da mensa.c durante il giorno
   --------------------------------------------------------- */
void stations_refill_periodic(shm_t *shm) {

    /* PRIMI */
    for (int i = 0; i < shm->menu_primi_count; i++) {
        if (shm->st_primi.porzioni[i] < shm->MAXPORZIONIPRIMI)
            shm->st_primi.porzioni[i]++;
    }

    /* SECONDI */
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

    int workers = shm->NOFWORKERS;

    /* 1. Ogni stazione deve avere almeno un operatore */
    shm->st_primi.postazioni_totali   = 1;
    shm->st_secondi.postazioni_totali = 1;
    shm->st_coffee.postazioni_totali  = 1;
    shm->st_cassa.postazioni_totali   = 1;

    workers -= 4;

    if (workers < 0) {
        printf("[STATIONS] ERRORE: operatori insufficienti!\n");
        exit(EXIT_FAILURE);
    }

    /* 2. Distribuzione operatori rimanenti in base ai tempi medi */
    while (workers > 0) {

        /* Ordine di priorità (più lento → più operatori) */
        if (shm->AVGSRVCMAINCOURSE >= shm->AVGSRVCPRIMI &&
            shm->AVGSRVCMAINCOURSE >= shm->AVGSRVCCOFFEE &&
            shm->AVGSRVCMAINCOURSE >= shm->AVGSRVCCASSA) {

            shm->st_secondi.postazioni_totali++;
        }
        else if (shm->AVGSRVCPRIMI >= shm->AVGSRVCCOFFEE &&
                 shm->AVGSRVCPRIMI >= shm->AVGSRVCCASSA) {

            shm->st_primi.postazioni_totali++;
        }
        else if (shm->AVGSRVCCOFFEE >= shm->AVGSRVCCASSA) {

            shm->st_coffee.postazioni_totali++;
        }
        else {
            shm->st_cassa.postazioni_totali++;
        }

        workers--;
    }

    printf("[STATIONS] Postazioni assegnate:\n");
    printf("  PRIMI:   %d\n", shm->st_primi.postazioni_totali);
    printf("  SECONDI: %d\n", shm->st_secondi.postazioni_totali);
    printf("  COFFEE:  %d\n", shm->st_coffee.postazioni_totali);
    printf("  CASSA:   %d\n", shm->st_cassa.postazioni_totali);
}

/* ---------------------------------------------------------
   Calcolo piatti avanzati a fine giornata
   --------------------------------------------------------- */
void stations_compute_leftovers(shm_t *shm) {

    /* PRIMI */
    for (int i = 0; i < shm->menu_primi_count; i++)
        shm->stats_giorno.piatti_primi_avanzati += shm->st_primi.porzioni[i];

    /* SECONDI */
    for (int i = 0; i < shm->menu_secondi_count; i++)
        shm->stats_giorno.piatti_secondi_avanzati += shm->st_secondi.porzioni[i];
}