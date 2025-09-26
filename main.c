#define NOB_IMPLEMENTATION
#include "nob.h"
#include "raylib.h"
#include "raymath.h"

#define min(a, b) (a) < (b) ? (a) : (b)

typedef struct
{
    uint16_t ms_to_trigger;
    uint16_t ms_accumulated;
} Accumulator;

#define RESTART true
#define KEEP false

static bool accumulator_tick(Accumulator *accumulator, float dt, bool restart)
{
    if (accumulator->ms_accumulated > accumulator->ms_to_trigger)
    {
        if (restart)
        {
            accumulator->ms_accumulated = 0;
        }
        return true;
    }

    float add = dt * 1000.0;
    accumulator->ms_accumulated += add;
    return false;
}

typedef struct
{
    Vector2 position;
    Accumulator shooting;
} Enemy;

typedef struct
{
    Enemy *items;
    size_t count;
    size_t capacity;
} Enemies;

typedef struct
{
    Vector2 position;
    Accumulator timing;
} Bullet;

typedef struct
{
    Bullet *items;
    size_t count;
    size_t capacity;
} Bullets;

typedef struct
{
    Vector2 position;
    bool destroyed;
} Destroyable;

typedef struct
{
    Destroyable *items;
    size_t count;
    size_t capacity;
} Destroyables;

typedef struct
{
    Vector2 position;
    Accumulator shooting;
    Bullet bullet;
} Player;

#define ENEMY_ROWS 3
#define COLUMNS 8

#define EMPTY_ROWS 4
#define GAME_ROWS (ENEMY_ROWS + EMPTY_ROWS + 1)

static Bullets bullets = {0};

void enemy_fire_bullet(const Enemy *enemy)
{
    Bullet bullet = {
        .position = {.x = enemy->position.x, .y = enemy->position.y},
        .timing =
            {
                .ms_accumulated = 0,
                .ms_to_trigger = 200,
            },
    };

    // make a pool
    nob_da_append(&bullets, bullet);
}

static Player player = {
    .position =
        {
            .x = COLUMNS / 2,
            .y = GAME_ROWS - 1,
        },
    .shooting =
        {
            .ms_accumulated = 0,
            .ms_to_trigger = 200,
        },
    .bullet = {{0}},
};

static Enemies enemies = {0};

static Destroyables destroyables = {0};

static const Vector2 BULLET_SIZE = {
    .x = .1,
    .y = .1,
};

static const Vector2 DESTROYABLE_SIZE = {
    .x = .1,
    .y = .1,
};

static const Vector2 PLAYER_SIZE = {
    .x = 1,
    .y = 1,
};

static const Vector2 ENEMY_SIZE = {
    .x = 1,
    .y = 1,
};

Vector2 world_to_screen(const Vector2 world_coordinates, float scale, const Vector2 offset, const Vector2 padding)
{
    Vector2 position = Vector2Scale(world_coordinates, scale);
    position = Vector2Add(position, offset);
    Vector2 scaled_padding = {
        .x = padding.x * world_coordinates.x,
        .y = padding.y * world_coordinates.y,
    };
    position = Vector2Add(position, scaled_padding);
    return position;
}

static void move_player_bullet(void)
{
    Bullet *bullet = &player.bullet;

    if (bullet->position.y <= 0)
    {
        bullet->position = Vector2Zero();
        return;
    }

    Rectangle bullet_collision_box = {
        .width = BULLET_SIZE.x,
        .height = BULLET_SIZE.y,
        .x = bullet->position.x,
        .y = bullet->position.y,
    };

    nob_da_foreach(Enemy, enemy, &enemies)
    {
        Rectangle enemy_collision_box = {
            .width = DESTROYABLE_SIZE.x,
            .height = DESTROYABLE_SIZE.y,
            .x = enemy->position.x,
            .y = enemy->position.y,
        };

        if (CheckCollisionRecs(enemy_collision_box, bullet_collision_box))
        {
            printf("Enemy @ (%.0f, %.0f) would be destroyed!\n", enemy->position.x, enemy->position.y);
            // TODO: destroy enemy
            bullet->position = Vector2Zero();
            return;
        }
    }

    nob_da_foreach(Destroyable, destroyable, &destroyables)
    {
        if (destroyable->destroyed)
        {
            continue;
        }

        Rectangle destroyable_collision_box = {
            .width = DESTROYABLE_SIZE.x,
            .height = DESTROYABLE_SIZE.y,
            .x = destroyable->position.x,
            .y = destroyable->position.y,
        };

        if (CheckCollisionRecs(destroyable_collision_box, bullet_collision_box))
        {
            destroyable->destroyed = true;
            bullet->position = Vector2Zero();
            return;
        }
    }
}

int main(void)
{
    InitWindow(800, 600, "Ray Invaders Game in Raylib");

    SetTargetFPS(60);

    srand(time(NULL));

    for (size_t i = 0; i < COLUMNS; ++i)
    {
        for (size_t j = 0; j < ENEMY_ROWS; ++j)
        {
            Enemy enemy = {
                .position = {.x = i, .y = j},
                .shooting =
                    {
                        .ms_accumulated = 0,
                        // .ms_to_trigger = GetRandomValue(5000, 30000),
                        .ms_to_trigger = 10500,
                    },
            };
            nob_da_append(&enemies, enemy);
        }
    }

    for (size_t i = 0; i < 12; ++i)
    {
        int y = ENEMY_ROWS + GetRandomValue(1, EMPTY_ROWS - 1);
        int x = GetRandomValue(0, COLUMNS);

        int x_pieces = GetRandomValue(0, 10);
        int y_pieces = GetRandomValue(0, 10);

        for (int offset_x = -(x_pieces / 2); offset_x <= (x_pieces / 2); ++offset_x)
        {
            for (int offset_y = -(y_pieces / 2); offset_y <= (y_pieces / 2); ++offset_y)
            {
                nob_da_append(&destroyables, ((Destroyable){
                                                 .destroyed = false,
                                                 .position =
                                                     {
                                                         .x = x + offset_x * 0.1,
                                                         .y = y + offset_y * 0.1,
                                                     },
                                             }));
            }
        }
    }

    bool reverse = false;

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        float width = GetScreenWidth();
        float offset_width = width * 0.1;
        float width_for_game = width - offset_width;
        float padding_x = width_for_game * 0.05;
        float size_x = (width_for_game - (padding_x * COLUMNS)) / (float)(COLUMNS + 1);

        float height = GetScreenHeight();
        float offset_height = height * 0.1f;
        float height_for_game = height - offset_height;
        float padding_y = height_for_game * 0.05;
        float size_y = (height_for_game - (padding_y * GAME_ROWS)) / (float)GAME_ROWS;

        float scale = min(size_y, size_x);

        Vector2 offset = {
            .x = (width - (size_x * (COLUMNS)) - (padding_x * COLUMNS)),
            .y = offset_height,
        };

        Vector2 padding = {
            .x = padding_x,
            .y = padding_y,
        };

        // DEBUG
        {
            for (size_t i = 0; i < 12; ++i)
            {
                for (size_t j = 0; j < 12; ++j)
                {
                    Vector2 xy = {.x = i, .y = j};
                    Vector2 position = world_to_screen(xy, scale, offset, padding);
                    const char *text = nob_temp_sprintf("%lu:%lu(%d:%d)", i, j, (int)position.x, (int)position.y);
                    const size_t font_size = 10;
                    Vector2 text_size = MeasureTextEx(GetFontDefault(), text, font_size, 0);
                    DrawText(text,
                             position.x - text_size.x / 2, //
                             position.y - text_size.y / 2, //
                             font_size,                    //
                             BLACK);
                }
            }
        }

        {
            nob_da_foreach(Enemy, enemy, &enemies)
            {
                Vector2 position = world_to_screen(enemy->position, scale, offset, padding);
                Vector2 size = Vector2Scale(ENEMY_SIZE, scale);
                DrawRectangleV(position, size, DARKGRAY);
            }
        }

        {
            nob_da_foreach(Bullet, bullet, &bullets)
            {
                Vector2 position = world_to_screen(bullet->position, scale, offset, padding);
                Vector2 size = Vector2Scale(BULLET_SIZE, scale);
                DrawRectangleV(position, size, RED);
            }
        }

        {
            nob_da_foreach(Destroyable, destroyable, &destroyables)
            {
                if (destroyable->destroyed)
                {
                    continue;
                }

                Vector2 position = world_to_screen(destroyable->position, scale, offset, padding);
                Vector2 size = Vector2Scale(DESTROYABLE_SIZE, scale);
                DrawRectangleV(position, size, GREEN);
            }
        }

        {
            Vector2 position = world_to_screen(player.position, scale, offset, padding);
            Vector2 size = Vector2Scale(PLAYER_SIZE, scale);
            DrawRectangleV(position, size, BLUE);
        }

#define BULLET_IN_TRAVEL(bullet) (bullet.position.x != 0 && bullet.position.y != 0)

        if (BULLET_IN_TRAVEL(player.bullet))
        {
            Vector2 position = world_to_screen(player.bullet.position, scale, offset, padding);
            Vector2 size = Vector2Scale(BULLET_SIZE, scale);
            DrawRectangleV(position, size, BLUE);
        }

        EndDrawing();

        Vector2 next_direction = {0};

        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
        {
            next_direction.x += GetFrameTime();
        }
        else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))
        {
            next_direction.x -= GetFrameTime();
        }
        else if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))
        {
            next_direction.y -= GetFrameTime();
        }
        else if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))
        {
            next_direction.y += GetFrameTime();
        }

        player.position = Vector2Add(next_direction, player.position);

        if (IsKeyDown(KEY_SPACE) && accumulator_tick(&player.shooting, GetFrameTime(), KEEP) &&
            !BULLET_IN_TRAVEL(player.bullet))
        {
            player.shooting.ms_accumulated = 0;
            player.bullet.position = (Vector2){
                .x = player.position.x,
                .y = player.position.y,
            };
            player.bullet.timing = (Accumulator){
                .ms_accumulated = 0,
                .ms_to_trigger = 200,
            };
        }

        nob_da_foreach(Enemy, enemy, &enemies)
        {
            Vector2 new_position = (Vector2){
                .x = enemy->position.x + (reverse ? 0.01f : -0.01f),
                .y = enemy->position.y,
            };

            if (new_position.x < 0 || new_position.x > COLUMNS)
            {
                reverse = !reverse;
            }
        }

        nob_da_foreach(Enemy, enemy, &enemies)
        {
            enemy->position = (Vector2){
                .x = enemy->position.x + (reverse ? 0.01f : -0.01f),
                .y = enemy->position.y,
            };
        }

        nob_da_foreach(Enemy, enemy, &enemies)
        {
            if (accumulator_tick(&enemy->shooting, GetFrameTime(), RESTART))
            {
                enemy_fire_bullet(enemy);
            }
        }

        {
            const Vector2 gravity = {
                .x = 0,
                .y = 10 * GetFrameTime(),
            };

            nob_da_foreach(Bullet, bullet, &bullets)
            {
                if (accumulator_tick(&bullet->timing, GetFrameTime(), RESTART))
                {
                    bullet->position = Vector2Add(bullet->position, gravity);
                }
            }

            if (BULLET_IN_TRAVEL(player.bullet))
            {
                player.bullet.position = Vector2Add(player.bullet.position, Vector2Scale(gravity, -1.0f));
            }
        }

        {
            Rectangle player_box = {
                .height = PLAYER_SIZE.x,
                .width = PLAYER_SIZE.y,
                .x = player.position.x,
                .y = player.position.y,
            };

            for (int i = bullets.count - 1; i >= 0; --i)
            {
                Bullet *bullet = &bullets.items[i];

                if (bullet->position.y > GAME_ROWS)
                {
                    nob_da_remove_unordered(&bullets, i);
                }

                Rectangle bullet_collision_box = {
                    .width = BULLET_SIZE.x,
                    .height = BULLET_SIZE.y,
                    .x = bullet->position.x,
                    .y = bullet->position.y,
                };

                if (CheckCollisionRecs(player_box, bullet_collision_box))
                {
                    // TODO: player death
                    nob_da_remove_unordered(&bullets, i);
                    continue;
                }

                nob_da_foreach(Destroyable, destroyable, &destroyables)
                {
                    if (destroyable->destroyed)
                    {
                        continue;
                    }

                    Rectangle destroyable_collision_box = {
                        .width = DESTROYABLE_SIZE.x,
                        .height = DESTROYABLE_SIZE.y,
                        .x = destroyable->position.x,
                        .y = destroyable->position.y,
                    };

                    if (CheckCollisionRecs(destroyable_collision_box, bullet_collision_box))
                    {
                        destroyable->destroyed = true;
                        nob_da_remove_unordered(&bullets, i);
                        break;
                    }
                }
            }
        }

        move_player_bullet();

        nob_temp_reset();
    }
}
