#define _POSIX_C_SOURCE 200809L

#include "lifehashmap.h"
#include "threadpool.h"

#include <GL/glut.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define GRID_W 800 // Grid width
#define GRID_H 800 // Grid height
#define CELL_SIZE 1

/* MAP_SIZE MUST BE DIVISIBLE BY BUCKETS_PER_THREAD */
#define MAP_SIZE 262144
// #define MAP_SIZE 131072
// #define MAP_SIZE 65536
// #define MAP_SIZE 32768
// #define MAP_SIZE 16384
// #define MAP_SIZE 8192
// #define MAP_SIZE 4096
// #define MAP_SIZE 2048
// #define MAP_SIZE 4

#define BENCHMARK (true)
#define VISUAL (false)
#define BINARY_OUT (true)

#define DELAY 0 // mili
#define BUCKETS_PER_THREAD 4096
#define THREAD_COUNT 14

lifeHashMap *map;
lifeHashMap *new_map;

thread_pool *pool;

// R G B A — this ends up in memory as: [B][G][R][A]
uint32_t rgba(uint8_t r, uint8_t g, uint8_t b)
{
    return (b) | (g << 8) | (r << 16) | (0 << 24); // alpha = 0
}

void binary_to_stdout()
{
    for (int32_t y = GRID_H - 1; y >= 0; y--) {
        uint32_t buffer[GRID_H];
        memset(buffer, 0xff, sizeof buffer);
        for (uint32_t x = 0; x < GRID_W; x++) {
            cell *cell = lifemap_get(map, x, y);
            if (cell == NULL) {
                continue;
            }
            switch (cell->state) {
            case ORANGE:
                buffer[x] = rgba(240, 128, 0);
                break;
            case BLUE:
                buffer[x] = rgba(0, 128, 240);
                break;
            case DEAD:
                buffer[x] = rgba(75, 75, 75);
                break;
            case 4:
                buffer[x] = rgba(136, 136, 136);
                break;
            case 5:
                buffer[x] = rgba(160, 160, 160);
                break;
            default:
                buffer[x] = rgba(238, 238, 238);
                break;
            }
        }
        write(STDOUT_FILENO, buffer, sizeof buffer);
    }
}

void print_load_factor()
{
    if (!BENCHMARK) {
        return;
    }

    double load_factor = (double) count_items(map) / (double) MAP_SIZE;

    fprintf(stderr, "Load factor: %f\n", load_factor);
}

void print_fps()
{
    if (!BENCHMARK) {
        return;
    }

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
        printf("FPS: %.2f\n", fps);
        print_load_factor();
    }
}

// To initialize the grid with random cells
void initialize_map()
{
    map = innit(MAP_SIZE, GRID_W, GRID_H, BENCHMARK);
    new_map = innit(MAP_SIZE, 0, 0, BENCHMARK);
}

void count_neighbors(int x, int y, uint8_t *blue_count, uint8_t *orange_count)
{
    for (int8_t dy = -1; dy <= 1; dy++) {
        for (int8_t dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) { // skip self
                continue;
            }
            int ny = y + dy;
            int nx = x + dx;
            cell *neighbour = lifemap_get(map, nx, ny);
            if (neighbour != NULL) {
                if (neighbour->state == BLUE) {
                    (*blue_count)++;
                } else if (neighbour->state == ORANGE) {
                    (*orange_count)++;
                }
            }
        }
    }
}

uint8_t cell_change(int x, int y)
{
    cell *tmp = lifemap_get(map, x, y);
    uint8_t cell = (tmp == NULL ? EMPTY : tmp->state);

    if (cell >= DEAD) {
        if (cell == 6) {
            return EMPTY;
        }
        return cell + 1;
    }

    uint8_t blue_count, orange_count;
    blue_count = orange_count = 0;
    count_neighbors(x, y, &blue_count, &orange_count);
    uint8_t count = blue_count + orange_count;

    if ((cell == BLUE) || (cell == ORANGE)) {
        if ((3 <= count) && (count <= 5)) {
            return cell;
        }
        return DEAD;
    } else if ((cell == EMPTY) && (count == 3)) {
        if (blue_count > orange_count) {
            return BLUE;
        }
        return ORANGE;
    }
    return cell;
}

void update_bucket(void *bucket_p)
{
    if (bucket_p == NULL) {
        return;
    }
    cellNode **b = bucket_p;

    cellNode *curr = *b;
    while (curr) {
        for (int8_t dy = -1; dy <= 1; dy++) {
            for (int8_t dx = -1; dx <= 1; dx++) {
                cell new_cell = { .x = curr->c.x + dx, .y = curr->c.y + dy, .state = cell_change(curr->c.x + dx, curr->c.y + dy) };
                lifemap_set(new_map, new_cell);
            }
        }
        curr = curr->next;
    }
}

void update_buckets(void *start_bucket)
{
    assert(start_bucket != NULL);
    for (int i = 0; i < BUCKETS_PER_THREAD; i++) {
        update_bucket(start_bucket);
        start_bucket += sizeof(cellNode *);
    }
}

void purify_bucket(void *bucket_p)
{
    if (bucket_p == NULL) {
        return;
    }
    cellNode **b = bucket_p;
    cellNode *curr = *b;

    //  keeps the cells that can be reused
    // while (curr) {
    //     cellNode *tmp = curr->next;
    //     if (!lifemap_get(map, curr->c.x, curr->c.y)) {
    //         lifemap_set(new_map, (cell){ .x = curr->c.x, .y = curr->c.y, .state = EMPTY });
    //         curr = tmp;
    //     }
    //     curr = tmp;
    // }

    // removes the entire bucket
    while (curr) {
        cellNode *tmp = curr;
        curr = curr->next;
        free(tmp);
    }
    *b = NULL;
}

void purify_buckets(void *start_bucket)
{
    assert(start_bucket != NULL);
    for (int i = 0; i < BUCKETS_PER_THREAD; i++) {
        purify_bucket(start_bucket);
        start_bucket += sizeof(cellNode *);
    }
}

void for_buckets(lifeHashMap *life_map, void (*f)(void *))
{
    pool->tasks->queued_count = 0;
    for (uint32_t i = 0; i < life_map->size; i += BUCKETS_PER_THREAD) {
        // if (life_map->buckets[i] == NULL) {
        //     continue;
        // }
        enqueue(pool->tasks, (package) { .data_p = &(life_map->buckets[i]), .func = f });
        pool->tasks->queued_count++;
    }

    while (pool->tasks->completed_count != pool->tasks->queued_count) {
        // sched_yield();
        continue;
    }
    pool->tasks->completed_count = 0;
    pool->tasks->queued_count = 0;
}

void update_grid()
{
    print_load_factor();
    for_buckets(new_map, purify_buckets);
    // lifemap_free(new_map);
    // new_map = innit(MAP_SIZE, 0, 0);

    // compute new_map
    for_buckets(map, update_buckets);

    // Single threaded version for comparison
    // for (uint16_t i = 0; i < MAP_SIZE; i += BUCKETS_PER_THREAD) {
    //     update_buckets(&(map->buckets[i]));
    // }

    // swap pointers
    void *tmp = map;
    map = new_map;
    new_map = tmp;

    if (BINARY_OUT) {
        binary_to_stdout();
    }
}

// Function to draw the grid using OpenGL
void draw_grid()
{
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(1, 1, 1, 1);

    // Group cells by state
    // Goes over the whole map 6 times reducing glcolor3f calls

    glBegin(GL_QUADS);
    for (uint32_t i = 0; i < map->size; i++) {
        cellNode *curr = map->buckets[i];
        while (curr) {
            if (curr->c.x > GRID_W || curr->c.y > GRID_H) {
                curr = curr->next;
                continue; // Skip off-screen cells
            }
            switch (curr->c.state) {
            case ORANGE:
                glColor3f(1, 0.6f, 0);
                break;
            case BLUE:
                glColor3f(0, 0.6f, 1);
                break;
            case DEAD:
                glColor3f(0.4f, 0.4f, 0.4f);
                break;
            case 4:
                glColor3f(0.5f, 0.5f, 0.5f);
                break;
            case 5:
                glColor3f(0.58f, 0.58f, 0.58f);
                break;
            default:
                glColor3f(0.7f, 0.7f, 0.7f);
                break;
            }
            int x1 = curr->c.x * CELL_SIZE;
            int y1 = curr->c.y * CELL_SIZE;
            int x2 = x1 + CELL_SIZE;
            int y2 = y1 + CELL_SIZE;

            // Draw the cell as a quad
            glVertex2i(x1, y1);
            glVertex2i(x2, y1);
            glVertex2i(x2, y2);
            glVertex2i(x1, y2);
            // glVertex2i(curr->c.x, curr->c.y);
            curr = curr->next;
        }
    }
    glEnd();
    glutSwapBuffers();
}

// Function to handle the timer callback for updates
void timer_callback(int _)
{
    update_grid();
    glutPostRedisplay();
    binary_to_stdout();
    glutTimerFunc(DELAY, timer_callback, _); // Schedule next update in DELAYms
    // print_fps();
}
// Faster when delay is 0
void idle_callback()
{
    update_grid();
    glutPostRedisplay();
    // print_fps();
}

void cleanup()
{
    lifemap_free(map);
    lifemap_free(new_map);
    destroy_pool(pool);
    printf("Cleaned up resources.\n");
}

int main(int argc, char **argv)
{
    printf("Renderer: %s\n", glGetString(GL_RENDERER));                     // Get renderer string
    printf("Vendor: %s\n", glGetString(GL_VENDOR));                         // Get vendor name
    printf("OpenGL Version: %s\n", glGetString(GL_VERSION));                // Get OpenGL version
    printf("GLSL Version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION)); // Get GLSL version (if supported)

    assert(MAP_SIZE % BUCKETS_PER_THREAD == 0);
    atexit(cleanup); // Register cleanup function

    initialize_map();

    pool = create_pool(THREAD_COUNT);
    if (!pool) {
        return EXIT_FAILURE;
    }

    printf("Enqueues per update: %d\n", MAP_SIZE / BUCKETS_PER_THREAD);
    while (!VISUAL) {
        update_grid();
    }

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(GRID_W * CELL_SIZE, GRID_H * CELL_SIZE);
    glutCreateWindow("Game of Life");

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, GRID_W * CELL_SIZE, 0, GRID_H * CELL_SIZE);
    glutDisplayFunc(draw_grid);

    if (DELAY) {
        glutTimerFunc(DELAY, timer_callback, 0);
        puts("using DELAY");
    } else {
        glutIdleFunc(idle_callback); // Continuously update and render
    }

    glutMainLoop(); // Start the GLUT event loop

    return EXIT_SUCCESS;
}
