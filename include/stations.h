#ifndef STATIONS_H
#define STATIONS_H

#include "shared_structs.h"

void stations_init(shm_t *shm);
void stations_refill_day(shm_t *shm);
void stations_refill_periodic(shm_t *shm);
void stations_assign_workers(shm_t *shm);
void stations_compute_leftovers(shm_t *shm);

#endif