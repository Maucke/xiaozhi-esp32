#pragma once

#define GRID_WIDTH 10
#define GRID_HEIGHT 9
#define BALL_COUNT 18

typedef struct {
    float x, y;
    float vx, vy;
} Ball;

void physics_init();
void physics_update(float ax, float ay, float dt);
void physics_render_to_grid(char grid[GRID_HEIGHT][GRID_WIDTH]);