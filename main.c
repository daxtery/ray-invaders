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

static bool accumulator_tick(Accumulator *accumulator, float dt)
{
    float add = dt * 1000.0;

    if (accumulator->ms_accumulated + add > accumulator->ms_to_trigger)
    {
        accumulator->ms_accumulated = 0;
        return true;
    }

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
};

static Enemies enemies = {0};

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

int main(void)
{
    InitWindow(800, 600, "Ray Invaders Game in Raylib");

    SetTargetFPS(60);

    srand(time(NULL));

    // 8*3
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
                        .ms_to_trigger = 1000,
                    },
            };
            nob_da_append(&enemies, enemy);
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

        Vector2 padding = {.x = padding_x, .y = padding_y};

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
            Vector2 enemy_size = {
                .x = scale,
                .y = scale,
            };

            nob_da_foreach(Enemy, enemy, &enemies)
            {
                Vector2 position = world_to_screen(enemy->position, scale, offset, padding);
                DrawRectangleV(position, enemy_size, DARKGRAY);
            }
        }

        Vector2 bullet_size = {
            .x = scale * 0.1,
            .y = scale * 0.1,
        };

        {
            nob_da_foreach(Bullet, bullet, &bullets)
            {
                Vector2 position = world_to_screen(bullet->position, scale, offset, padding);
                DrawRectangleV(position, bullet_size, RED);
            }
        }

        Vector2 player_size = {
            .x = scale,
            .y = scale,
        };

        {
            Vector2 position = world_to_screen(player.position, scale, offset, padding);
            DrawRectangleV(position, player_size, BLUE);
        }

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

        {
            for (int i = bullets.count - 1; i >= 0; --i)
            {
                Bullet *bullet = &bullets.items[i];
                if (bullet->position.y > GAME_ROWS)
                {
                    nob_da_remove_unordered(&bullets, i);
                }
            }
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
            if (accumulator_tick(&enemy->shooting, GetFrameTime()))
            {
                enemy_fire_bullet(enemy);
            }
        }

        {
            const Vector2 gravity = {
                .x = 0,
                .y = 1,
            };

            nob_da_foreach(Bullet, bullet, &bullets)
            {
                if (accumulator_tick(&bullet->timing, GetFrameTime()))
                {
                    bullet->position = Vector2Add(bullet->position, gravity);
                }
            }
        }

        {
            Rectangle player_box = {
                .height = 1,
                .width = 1,
                .x = player.position.x,
                .y = player.position.y,
            };

            nob_da_foreach(Bullet, bullet, &bullets)
            {
                Rectangle bullet_collision_box = {
                    .width = 0.1,
                    .height = 0.1,
                    .x = bullet->position.x,
                    .y = bullet->position.y,
                };

                if (CheckCollisionRecs(player_box, bullet_collision_box))
                {
                    printf("YOU ARE DEAD!\n");
                }
            }
        }

        nob_temp_reset();

        EndDrawing();
    }
}
