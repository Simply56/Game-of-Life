#include "lifehashmap.h"

uint16_t hash(uint16_t x, uint16_t y, uint16_t modulo)
{
    uint32_t combined = (((uint32_t) x) << 16) | y;

    // Bit mixing
    combined ^= combined >> 16;
    combined *= 0x27d4eb2d;
    combined ^= combined >> 13;

    return combined % modulo;
}

lifeHashMap *innit(uint16_t size, int width, int height)
{
    cellNode **slots = NULL;
    lifeHashMap *map = NULL;
    pthread_mutex_t *locks = NULL;
    map = calloc(1, sizeof(lifeHashMap));
    slots = calloc(size, sizeof(cellNode *));
    locks = calloc(size, sizeof(pthread_mutex_t));
    if (!map || !slots || !locks) {
        goto err;
    }
    map->size = size;
    map->slots = slots;
    map->locks = locks;

    // crete mutexes
    for (uint16_t i = 0; i < size; i++) {
        pthread_mutex_init(&(map->locks[i]), NULL);
    }

    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            lifemap_set(map, (cell){ .x = x, .y = y, .state = rand() % 3 });
        }
    }

    return map;

err:
    free(slots);
    free(map);
    free(locks);
    return NULL;
}

// NULL means EMPTY
cell *__lifemap_get(lifeHashMap *map, uint16_t x, uint16_t y)
{
    uint16_t key = hash(x, y, map->size);
    cellNode *curr = map->slots[key];
    while (curr) {
        if (curr->c.x == x && curr->c.y == y) {
            return &curr->c;
        }
        curr = curr->next;
    }
    return NULL;
}
cell *lifemap_get(lifeHashMap *map, uint16_t x, uint16_t y)
{
    uint16_t key = hash(x, y, map->size);
    pthread_mutex_lock(&(map->locks[key]));
    cell *result = __lifemap_get(map, x, y);
    pthread_mutex_unlock(&(map->locks[key]));
    return result;
}

bool lifemap_del(lifeHashMap *map, cell c)
{
    // printf("removing: ");
    // print_cell(c);

    uint16_t key = hash(c.x, c.y, map->size);
    cellNode *curr = map->slots[key];
    if (curr == NULL) {
        return false;
    }

    // remove head
    if (curr->c.x == c.x && curr->c.y == c.y) {
        cellNode *tmp = curr;
        map->slots[key] = curr->next;
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
bool __lifemap_set(lifeHashMap *map, cell c)
{
    // printf("x: %d, ", c.x);
    // printf("y: %d\n", c.y);
    if (c.state == EMPTY) {
        lifemap_del(map, c);
        return true;
    }
    uint16_t key = hash(c.x, c.y, map->size);
    cellNode *curr = map->slots[key];
    if (curr == NULL) {
        cellNode *new_node = calloc(1, sizeof(cellNode));
        if (new_node == NULL)
            return false;

        new_node->c = c;
        map->slots[key] = new_node;
        return true;
    }

    while (curr->next) {
        if (curr->c.x == c.x && curr->c.y == c.y) {
            curr->c.state = c.state;
            // puts("hit");
            return true;
        }
        curr = curr->next;
    }
    cellNode *new_node = calloc(1, sizeof(cellNode));
    if (new_node == NULL)
        return false;

    new_node->c = c;
    curr->next = new_node;
    return true;
}

bool lifemap_set(lifeHashMap *map, cell c)
{
    uint16_t key = hash(c.x, c.y, map->size);
    pthread_mutex_lock(&(map->locks[key]));
    bool result = __lifemap_set(map, c);
    pthread_mutex_unlock(&(map->locks[key]));
    return result;
}

void lifemap_free(lifeHashMap *map)
{
    for (uint16_t i = 0; i < map->size; i++) {
        pthread_mutex_destroy(&(map->locks[i]));
    }
    free(map->locks);

    for (uint16_t i = 0; i < map->size; i++) {
        cellNode *curr = map->slots[i];
        while (curr) {
            cellNode *tmp = curr;
            curr = curr->next;
            free(tmp);
        }
        map->slots[i] = NULL;
    }
    free(map->slots);
    free(map);
}

void print_cell(cell c)
{
    char *s = NULL;
    switch (c.state) {
    case BLUE:
        s = "blue";
        break;
    case ORANGE:
        s = "orange";
        break;
    case EMPTY:
        s = "empty";
        break;
    case DEAD:
        s = "dead";
        break;

    default:
        break;
    }
    if (s != NULL) {
        printf("%d,%d: %s", c.x, c.y, s);
    } else {
        printf("%d,%d: %d", c.x, c.y, c.state);
    }
}

void print_map(lifeHashMap *map)
{
    puts("--------------------");
    for (uint16_t i = 0; i < map->size; i++) {
        cellNode *curr = map->slots[i];
        printf("[%d]: ", i);
        pthread_mutex_lock(&(map->locks[i]));
        while (curr) {
            print_cell(curr->c);
            printf(", ");
            curr = curr->next;
        }
        puts("");
        pthread_mutex_unlock(&(map->locks[i]));
    }
    puts("--------------------");
}