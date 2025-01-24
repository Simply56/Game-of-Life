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

struct cell
{
    int x;
    int y;
    uint8_t state;
} typedef cell;

/* CONVERSION TO BINARY TREE FROM LINKED LIST
 * The simplest possible iterator stores the last seen key, and then on the next iteration,
 * searches the tree for the least upper bound for that key. Iteration is O(log n).
 * This has the advantage of being very simple. If keys are small then the iterators are also small.
 * Of course it has the disadvantage of being a relatively slow way of iterating through the tree. 
 * It also won't work for non-unique sequences.
 * my note: static variables are not thread safe, so use pointers to next instead
 */

struct cellNode
{
    cell c;
    struct cellNode *next;
} typedef cellNode;

struct lifeHashMap
{
    cellNode **buckets; // array of pointers
    uint32_t size;
    pthread_rwlock_t *locks; // fixed width array
} typedef lifeHashMap;

void print_cell(cell c);
void print_map(lifeHashMap *map);
size_t count_items(lifeHashMap * map);

lifeHashMap *innit(uint32_t size, int width, int height, int benchmark);
void lifemap_free(lifeHashMap *map);

cell *lifemap_get(lifeHashMap *map, int x, int y);
bool lifemap_set(lifeHashMap *map, cell c);
