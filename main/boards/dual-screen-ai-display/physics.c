#include "physics.h"
#include <math.h>
#include <string.h>

static Ball balls[BALL_COUNT];

void physics_init() {
    for (int i = 0; i < BALL_COUNT; i++) {
        balls[i].x = rand() % GRID_WIDTH;
        balls[i].y = rand() % GRID_HEIGHT;
        balls[i].vx = 0;
        balls[i].vy = 0;
    }
}

void physics_update(float ax, float ay, float dt) {
    const float friction = 0.98f;    // 摩擦力（每帧速度保留98%）
    const float bounce_loss = 0.8f;  // 反弹能量损耗（80%保留）

    for (int i = 0; i < BALL_COUNT; i++) {
        Ball* b = &balls[i];
        b->vx += ax * dt;
        b->vy += ay * dt;

        // 摩擦力：模拟空气阻力
        b->vx *= friction;
        b->vy *= friction;

        b->x += b->vx * dt;
        b->y += b->vy * dt;

        // 边界反弹
        if (b->x < 0) {
            b->x = 0;
            b->vx *= -bounce_loss;
        } else if (b->x >= GRID_WIDTH) {
            b->x = GRID_WIDTH - 0.01;
            b->vx *= -bounce_loss;
        }

        if (b->y < 0) {
            b->y = 0;
            b->vy *= -bounce_loss;
        } else if (b->y >= GRID_HEIGHT) {
            b->y = GRID_HEIGHT - 0.01;
            b->vy *= -bounce_loss;
        }
    }

    // 小球间碰撞检测与响应（精确防止重叠）
    for (int i = 0; i < BALL_COUNT; i++) {
        for (int j = i + 1; j < BALL_COUNT; j++) {
            Ball* a = &balls[i];
            Ball* b = &balls[j];

            float dx = b->x - a->x;
            float dy = b->y - a->y;
            float dist2 = dx*dx + dy*dy;
            float min_dist = 0.6f; // 代表小球半径的最小距离

            if (dist2 < min_dist * min_dist) {
                // 计算碰撞法线方向
                float dist = sqrt(dist2);
                float overlap = min_dist - dist;

                // 归一化碰撞方向向量
                float nx = dx / dist;
                float ny = dy / dist;

                // 位置修正，防止小球重叠
                a->x -= nx * overlap * 0.5;
                a->y -= ny * overlap * 0.5;
                b->x += nx * overlap * 0.5;
                b->y += ny * overlap * 0.5;

                // 交换速度，模拟弹性碰撞
                float vx1 = a->vx;
                float vy1 = a->vy;
                a->vx = b->vx * bounce_loss;
                a->vy = b->vy * bounce_loss;
                b->vx = vx1 * bounce_loss;
                b->vy = vy1 * bounce_loss;
            }
        }
    }
}

void physics_render_to_grid(char grid[GRID_HEIGHT][GRID_WIDTH]) {
    memset(grid, '.', GRID_WIDTH * GRID_HEIGHT);
    for (int i = 0; i < BALL_COUNT; i++) {
        int x = (int)(balls[i].x);
        int y = (int)(balls[i].y);
        if (x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT) {
            grid[y][x] = 'o';
        }
    }
}