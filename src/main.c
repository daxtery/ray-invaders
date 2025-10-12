#include "accumulator.h"
#define NOB_IMPLEMENTATION
#include "nob.h"
#include "raylib.h"
#include "raymath.h"

#define min(a, b) (a) < (b) ? (a) : (b)

typedef struct
{
    uint8_t x;
    uint8_t y;
} AtlasPiece;

typedef struct
{
    uint8_t width;
    uint8_t height;
    uint8_t pieces_count;
    AtlasPiece pieces[];
} AtlasDefinition;

typedef struct
{
    Texture2D *texture;
    AtlasDefinition *atlas_definition;
    Accumulator accumulator;
    size_t current_frame;
} Animator;

typedef struct
{
    Vector2 position;
    Accumulator shooting;
    Animator animator;
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
    Animator animator;
    Bullet bullet;
    bool destroyed;
} Player;

typedef enum
{
    LOST,
    WAITING,
    PLAYING,
    WON,
} Status;
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
    //
    Status status;
} State;

#define ENEMY_ROWS 3
#define COLUMNS 8

#define EMPTY_ROWS 4
#define ENEMIES_GAME_OVER_ROW 5
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

static bool all_enemies_defeated(const State *state)
{
    nob_da_foreach(Enemy, enemy, &state->enemies)
    {
        if (enemy->destroyed)
        {
            return false;
        }
    }

    return true;
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
        if (all_enemies_defeated(state))
        {
            state->status = WON;
        }
    }
}

static void setup(State *state, AtlasDefinition *enemy_atlas, AtlasDefinition *player_atlas,
                  Texture2D *sprite_sheet_texture)
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
        .animator =
            {
                .atlas_definition = player_atlas,
                .accumulator =
                    {
                        .ms_accumulated = 0,
                        .ms_to_trigger = 200,
                    },
                .current_frame = 0,
                .texture = sprite_sheet_texture,
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
                        .ms_to_trigger = GetRandomValue(5000, 30000),
                    },
                .animator =
                    {
                        .texture = sprite_sheet_texture,
                        .atlas_definition = enemy_atlas,
                        .current_frame = 0,
                        .accumulator =
                            {
                                .ms_accumulated = 0,
                                .ms_to_trigger = 200,
                            },
                    },
                .destroyed = false,
            };
            nob_da_append(&state->enemies, enemy);
        }
    }

    for (size_t i = 0; i < 3; ++i)
    {
        int y = ENEMY_ROWS + 2;
        int x = (i + 1) * 2;

        int x_pieces = 15;
        int y_pieces = 5;

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

static bool MovePlayer(Vector2 *position)
{
    Vector2 next_direction = {0};

    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
    {
        next_direction.x += GetFrameTime();
    }
    else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))
    {
        next_direction.x -= GetFrameTime();
    }

    Vector2 new_position = Vector2Add(next_direction, *position);

    if (new_position.x <= 0)
    {
        new_position.x = 0;
    }
    else if (new_position.x >= COLUMNS)
    {
        new_position.x = COLUMNS;
    }

    *position = new_position;
    return next_direction.x != 0.0;
}

static void HandlePlayerShooting(Player *player)
{
    if (IsKeyDown(KEY_SPACE) && accumulator_tick(&player->shooting, GetFrameTime(), When_Tick_Ends_Keep) &&
        player->bullet.destroyed)
    {
        player->shooting.ms_accumulated = 0;
        player->bullet.position = (Vector2){
            .x = player->position.x + PLAYER_SIZE.x / 4,
            .y = player->position.y,
        };
        player->bullet.timing = (Accumulator){
            .ms_accumulated = 0,
            .ms_to_trigger = 200,
        };
        player->bullet.destroyed = false;
    }
}

int main(void)
{
    InitWindow(800, 600, "Ray Invaders Game in Raylib");

    SetTargetFPS(60);

    srand(time(NULL));

    Texture2D sprite_sheet_texture = LoadTexture("resources/SpaceInvaders.png");
    static AtlasDefinition enemy_frames = {
        .width = 16,
        .height = 16,
        .pieces_count = 2,
        .pieces = {{
                       .x = 0,
                       .y = 0,
                   },
                   {
                       .x = 1,
                       .y = 0,
                   }},
    };

    static AtlasDefinition player_frames = {
        .width = 16,
        .height = 16,
        .pieces_count = 1,
        .pieces = {{
            .x = 4,
            .y = 0,
        }},
    };

    float lastHeight = 0;
    float lastWidth = 0;

    State state = {0};
    state.status = WAITING;
    setup(&state, &enemy_frames, &player_frames, &sprite_sheet_texture);

    RenderTexture2D target;

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

        if (fabsf(height - lastHeight) > 0.001 || fabsf(width - lastWidth) > 0.001)
        {
            lastHeight = height;
            lastWidth = width;
            target = LoadRenderTexture(width, height);
        }

        switch (state.status)
        {
        case WAITING:
        case PLAYING: {
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

                    Rectangle destination_rec = {
                        .width = size.x,
                        .height = size.y,
                        .x = position.x,
                        .y = position.y,
                    };

                    if (accumulator_tick(&enemy->animator.accumulator, GetFrameTime(), When_Tick_Ends_Restart))
                    {
                        enemy->animator.current_frame =
                            (enemy->animator.current_frame + 1) % enemy->animator.atlas_definition->pieces_count;
                    }

                    AtlasPiece piece = enemy->animator.atlas_definition->pieces[enemy->animator.current_frame];
                    Rectangle source_rec = {.x = piece.x * enemy_frames.width,
                                            .y = piece.y * enemy_frames.height,
                                            .height = enemy_frames.height,
                                            .width = enemy_frames.width};

                    DrawTexturePro(*enemy->animator.texture, source_rec, destination_rec, Vector2Zero(), 0.0f, WHITE);
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

                Rectangle destination_rec = {
                    .width = size.x,
                    .height = size.y,
                    .x = position.x,
                    .y = position.y,
                };

                AtlasPiece piece = state.player.animator.atlas_definition->pieces[state.player.animator.current_frame];
                Rectangle source_rec = {.x = piece.x * player_frames.width,
                                        .y = piece.y * player_frames.height,
                                        .height = player_frames.height,
                                        .width = player_frames.width};

                DrawTexturePro(*state.player.animator.texture, source_rec, destination_rec, Vector2Zero(), 0.0f, WHITE);
            }

            if (!state.player.bullet.destroyed)
            {
                Vector2 position = world_to_screen(state.player.bullet.position, scale, offset, padding);
                Vector2 size = Vector2Scale(BULLET_SIZE, scale);
                DrawRectangleV(position, size, BLUE);
            }

            bool moved = MovePlayer(&state.player.position);
            if (moved && state.status == WAITING)
            {
                state.status = PLAYING;
            }

            bool reached_wall = false;
            float enemy_speed = 0.1f * GetFrameTime();

            if (state.status == PLAYING)
            {
                HandlePlayerShooting(&state.player);

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

                    if (enemy->position.y >= ENEMIES_GAME_OVER_ROW)
                    {
                        state.status = LOST;
                        break;
                    }
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
                            .position =
                                {
                                    .x = enemy->position.x + ENEMY_SIZE.x / 4,
                                    .y = enemy->position.y + ENEMY_SIZE.y,
                                },
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
                        state.player.bullet.position =
                            Vector2Add(state.player.bullet.position, Vector2Scale(gravity, -1.0f));
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
                            state.status = LOST;
                            break;
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
            }
            if (state.status == WAITING)
            {
                const char *text = nob_temp_sprintf("Move with A/D\nor Left/Right arrows\nto start");
                const size_t font_size = 50;
                Vector2 text_size = MeasureTextEx(GetFontDefault(), text, font_size, 0);
                Vector2 position = {
                    .x = GetScreenWidth() / 2,
                    .y = GetScreenHeight() / 2,
                };
                DrawText(text,
                         position.x - text_size.x / 2, //
                         position.y - text_size.y / 2, //
                         font_size,                    //
                         BLACK);
            }
            break;
        }
        case WON:
        case LOST: {
            BeginTextureMode(target);
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

            EndTextureMode();
            DrawTextureRec(target.texture,
                           (Rectangle){0, 0, (float)target.texture.width, (float)-target.texture.height}, Vector2Zero(),
                           RED);

            if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D) || IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))
            {
                state.status = PLAYING;
                setup(&state, &enemy_frames, &player_frames, &sprite_sheet_texture);
            }

            const char *text = state.status == LOST
                                   ? nob_temp_sprintf("Lost. Score: %d\nPress any key to restart", state.score)
                                   : nob_temp_sprintf("Won. Score: %d\nPress any key to restart", state.score);

            const size_t font_size = 50;
            Vector2 text_size = MeasureTextEx(GetFontDefault(), text, font_size, 0);
            Vector2 position = {
                .x = GetScreenWidth() / 2,
                .y = GetScreenHeight() / 2,
            };
            DrawText(text,
                     position.x - text_size.x / 2, //
                     position.y - text_size.y / 2, //
                     font_size,                    //
                     BLACK);
        }
        break;

        default:
            NOB_UNREACHABLE("Status was bad?\n");
            break;
        }

        EndDrawing();

        nob_temp_reset();
    }
}
