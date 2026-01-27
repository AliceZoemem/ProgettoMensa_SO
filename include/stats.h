#ifndef STATS_H
#define STATS_H

#include "shared_structs.h"

void stats_reset_day(stats_t *s);
void stats_print_day(stats_t *s, int day);
void stats_print_final(stats_t *s);
void stats_update_totals(stats_t *tot, stats_t *day);

#endif