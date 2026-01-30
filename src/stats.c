#include <stdio.h>
#include <string.h>
#include "shared_structs.h"
#include "stats.h"


/* ---------------------------------------------------------
   Reset statistiche giornaliere
   --------------------------------------------------------- */
void stats_reset_day(stats_t *s) {

    memset(s, 0, sizeof(stats_t));

    s->ricavo_giornaliero = 0.0;
}

/* ---------------------------------------------------------
   Aggiornamento statistiche totali
   --------------------------------------------------------- */
void stats_update_totals(stats_t *tot, stats_t *day) {

    tot->utenti_serviti      += day->utenti_serviti;
    tot->utenti_non_serviti  += day->utenti_non_serviti;

    tot->piatti_primi_serviti   += day->piatti_primi_serviti;
    tot->piatti_secondi_serviti += day->piatti_secondi_serviti;
    tot->piatti_coffee_serviti  += day->piatti_coffee_serviti;

    tot->piatti_primi_avanzati   += day->piatti_primi_avanzati;
    tot->piatti_secondi_avanzati += day->piatti_secondi_avanzati;

    tot->tempo_attesa_primi_ns   += day->tempo_attesa_primi_ns;
    tot->tempo_attesa_secondi_ns += day->tempo_attesa_secondi_ns;
    tot->tempo_attesa_coffee_ns  += day->tempo_attesa_coffee_ns;
    tot->tempo_attesa_cassa_ns   += day->tempo_attesa_cassa_ns;

    tot->operatori_attivi += day->operatori_attivi;
    tot->pause_totali     += day->pause_totali;

    tot->ricavo_giornaliero += day->ricavo_giornaliero;
}

/* ---------------------------------------------------------
   Stampa statistiche giornaliere
   --------------------------------------------------------- */
void stats_print_day(stats_t *s, int day) {

    printf("\n================== STATISTICHE GIORNO %d ==================\n", day);

    printf("Utenti serviti:            %d\n", s->utenti_serviti);
    printf("Utenti non serviti:        %d\n", s->utenti_non_serviti);
    printf("Utenti in attesa (fine giornata): %d\n", s->utenti_in_attesa);

    printf("\nPiatti serviti:\n");
    printf("  Primi:                   %d\n", s->piatti_primi_serviti);
    printf("  Secondi:                 %d\n", s->piatti_secondi_serviti);
    printf("  Coffee/Dolci:            %d\n", s->piatti_coffee_serviti);

    printf("\nPiatti avanzati:\n");
    printf("  Primi:                   %d\n", s->piatti_primi_avanzati);
    printf("  Secondi:                 %d\n", s->piatti_secondi_avanzati);

    printf("\nTempi medi di attesa (ms):\n");

    long avg_primi   = s->utenti_serviti ? s->tempo_attesa_primi_ns   / 1000000 / s->utenti_serviti : 0;
    long avg_secondi = s->utenti_serviti ? s->tempo_attesa_secondi_ns / 1000000 / s->utenti_serviti : 0;
    long avg_coffee  = s->utenti_serviti ? s->tempo_attesa_coffee_ns  / 1000000 / s->utenti_serviti : 0;
    long avg_cassa   = s->utenti_serviti ? s->tempo_attesa_cassa_ns   / 1000000 / s->utenti_serviti : 0;

    printf("  Stazione primi:          %ld ms\n", avg_primi);
    printf("  Stazione secondi:        %ld ms\n", avg_secondi);
    printf("  Stazione coffee:         %ld ms\n", avg_coffee);
    printf("  Cassa:                   %ld ms\n", avg_cassa);

    printf("\nOperatori attivi:          %d\n", s->operatori_attivi);
    printf("Pause totali:              %d\n", s->pause_totali);

    printf("\nRicavo giornaliero:        %.2f €\n", s->ricavo_giornaliero);

    printf("===========================================================\n\n");
}

/* ---------------------------------------------------------
   Stampa statistiche finali
   --------------------------------------------------------- */
void stats_print_final(stats_t *tot, int giorni) {

    printf("\n================== STATISTICHE FINALI ==================\n");

    /* Utenti */
    printf("\nUTENTI:\n");
    printf("Utenti serviti totali:     %d\n", tot->utenti_serviti);
    printf("Utenti non serviti totali: %d\n", tot->utenti_non_serviti);
    if (giorni > 0) {
        printf("Utenti serviti in media al giorno:     %.2f\n", 
               (double)tot->utenti_serviti / giorni);
        printf("Utenti non serviti in media al giorno: %.2f\n", 
               (double)tot->utenti_non_serviti / giorni);
    }

    /* Piatti serviti */
    printf("\nPIATTI DISTRIBUITI:\n");
    int tot_piatti_serviti = tot->piatti_primi_serviti + 
                             tot->piatti_secondi_serviti + 
                             tot->piatti_coffee_serviti;
    printf("Totali:                  %d\n", tot_piatti_serviti);
    printf("  Primi:                 %d\n", tot->piatti_primi_serviti);
    printf("  Secondi:               %d\n", tot->piatti_secondi_serviti);
    printf("  Coffee/Dolci:          %d\n", tot->piatti_coffee_serviti);
    if (giorni > 0) {
        printf("\nPiatti distribuiti in media al giorno:\n");
        printf("Totali:                  %.2f\n", (double)tot_piatti_serviti / giorni);
        printf("  Primi:                 %.2f\n", (double)tot->piatti_primi_serviti / giorni);
        printf("  Secondi:               %.2f\n", (double)tot->piatti_secondi_serviti / giorni);
        printf("  Coffee/Dolci:          %.2f\n", (double)tot->piatti_coffee_serviti / giorni);
    }

    /* Piatti avanzati */
    printf("\nPIATTI AVANZATI:\n");
    int tot_piatti_avanzati = tot->piatti_primi_avanzati + 
                              tot->piatti_secondi_avanzati;
    printf("Totali:                  %d\n", tot_piatti_avanzati);
    printf("  Primi:                 %d\n", tot->piatti_primi_avanzati);
    printf("  Secondi:               %d\n", tot->piatti_secondi_avanzati);
    if (giorni > 0) {
        printf("\nPiatti avanzati in media al giorno:\n");
        printf("Totali:                  %.2f\n", (double)tot_piatti_avanzati / giorni);
        printf("  Primi:                 %.2f\n", (double)tot->piatti_primi_avanzati / giorni);
        printf("  Secondi:               %.2f\n", (double)tot->piatti_secondi_avanzati / giorni);
    }

    /* Tempi di attesa */
    printf("\nTEMPI MEDI DI ATTESA:\n");
    
    if (tot->utenti_serviti > 0) {
        long avg_primi   = tot->tempo_attesa_primi_ns   / 1000000 / tot->utenti_serviti;
        long avg_secondi = tot->tempo_attesa_secondi_ns / 1000000 / tot->utenti_serviti;
        long avg_coffee  = tot->tempo_attesa_coffee_ns  / 1000000 / tot->utenti_serviti;
        long avg_cassa   = tot->tempo_attesa_cassa_ns   / 1000000 / tot->utenti_serviti;
        
        long avg_complessivo = (avg_primi + avg_secondi + avg_coffee + avg_cassa) / 4;
        
        printf("Tempo medio complessivo:     %ld ms\n", avg_complessivo);
        printf("  Stazione primi:            %ld ms\n", avg_primi);
        printf("  Stazione secondi:          %ld ms\n", avg_secondi);
        printf("  Stazione coffee:           %ld ms\n", avg_coffee);
        printf("  Cassa:                     %ld ms\n", avg_cassa);
    } else {
        printf("Nessun utente servito\n");
    }

    /* Operatori e pause */
    printf("\nOPERATORI:\n");
    printf("Operatori attivi totali:     %d\n", tot->operatori_attivi);
    printf("Pause totali:                %d\n", tot->pause_totali);
    if (giorni > 0) {
        printf("Pause medie per giornata:    %.2f\n", (double)tot->pause_totali / giorni);
    }

    /* Ricavi */
    printf("\nRICAVI:\n");
    printf("Ricavo totale:               %.2f €\n", tot->ricavo_giornaliero);
    if (giorni > 0) {
        printf("Ricavo medio per giornata:   %.2f €\n", tot->ricavo_giornaliero / giorni);
    }

    printf("=========================================================\n\n");
}