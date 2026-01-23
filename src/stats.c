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
void stats_print_final(stats_t *tot) {

    printf("\n================== STATISTICHE FINALI ==================\n");

    printf("Utenti serviti totali:     %d\n", tot->utenti_serviti);
    printf("Utenti non serviti totali: %d\n", tot->utenti_non_serviti);

    printf("\nPiatti serviti totali:\n");
    printf("  Primi:                   %d\n", tot->piatti_primi_serviti);
    printf("  Secondi:                 %d\n", tot->piatti_secondi_serviti);
    printf("  Coffee/Dolci:            %d\n", tot->piatti_coffee_serviti);

    printf("\nPiatti avanzati totali:\n");
    printf("  Primi:                   %d\n", tot->piatti_primi_avanzati);
    printf("  Secondi:                 %d\n", tot->piatti_secondi_avanzati);

    printf("\nTempi medi di attesa globali (ms):\n");

    long avg_primi   = tot->utenti_serviti ? tot->tempo_attesa_primi_ns   / 1000000 / tot->utenti_serviti : 0;
    long avg_secondi = tot->utenti_serviti ? tot->tempo_attesa_secondi_ns / 1000000 / tot->utenti_serviti : 0;
    long avg_coffee  = tot->utenti_serviti ? tot->tempo_attesa_coffee_ns  / 1000000 / tot->utenti_serviti : 0;
    long avg_cassa   = tot->utenti_serviti ? tot->tempo_attesa_cassa_ns   / 1000000 / tot->utenti_serviti : 0;

    printf("  Stazione primi:          %ld ms\n", avg_primi);
    printf("  Stazione secondi:        %ld ms\n", avg_secondi);
    printf("  Stazione coffee:         %ld ms\n", avg_coffee);
    printf("  Cassa:                   %ld ms\n", avg_cassa);

    printf("\nOperatori attivi totali:   %d\n", tot->operatori_attivi);
    printf("Pause totali:              %d\n", tot->pause_totali);

    printf("\nRicavo totale:             %.2f €\n", tot->ricavo_giornaliero);

    printf("=========================================================\n\n");
}