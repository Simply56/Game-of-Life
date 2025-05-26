#pragma once
#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* game of life hashmap using a singly linked list as buckets */

#define EMPTY ((uint8_t) 0)
#define BLUE ((uint8_t) 1)
#define ORANGE ((uint8_t) 2)
#define DEAD ((uint8_t) 3)

typedef struct cell
{
    int x;
    int y;
    uint8_t state;
} cell;

typedef struct cellNode
{
    cell c;
    struct cellNode *next;
} cellNode;

typedef struct lifeHashMap
{
    cellNode **buckets;      // fixed width array of pointers
    uint32_t size;           // number of buckets (linked lists)
    pthread_mutex_t *locks; // fixed width array
} lifeHashMap;


void print_cell(cell c);
void print_map(lifeHashMap *map);
size_t count_items(lifeHashMap *map); // used for calculating the load factor

lifeHashMap *innit(uint32_t size, int width, int height, int benchmark);
void lifemap_free(lifeHashMap *map);

cell *lifemap_get(lifeHashMap *map, int x, int y);
bool lifemap_set(lifeHashMap *map, cell c);
