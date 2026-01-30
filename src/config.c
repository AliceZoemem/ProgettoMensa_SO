#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared_structs.h"
#include "config.h"


extern shm_t *shm;

/* ---------------------------------------------------------
   Lettura file config.txt
   --------------------------------------------------------- */
int load_config(void) {
    return load_config_from_file("config.txt");
}

int load_config_from_file(const char *filename) {

    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "[CONFIG] Impossibile aprire %s\n", filename);
        perror("[CONFIG]");
        return -1;
    }
    
    printf("[CONFIG] Caricamento configurazione da %s\n", filename);

    char line[256];
    char key[64];

    while (fgets(line, sizeof(line), f)) {
        /* Ignora commenti e righe vuote */
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        
        /* Prova a leggere come double (per i prezzi) */
        double dvalue;
        if (sscanf(line, "%63s %lf", key, &dvalue) == 2) {
            
            /* Parametri in formato double */
            if (strcmp(key, "PRICEPRIMI") == 0) {
                shm->PRICEPRIMI = dvalue;
                continue;
            }
            else if (strcmp(key, "PRICESECONDI") == 0) {
                shm->PRICESECONDI = dvalue;
                continue;
            }
            else if (strcmp(key, "PRICECOFFEE") == 0) {
                shm->PRICECOFFEE = dvalue;
                continue;
            }
            
            /* Parametri interi */
            long value = (long)dvalue;

        if (strcmp(key, "NOFWORKERS") == 0)
            shm->NOFWORKERS = value;

        else if (strcmp(key, "NOFUSERS") == 0)
            shm->NOFUSERS = value;

        else if (strcmp(key, "SIMDURATION") == 0)
            shm->SIMDURATION = value;

        else if (strcmp(key, "NNANOSECS") == 0)
            shm->NNANOSECS = value;

        else if (strcmp(key, "OVERLOADTHRESHOLD") == 0)
            shm->OVERLOADTHRESHOLD = value;

        else if (strcmp(key, "NOFTABLESEATS") == 0)
            shm->NOFTABLESEATS = value;
        else if (strcmp(key, "NOFWKSEATSPRIMI") == 0)
            shm->NOFWKSEATSPRIMI = value;
        
        else if (strcmp(key, "NOFWKSEATSSECONDI") == 0)
            shm->NOFWKSEATSSECONDI = value;
        
        else if (strcmp(key, "NOFWKSEATSCOFFEE") == 0)
            shm->NOFWKSEATSCOFFEE = value;
        
        else if (strcmp(key, "NOFWKSEATSCASSA") == 0)
            shm->NOFWKSEATSCASSA = value;

        else if (strcmp(key, "AVGREFILLPRIMI") == 0)
            shm->AVGREFILLPRIMI = value;

        else if (strcmp(key, "AVGREFILLSECONDI") == 0)
            shm->AVGREFILLSECONDI = value;

        else if (strcmp(key, "MAXPORZIONIPRIMI") == 0)
            shm->MAXPORZIONIPRIMI = value;

        else if (strcmp(key, "MAXPORZIONISECONDI") == 0)
            shm->MAXPORZIONISECONDI = value;

        else if (strcmp(key, "AVGSRVCPRIMI") == 0)
            shm->AVGSRVCPRIMI = value;

        else if (strcmp(key, "AVGSRVCMAINCOURSE") == 0)
            shm->AVGSRVCMAINCOURSE = value;

        else if (strcmp(key, "AVGSRVCCOFFEE") == 0)
            shm->AVGSRVCCOFFEE = value;

        else if (strcmp(key, "AVGSRVCCASSA") == 0)
            shm->AVGSRVCCASSA = value;

        else if (strcmp(key, "NOFPAUSE") == 0)
            shm->NOFPAUSE = value;

        else {
            printf("[CONFIG] Parametro sconosciuto: %s\n", key);
        }
        }
    }

    fclose(f);
    return 0;
}

/* ---------------------------------------------------------
   Lettura file menu.txt
   Formato esempio:
   PRIMI: pasta,risotto,gnocchi
   SECONDI: pollo,manzo,pesce
   COFFEE: normale,macchiato,decaffeinato,ginseng
   --------------------------------------------------------- */
int load_menu(void) {

    FILE *f = fopen("menu.txt", "r");
    if (!f) {
        perror("[CONFIG] Impossibile aprire menu.txt");
        return -1;
    }

    char line[256];

    while (fgets(line, sizeof(line), f)) {

        if (strncmp(line, "PRIMI:", 6) == 0) {
            // parsing semplice: conta quanti piatti ci sono
            int count = 0;
            char *tok = strtok(line + 6, ",\n");
            while (tok && count < MAX_PRIMI_TYPES) {
                shm->menu_primi[count++] = strdup(tok);
                tok = strtok(NULL, ",\n");
            }
            shm->menu_primi_count = count;
        }

        else if (strncmp(line, "SECONDI:", 8) == 0) {
            int count = 0;
            char *tok = strtok(line + 8, ",\n");
            while (tok && count < MAX_SECONDI_TYPES) {
                shm->menu_secondi[count++] = strdup(tok);
                tok = strtok(NULL, ",\n");
            }
            shm->menu_secondi_count = count;
        }

        else if (strncmp(line, "COFFEE:", 7) == 0) {
            int count = 0;
            char *tok = strtok(line + 7, ",\n");
            while (tok && count < 4) {
                shm->menu_coffee[count++] = strdup(tok);
                tok = strtok(NULL, ",\n");
            }
            shm->menu_coffee_count = count;
        }
    }

    fclose(f);
    return 0;
}