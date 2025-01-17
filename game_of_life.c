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

#define GRID_W 200 // Grid width
#define GRID_H 200 // Grid height
#define CELL_SIZE 2

// #define MAP_SIZE 131072
// #define MAP_SIZE 65536
// #define MAP_SIZE 32768
#define MAP_SIZE 16384
// #define MAP_SIZE 8192
// #define MAP_SIZE 4096
// #define MAP_SIZE 2048

#define DELAY 0 // mili
// MAP_SIZE MUST BE DIVISIBLE BY BUCKETS_PER_THREAD
#define BUCKETS_PER_THREAD 32
#define THREADS 10

lifeHashMap *map;
lifeHashMap *new_map;

thread_pool *pool;

// Function to calculate and print FPS, overwriting the last line
void print_fps()
{
    static int frame_count = 0;
    static double last_time = 0;
    static double fps = 0.0;

    // Get the current time in seconds
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    double now = current_time.tv_sec + (current_time.tv_nsec / 1e9);

    // Increment frame count
    frame_count++;

    // Calculate FPS every second
    if (now - last_time >= 1.0) {
        fps = frame_count / (now - last_time); // Frames per second
        frame_count = 0;                       // Reset frame count
        last_time = now;                       // Reset time

        // Print FPS and overwrite the same line
        printf("FPS: %.2f\n", fps);
    }
}

// Function to initialize the grid with random cells
void initialize_map()
{
    map = innit(MAP_SIZE, GRID_W, GRID_H);
    new_map = innit(MAP_SIZE, 0, 0);
}

void count_neighbors(int x, int y, uint8_t *blue_count, uint8_t *orange_count)
{
    for (int8_t dy = -1; dy <= 1; dy++) {
        for (int8_t dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) {
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

    // keeps the cells that can be reused
    while (curr) {
        cellNode *tmp = curr->next;
        if (!lifemap_get(map, curr->c.x, curr->c.y)) {
            lifemap_set(new_map, (cell){ .x = curr->c.x, .y = curr->c.y, .state = EMPTY });
            curr = tmp;
        }
        curr = tmp;
    }

    // removes the entire bucket
    // while (curr) {
    //     cellNode *tmp = curr;
    //     curr = curr->next;
    //     free(tmp);
    // }
    // *b = NULL;
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
        enqueue(pool->tasks, (package){ .data_p = &(life_map->buckets[i]), .func = f });
        pool->tasks->queued_count++;
    }

    while (pool->tasks->completed_count != pool->tasks->queued_count) {
        sched_yield();
        continue;
    }
    pool->tasks->completed_count = 0;
    pool->tasks->queued_count = 0;
}

void update_grid()
{
    print_fps();
    // return;

    // purify new_map
    for_buckets(new_map, purify_buckets);
    // lifemap_free(new_map);
    // new_map = innit(MAP_SIZE, 0, 0);

    // compute new_map
    for_buckets(map, update_buckets);

    // single threaded version for comparison
    // for (uint16_t i = 0; i < MAP_SIZE; i += BUCKETS_PER_THREAD) {
    //     update_buckets(&(map->buckets[i]));
    // }

    // swap
    void *tmp = map;
    map = new_map;
    new_map = tmp;
}

// Function to draw the grid using OpenGL
void draw_grid()
{
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(1, 1, 1, 1);

    for (uint32_t i = 0; i < map->size; i++) {
        cellNode *curr = map->buckets[i];
        int slot_len = 0;
        while (curr) {
            slot_len++;
            assert(curr->c.state != EMPTY);
            // print_cell(curr->c);
            if (curr->c.state == ORANGE) {
                glColor3f(1, 0.6f, 0);
            } else if (curr->c.state == BLUE) {
                glColor3f(0, 0.6f, 1);
            } else {
                switch (curr->c.state) {
                case 3:
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
                    // printf("slot len: %d\n", slot_len);
                    // print_cell(curr->c);

                    break;
                }
            }
            if (curr->c.x > GRID_W || curr->c.y > GRID_H) {
                // don't draw off-screen cells
                // printf("%d, %d: skipping off-screen cell\n", curr->c.x, curr->c.y);
                curr = curr->next;
                continue;
            }

            int x1 = curr->c.x * CELL_SIZE;
            int y1 = curr->c.y * CELL_SIZE;
            int x2 = x1 + CELL_SIZE;
            int y2 = y1 + CELL_SIZE;

            glBegin(GL_QUADS);
            glVertex2i(x1, y1);
            glVertex2i(x2, y1);
            glVertex2i(x2, y2);
            glVertex2i(x1, y2);
            glEnd();

            curr = curr->next;
        }
    }

    glutSwapBuffers();
}

// Function to handle the timer callback for updates
void timer_callback(int _)
{
    update_grid();
    glutPostRedisplay();
    glutTimerFunc(DELAY, timer_callback, _); // Schedule next update in DELAYms
}

void idle_callback()
{
    update_grid();
    glutPostRedisplay();
}

void cleanup()
{
    lifemap_free(map);
    lifemap_free(new_map);
    destroy_pool(pool);
    printf("Cleaned up resources.\n");
}
// Main function
int main(int argc, char **argv)
{
    const GLubyte *renderer = glGetString(GL_RENDERER);                       // Get renderer string
    const GLubyte *vendor = glGetString(GL_VENDOR);                           // Get vendor name
    const GLubyte *version = glGetString(GL_VERSION);                         // Get OpenGL version
    const GLubyte *shadingVersion = glGetString(GL_SHADING_LANGUAGE_VERSION); // Get GLSL version (if supported)

    // Print GPU information
    printf("Renderer: %s\n", renderer);
    printf("Vendor: %s\n", vendor);
    printf("OpenGL Version: %s\n", version);
    printf("GLSL Version: %s\n", shadingVersion);

    assert(MAP_SIZE % BUCKETS_PER_THREAD == 0);
    atexit(cleanup); // Register cleanup function
    double load_factor = ((double) GRID_H * (double) GRID_W) / (double) MAP_SIZE;
    printf("Load factor: %f\n", load_factor);
    initialize_map();

    pool = create_pool(THREADS);
    if (!pool) {
        return EXIT_FAILURE;
    }

    // debugging without window
    // while (1) {
    //     update_grid();
    // }

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
