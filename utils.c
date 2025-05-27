#include "utils.h"

void print_load_factor(lifeHashMap *map, double map_size)
{
    double load_factor = (double) count_items(map) / (double) map_size;

    fprintf(stderr, "Load factor: %f\n", load_factor);
}

void print_fps(lifeHashMap *map, double map_size)
{
    static int frame_count = 0;
    static double last_time = 0;
    static double fps = 0.0;

    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    double now = current_time.tv_sec + (current_time.tv_nsec / 1e9);

    frame_count++;

    // Calculate FPS every second
    if (now - last_time >= 1.0) {
        fps = frame_count / (now - last_time);
        frame_count = 0;
        last_time = now;
        fprintf(stderr, "FPS: %.2f\n", fps);
        print_load_factor(map, map_size);
    }
}


size_t count_items(lifeHashMap *map)
{
    size_t result = 0;
    for (uint32_t i = 0; i < map->size; i++) {
        cellNode *curr = map->buckets[i];
        while (curr) {
            result++;
            curr = curr->next;
        }
    }
    return result;
}

// helper functions
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
    for (uint32_t i = 0; i < map->size; i++) {
        cellNode *curr = map->buckets[i];
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