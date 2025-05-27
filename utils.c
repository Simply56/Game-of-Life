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