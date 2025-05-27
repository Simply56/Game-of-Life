#define _POSIX_C_SOURCE 200809L

#include "lifehashmap.h"
#include "threadpool.h"

#include <stdio.h>
#include <stdlib.h>

void print_load_factor(lifeHashMap *map, double map_size);
void print_fps(lifeHashMap *map, double map_size);

void print_cell(cell c);
size_t count_items(lifeHashMap *map);
void print_map(lifeHashMap *map);