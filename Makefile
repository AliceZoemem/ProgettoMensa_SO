# ============================================================
# Makefile - Progetto Mensa (Sistemi Operativi)
# ============================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -std=gnu99 -g
INCLUDES = -Iinclude
LDFLAGS = -pthread

SRC_DIR = src
OBJ_DIR = obj

# Lista dei file sorgenti
SRCS_COMMON = $(SRC_DIR)/ipc.c $(SRC_DIR)/stations.c $(SRC_DIR)/stats.c \
              $(SRC_DIR)/config.c $(SRC_DIR)/util.c

OBJS_COMMON = $(SRCS_COMMON:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Target principali
all: mensa operatore utente

# ------------------------------------------------------------
# Eseguibile principale: mensa
# ------------------------------------------------------------
mensa: $(OBJ_DIR)/mensa.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) $(INCLUDES) -o mensa $(OBJ_DIR)/mensa.o $(OBJS_COMMON) $(LDFLAGS)

# ------------------------------------------------------------
# Processo operatore
# ------------------------------------------------------------
operatore: $(OBJ_DIR)/operatore.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) $(INCLUDES) -o operatore $(OBJ_DIR)/operatore.o $(OBJS_COMMON) $(LDFLAGS)

# ------------------------------------------------------------
# Processo utente
# ------------------------------------------------------------
utente: $(OBJ_DIR)/utente.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) $(INCLUDES) -o utente $(OBJ_DIR)/utente.o $(OBJS_COMMON) $(LDFLAGS)

# ------------------------------------------------------------
# Regola generica per compilare i .c in obj/
# ------------------------------------------------------------
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# ------------------------------------------------------------
# Pulizia
# ------------------------------------------------------------
clean:
	rm -rf $(OBJ_DIR) mensa operatore utente

# ------------------------------------------------------------
# Esecuzione rapida
# ------------------------------------------------------------
run: all
	./mensa

# ------------------------------------------------------------
# Test diverse configurazioni
# ------------------------------------------------------------
test-timeout: all
	@echo "=========================================="
	@echo "Test configurazione TIMEOUT"
	@echo "=========================================="
	./mensa config_timeout.conf

test-overload: all
	@echo "=========================================="
	@echo "Test configurazione OVERLOAD"
	@echo "=========================================="
	./mensa config_overload.conf

test-all: test-timeout test-overload
	@echo "=========================================="
	@echo "Tutti i test completati"
	@echo "=========================================="