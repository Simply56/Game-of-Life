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
#define CELL_SIZE 4

/* 
 * due to the sequencia nature of adding buckets to the qeueue
 * highest possible number of bucket doesn't achieves the fastest result
 */
// #define MAP_SIZE UINT16_MAX // max value
#define MAP_SIZE 4096

#define DELAY 1 // mili

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
        fflush(stdout); // Ensure the output is flushed to the terminal
    }
}

// Function to initialize the grid with random cells
void initialize_map()
{
    map = innit(MAP_SIZE, GRID_W, GRID_H);
    new_map = innit(MAP_SIZE, 0, 0);
}

int_fast8_t might_live(int x, int y)
{
    int_fast8_t orange_count = 0;
    int_fast8_t blue_count = 0;

    for (int_fast8_t dy = -1; dy <= 1; dy++) {
        for (int_fast8_t dx = -1; dx <= 1; dx++) {
            if ((dy == -1 && y == 0) || (dx == -1 && x == 0)) { // avoid uint16_t underflow
                continue;
            }
            if (dx == 0 && dy == 0) {
                continue;
            }
            uint16_t ny = y + dy;
            uint16_t nx = x + dx;
            cell *neighbour = lifemap_get(map, nx, ny);
            if (neighbour != NULL) {
                orange_count += (neighbour->state == ORANGE);
                blue_count += (neighbour->state == BLUE);
            }
        }
    }
    if (orange_count + blue_count == 3) {
        // puts("came to life");
        return blue_count > orange_count ? BLUE : ORANGE;
    }
    // puts("stayed empty");
    return EMPTY;
}

int_fast8_t might_die(int x, int y)
{
    int_fast8_t live_neighbour_count = 0;

    for (int_fast8_t dy = -1; dy <= 1; dy++) {
        for (int_fast8_t dx = -1; dx <= 1; dx++) {
            if ((dy == -1 && y == 0) || (dx == -1 && x == 0)) { // avoid underflow
                continue;
            }
            if (dx == 0 && dy == 0) {
                continue;
            }

            int ny = y + dy;
            int nx = x + dx;
            cell *neighbour = lifemap_get(map, nx, ny);
            if (neighbour != NULL) {
                if (neighbour->state == BLUE || neighbour->state == ORANGE)
                    live_neighbour_count++;
            }
        }
    }
    if ((3 <= live_neighbour_count) && (live_neighbour_count <= 5)) {
        // TODO: INEFICIENT
        // puts("survived");
        cell *old_cell = lifemap_get(map, x, y);
        assert(old_cell != NULL);
        return old_cell->state;
    }
    // puts("died");
    return DEAD;
}

int_fast8_t cell_change(int x, int y)
{
    cell *tmp = lifemap_get(map, x, y);

    int_fast8_t cell = (tmp == NULL ? EMPTY : tmp->state);
    if (cell == BLUE || cell == ORANGE) {
        cell = might_die(x, y);
    } else if (cell >= DEAD) {
        if (cell == 6) {
            cell = EMPTY;
        } else {
            cell++;
        }
    } else if (cell == EMPTY) {
        cell = might_live(x, y);
    }
    return cell;
}

void update_bucket(void *bucket)
{
    if (bucket == NULL) {
        return;
    }

    cellNode *curr = bucket;
    while (curr) {
        for (int_fast8_t dy = -1; dy <= 1; dy++) {
            for (int_fast8_t dx = -1; dx <= 1; dx++) {
                //avoid underflow
                if ((dy == -1 && curr->c.y == 0) || (dx == -1 && curr->c.x == 0)) {
                    continue;
                }

                cell new_cell = { .x = curr->c.x + dx, .y = curr->c.y + dy, .state = cell_change(curr->c.x + dx, curr->c.y + dy) };
                assert(lifemap_set(new_map, new_cell));
            }
        }
        curr = curr->next;
    }
}

void purify_bucket(void *bucket)
{
    if (bucket == NULL) {
        return;
    }
    cellNode *curr = bucket;

    while (curr) {
        cellNode *tmp = curr->next;
        if (!lifemap_get(map, curr->c.x, curr->c.y)) {
            lifemap_set(new_map, (cell){ .x = curr->c.x, .y = curr->c.y, .state = EMPTY });
            curr = tmp;
        }
        curr = tmp;
    }
}

void update_grid()
{
    // lifemap_free(new_map);
    // new_map = innit(MAP_SIZE, 0, 0);

    // purify new_map
    pool->tasks->queued_count = 0;
    for (uint16_t i = 0; i < new_map->size; i++) {
        if (new_map->slots[i] == NULL) {
            continue;
        }
        enqueue(pool->tasks, (package){ .data_p = new_map->slots[i], .func = purify_bucket });
        pool->tasks->queued_count++;
    }
    while (pool->tasks->completed_count != pool->tasks->queued_count) {
        continue;
    }
    pool->tasks->completed_count = 0;
    pool->tasks->queued_count = 0;

    // generate new_map
    pool->tasks->queued_count = 0;
    for (uint16_t i = 0; i < map->size; i++) {
        if (map->slots[i] == NULL) {
            continue;
        }

        enqueue(pool->tasks, (package){ .data_p = map->slots[i], .func = update_bucket });
        pool->tasks->queued_count++;
    }

    while (pool->tasks->completed_count != pool->tasks->queued_count) {
        continue;
    }
    pool->tasks->completed_count = 0;
    pool->tasks->queued_count = 0;

    // for (uint16_t i = 0; i < MAP_SIZE; i++) {
    //     update_bucket(map->slots[i]);
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

    for (uint16_t i = 0; i < map->size; i++) {
        cellNode *curr = map->slots[i];
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

            cellNode *tmp = curr;
            curr = curr->next;
        }
    }

    glutSwapBuffers();
    print_fps(); // Call to calculate and print FPS
}

// Function to handle the timer callback for updates
void timer_callback(int value)
{
    update_grid();
    glutPostRedisplay();
    glutTimerFunc(DELAY, timer_callback, 0); // Schedule next update in DELAYms
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
    atexit(cleanup); // Register cleanup function
    double load_factor = ((double) GRID_H * (double) GRID_W) / (double) MAP_SIZE;
    printf("Load factor: %f\n", load_factor);
    initialize_map();

    pool = create_pool(8);
    if (!pool) {
        return EXIT_FAILURE;
    }
    // for (size_t i = 0; i < 3; i++) {
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
    glutTimerFunc(DELAY, timer_callback, 0);

    glutMainLoop();

    return EXIT_SUCCESS;
}
