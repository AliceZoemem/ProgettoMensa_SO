# Progetto Mensa - Sistemi Operativi

## Compilazione

```bash
make clean
make
```

## Esecuzione

### Con configurazione di default (config.txt)
```bash
./mensa
```

### Con configurazione specifica
```bash
./mensa config_timeout.conf
./mensa config_overload.conf
```

### Test automatici
```bash
make test-timeout      # Test terminazione per TIMEOUT
make test-overload     # Test terminazione per OVERLOAD
make test-all          # Esegue entrambi i test
```

## File di Configurazione

### config_timeout.conf
Configurazione bilanciata per testare la terminazione per **TIMEOUT**:
- 10 operatori, 30 utenti
- Durata: 5 giorni
- Soglia overload: 50 utenti
- Servizio rapido (1ms) e postazioni sufficienti
- **Risultato atteso**: La simulazione completa tutti i 5 giorni e termina con causa TIMEOUT

### config_overload.conf
Configurazione sbilanciata per testare la terminazione per **OVERLOAD**:
- 4 operatori, 100 utenti
- Durata: 10 giorni (ma si interrompe prima)
- Soglia overload: 15 utenti
- Servizio lento (3-5ms) e poche postazioni
- **Risultato atteso**: La simulazione si interrompe prima del giorno 10 a causa di troppi utenti in attesa (>15)

## Condizioni di Terminazione

La simulazione termina in uno dei seguenti casi:

1. **TIMEOUT**: Raggiungimento della durata impostata (SIM_DURATION giorni)
2. **OVERLOAD**: Numero di utenti in attesa a fine giornata > OVERLOAD_THRESHOLD

## Output

Al termine della simulazione, il programma stampa:
- **Causa di terminazione** (TIMEOUT o OVERLOAD)
- **Statistiche finali**:
  - Utenti serviti e non serviti
  - Piatti distribuiti e avanzati
  - Tempi medi di attesa
  - Statistiche operatori e pause
  - Ricavi totali

## Menu

Il menu è definito nel file `menu.txt` con la seguente sintassi:
```
PRIMI: pasta,risotto,gnocchi
SECONDI: pollo,manzo,pesce
COFFEE: normale,macchiato,decaffeinato,ginseng
```

## Note

- Gli utenti in attesa vengono contati alla fine di ogni giornata
- Il controllo overload avviene dopo ogni giornata completata
- I processi vengono terminati correttamente in entrambi i casi
