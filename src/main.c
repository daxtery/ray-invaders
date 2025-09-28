#define NOB_IMPLEMENTATION
#include "accumulator.h"
#include "nob.h"
#include "raylib.h"
#include "raymath.h"

#define min(a, b) (a) < (b) ? (a) : (b)

typedef struct
{
    Vector2 position;
    Accumulator shooting;
    bool destroyed;
} Enemy;

typedef struct
{
    Enemy *items;
    size_t count;
    size_t capacity;
} Enemies;

typedef struct
{
    bool destroyed;
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
    bool destroyed;
} Player;

typedef struct
{
    Bullets enemy_bullets;
    Enemies enemies;
    bool enemies_going_right;
    //
    Destroyables destroyables;
    //
    Player player;
    //
    uint16_t score;
} State;

#define ENEMY_ROWS 3
#define COLUMNS 8

#define EMPTY_ROWS 4
#define GAME_ROWS (ENEMY_ROWS + EMPTY_ROWS + 1)

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

static Vector2 world_to_screen(const Vector2 world_coordinates, float scale, const Vector2 offset,
                               const Vector2 padding)
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

typedef enum
{
    NONE,
    BULLET,
    DESTROYABLE,
    PLAYER = 3,
    ENEMY = 3,
} HitResult;

#define check_collisions(Type, it, da, Size, Checking, label)                                                          \
    nob_da_foreach(Type, it, da)                                                                                       \
    {                                                                                                                  \
        Rectangle __bullet_collision_box = {                                                                           \
            .width = BULLET_SIZE.x,                                                                                    \
            .height = BULLET_SIZE.y,                                                                                   \
            .x = bullet->position.x,                                                                                   \
            .y = bullet->position.y,                                                                                   \
        };                                                                                                             \
                                                                                                                       \
        if (it->destroyed)                                                                                             \
        {                                                                                                              \
            continue;                                                                                                  \
        }                                                                                                              \
                                                                                                                       \
        Rectangle __collision_box = {                                                                                  \
            .width = Size.x,                                                                                           \
            .height = Size.y,                                                                                          \
            .x = it->position.x,                                                                                       \
            .y = it->position.y,                                                                                       \
        };                                                                                                             \
                                                                                                                       \
        if (CheckCollisionRecs(__collision_box, __bullet_collision_box))                                               \
        {                                                                                                              \
            it->destroyed = true;                                                                                      \
            bullet->destroyed = true;                                                                                  \
            result = Checking;                                                                                         \
            goto label;                                                                                                \
        }                                                                                                              \
    }

static void move_player_bullet(State *state)
{
    Bullet *bullet = &state->player.bullet;

    if (bullet->position.y <= 0)
    {
        bullet->destroyed = true;
        return;
    }

    HitResult result = NONE;

    check_collisions(Enemy, enemy, &state->enemies, ENEMY_SIZE, ENEMY, move_player_bullet_after_collision);
    check_collisions(Destroyable, destroyable, &state->destroyables, DESTROYABLE_SIZE, DESTROYABLE,
                     move_player_bullet_after_collision);

move_player_bullet_after_collision:
    if (result == ENEMY)
    {
        state->score += 10;
    }
}

void setup(State *state)
{
    state->enemy_bullets.count = 0;
    state->enemies.count = 0;
    state->enemies_going_right = true;
    state->destroyables.count = 0;

    state->player = (Player){
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
        .bullet =
            {
                .position = {0},
                .timing = {0},
                .destroyed = true,
            },
        .destroyed = false,
    };

    state->score = 0;

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
                .destroyed = false,
            };
            nob_da_append(&state->enemies, enemy);
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
                nob_da_append(&state->destroyables, ((Destroyable){
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
}

int main(void)
{
    InitWindow(800, 600, "Ray Invaders Game in Raylib");

    SetTargetFPS(60);

    srand(time(NULL));

    State state = {0};
    setup(&state);

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
            const char *text = nob_temp_sprintf("Score: %d", state.score);
            const size_t font_size = 10;
            Vector2 text_size = MeasureTextEx(GetFontDefault(), text, font_size, 0);
            Vector2 position = {
                .x = GetScreenWidth() / 2,
                .y = 10,
            };
            DrawText(text,
                     position.x - text_size.x / 2, //
                     position.y - text_size.y / 2, //
                     font_size,                    //
                     RED);
        }

        {
            nob_da_foreach(Enemy, enemy, &state.enemies)
            {
                if (enemy->destroyed)
                {
                    continue;
                }

                Vector2 position = world_to_screen(enemy->position, scale, offset, padding);
                Vector2 size = Vector2Scale(ENEMY_SIZE, scale);
                DrawRectangleV(position, size, DARKGRAY);
            }
        }

        {
            nob_da_foreach(Bullet, bullet, &state.enemy_bullets)
            {
                Vector2 position = world_to_screen(bullet->position, scale, offset, padding);
                Vector2 size = Vector2Scale(BULLET_SIZE, scale);
                DrawRectangleV(position, size, RED);
            }
        }

        {
            nob_da_foreach(Destroyable, destroyable, &state.destroyables)
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
            Vector2 position = world_to_screen(state.player.position, scale, offset, padding);
            Vector2 size = Vector2Scale(PLAYER_SIZE, scale);
            DrawRectangleV(position, size, BLUE);
        }

        if (!state.player.bullet.destroyed)
        {
            Vector2 position = world_to_screen(state.player.bullet.position, scale, offset, padding);
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

        state.player.position = Vector2Add(next_direction, state.player.position);
        // TODO: keep player inside box

        if (IsKeyDown(KEY_SPACE) && accumulator_tick(&state.player.shooting, GetFrameTime(), When_Tick_Ends_Keep) &&
            state.player.bullet.destroyed)
        {
            state.player.shooting.ms_accumulated = 0;
            state.player.bullet.position = (Vector2){
                .x = state.player.position.x,
                .y = state.player.position.y,
            };
            state.player.bullet.timing = (Accumulator){
                .ms_accumulated = 0,
                .ms_to_trigger = 200,
            };
            state.player.bullet.destroyed = false;
        }

        bool reached_wall = false;
        float enemy_speed = 0.1f * GetFrameTime();

        nob_da_foreach(Enemy, enemy, &state.enemies)
        {
            if (enemy->destroyed)
            {
                continue;
            }

            Vector2 new_position = (Vector2){
                .x = enemy->position.x + (state.enemies_going_right ? enemy_speed : -enemy_speed),
                .y = enemy->position.y,
            };

            if (new_position.x < 0 || new_position.x > COLUMNS)
            {
                reached_wall = true;
                break;
            }
        }

        if (reached_wall)
        {
            state.enemies_going_right = !state.enemies_going_right;
        }

        nob_da_foreach(Enemy, enemy, &state.enemies)
        {
            if (enemy->destroyed)
            {
                continue;
            }

            enemy->position = (Vector2){
                .x = enemy->position.x + (state.enemies_going_right ? enemy_speed : -enemy_speed),
                .y = enemy->position.y + (reached_wall ? 0.05f : 0.0f),
            };
        }

        nob_da_foreach(Enemy, enemy, &state.enemies)
        {
            if (enemy->destroyed)
            {
                continue;
            }

            if (accumulator_tick(&enemy->shooting, GetFrameTime(), When_Tick_Ends_Restart))
            {
                Bullet bullet = {
                    .position = {.x = enemy->position.x, .y = enemy->position.y},
                    .timing =
                        {
                            .ms_accumulated = 0,
                            .ms_to_trigger = 200,
                        },
                };
                nob_da_append(&state.enemy_bullets, bullet);
            }
        }

        {
            const Vector2 gravity = {
                .x = 0,
                .y = 10 * GetFrameTime(),
            };

            nob_da_foreach(Bullet, bullet, &state.enemy_bullets)
            {
                if (accumulator_tick(&bullet->timing, GetFrameTime(), When_Tick_Ends_Restart))
                {
                    bullet->position = Vector2Add(bullet->position, gravity);
                }
            }

            if (!state.player.bullet.destroyed)
            {
                state.player.bullet.position = Vector2Add(state.player.bullet.position, Vector2Scale(gravity, -1.0f));
            }
        }

        {
            nob_da_foreach(Bullet, bullet, &state.enemy_bullets)
            {
                if (bullet->position.y > GAME_ROWS)
                {
                    bullet->destroyed = true;
                    continue;
                }

                HitResult result = NONE;
                check_collisions(Destroyable, destroyable, &state.destroyables, DESTROYABLE_SIZE, DESTROYABLE,
                                 main_loop_on_collision);
                const struct
                {
                    Player *items;
                    size_t count;
                } player_ = {.items = &state.player, .count = 1};

                check_collisions(Player, player, &player_, PLAYER_SIZE, PLAYER, main_loop_on_collision);

            main_loop_on_collision:
                if (result == PLAYER)
                {
                    // TODO: player death state
                    printf("Player died!\n");
                    setup(&state);
                }
                continue;
            }

            for (int i = state.enemy_bullets.count - 1; i >= 0; --i)
            {
                Bullet *bullet = &state.enemy_bullets.items[i];

                if (bullet->destroyed)
                {
                    nob_da_remove_unordered(&state.enemy_bullets, i);
                }
            }
        }

        move_player_bullet(&state);

        nob_temp_reset();
    }
}
