#include "lifehashmap.h"

uint32_t hash(int x, int y, uint32_t modulo)
{
    uint32_t combined = (((uint32_t) x) << 16) | y;

    // Bit mixing
    combined ^= combined >> 16;
    combined *= 0x27d4eb2d;
    combined ^= combined >> 13;

    return combined % modulo;
}

lifeHashMap *innit(uint32_t size, int width, int height, int benchmark)
{
    cellNode **buckets = NULL;
    lifeHashMap *map = NULL;
    pthread_mutex_t *locks = NULL;

    map = calloc(1, sizeof(lifeHashMap));
    buckets = calloc(size, sizeof(cellNode *));
    locks = calloc(size, sizeof(pthread_mutex_t));
    if (!map || !buckets || !locks) {
        goto err;
    }

    map->size = size;
    map->buckets = buckets;
    map->locks = locks;

    for (uint32_t i = 0; i < size; i++) {
        pthread_mutex_init(&(map->locks[i]), NULL);
    }

    if (benchmark) {
        srand(0); // set consistant seed when benchmarking
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            lifemap_set(map, (cell) { .x = x, .y = y, .state = rand() % 5 });
        }
    }

    return map;

err:
    free(buckets);
    free(map);
    free(locks);
    return NULL;
}

// NULL means EMPTY
cell *lifemap_get(lifeHashMap *map, int x, int y)
{
    uint32_t key = hash(x, y, map->size);
    cellNode *curr = map->buckets[key];
    while (curr) {
        if (curr->c.x == x && curr->c.y == y) {
            return &curr->c;
        }
        curr = curr->next;
    }
    return NULL;
}

bool lifemap_del(lifeHashMap *map, cell c)
{
    uint32_t key = hash(c.x, c.y, map->size);
    cellNode *curr = map->buckets[key];
    // bucket is empty
    if (curr == NULL) {
        return false;
    }
    // remove in head
    if (curr->c.x == c.x && curr->c.y == c.y) {
        cellNode *tmp = curr;
        map->buckets[key] = curr->next;
        free(tmp);

        return true;
    }

    // remove in tail
    while (curr->next) {
        if (curr->next->c.x == c.x && curr->next->c.y == c.y) {
            cellNode *tmp = curr->next;
            curr->next = curr->next->next;
            free(tmp);
            return true;
        }
        curr = curr->next;
    }

    return false;
}
// create new or set existing to new value
bool __lifemap_set(lifeHashMap *map, cell c, uint32_t key)
{
    if (c.state == EMPTY) {
        return lifemap_del(map, c);
    }

    // try changing value if the node exists
    for (cellNode *curr = map->buckets[key]; curr; curr = curr->next) {
        if (curr->c.x == c.x && curr->c.y == c.y) {
            curr->c.state = c.state;
            return true;
        }
    }

    cellNode *new_node = malloc(sizeof(cellNode));
    if (new_node == NULL)
        return false;

    new_node->c = c;
    new_node->next = map->buckets[key];
    map->buckets[key] = new_node;

    return true;
}

bool lifemap_set(lifeHashMap *map, cell c)
{
    uint32_t key = hash(c.x, c.y, map->size);
    pthread_mutex_lock(&(map->locks[key]));
    bool result = __lifemap_set(map, c, key);
    pthread_mutex_unlock(&(map->locks[key]));
    return result;
}

void lifemap_free(lifeHashMap *map)
{
    for (uint32_t i = 0; i < map->size; i++) {
        pthread_mutex_destroy(&(map->locks[i]));
    }
    free(map->locks);

    for (uint32_t i = 0; i < map->size; i++) {
        cellNode *curr = map->buckets[i];
        while (curr) {
            cellNode *tmp = curr;
            curr = curr->next;
            free(tmp);
        }
        map->buckets[i] = NULL;
    }
    free(map->buckets);
    free(map);
}
