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
    uint8_t offset_width;
    uint8_t offset_height;
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
    Accumulator accumulator;
    Animator bullet_animator;
} EnemyShooting;

typedef struct
{
    AtlasDefinition *atlas;
    AtlasDefinition *bullet_atlas;
} EnemyTypeInfo;

typedef struct
{
    EnemyTypeInfo enemy_regular, enemy_squid, enemy_skull, enemy_head, enemy_horns;
} EnemyTypes;

typedef struct
{
    Vector2 position;
    EnemyShooting shooting;
    Animator animator;
    uint8_t health;
} Enemy;

typedef struct
{
    Enemy *items;
    size_t count;
    size_t capacity;
} Enemies;

typedef struct
{
    Animator animator;
    Accumulator timing;
    Vector2 position;
    bool destroyed;
} Bullet;

typedef struct
{
    Bullet *items;
    size_t count;
    size_t capacity;
} Bullets;

typedef struct
{
    Animator animator;
    Vector2 position;
    uint8_t health;
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
    uint8_t health;
} Player;

typedef struct
{
    Animator animator;
    bool finished;
} Particle;

typedef struct
{
    Particle *items;
    size_t count;
    size_t capacity;
} Particles;

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
    Destroyables destroyables;
    Particles particles;
    Player player;
    uint16_t score;
    Status status;
} State;

#define ENEMY_ROWS 3
#define COLUMNS 8

#define EMPTY_ROWS 4
#define ENEMIES_GAME_OVER_ROW 5
#define GAME_ROWS (ENEMY_ROWS + EMPTY_ROWS + 1)

static const Vector2 BULLET_SIZE = {
    .x = .3,
    .y = .3,
};

static const Vector2 DESTROYABLE_SIZE = {
    .x = 1.5,
    .y = .5,
};

static const Vector2 PLAYER_SIZE = {
    .x = 1,
    .y = 1,
};

static const Vector2 ENEMY_SIZE = {
    .x = 1,
    .y = 1,
};

static Vector2 world_to_screen(const Vector2 world_coordinates, float scale, const Vector2 offset)
{
    Vector2 position = Vector2Scale(world_coordinates, scale);
    position = Vector2Add(position, offset);
    return position;
}

typedef enum
{
    NONE,
    BULLET,
    DESTROYABLE,
    PLAYER = 3,
    ENEMY = 3,
} EntityType;

typedef struct
{
    EntityType entity_type;
    const void *entity;
} HitResult;

#define BULLET_DAMAGE 5

#define DESTROYABLE_FULL_HEALTH (4 * BULLET_DAMAGE)
#define DESTROYABLE_SECOND_HEALTH (3 * BULLET_DAMAGE)
#define DESTROYABLE_THIRD_HEALTH (2 * BULLET_DAMAGE)
#define DESTROYABLE_FOURTH_HEALTH (1 * BULLET_DAMAGE)

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
        if (it->health <= 0)                                                                                           \
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
            it->health -= BULLET_DAMAGE;                                                                               \
            bullet->destroyed = true;                                                                                  \
            result = (HitResult){                                                                                      \
                .entity = it,                                                                                          \
                .entity_type = Checking,                                                                               \
            };                                                                                                         \
            goto label;                                                                                                \
        }                                                                                                              \
    }

static bool all_enemies_defeated(const State *state)
{
    nob_da_foreach(Enemy, enemy, &state->enemies)
    {
        if (enemy->health > 0)
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
    }

    if (bullet->destroyed)
    {
        return;
    }

    HitResult result = {0};

    check_collisions(Enemy, enemy, &state->enemies, ENEMY_SIZE, ENEMY, move_player_bullet_after_collision);
    check_collisions(Destroyable, destroyable, &state->destroyables, DESTROYABLE_SIZE, DESTROYABLE,
                     move_player_bullet_after_collision);
    goto no_hit;
move_player_bullet_after_collision:
    if (result.entity_type == ENEMY)
    {
        state->score += 10;

        if (all_enemies_defeated(state))
        {
            state->status = WON;
        }
    }
no_hit:
}

static void setup(State *state, const EnemyTypes *enemy_types, AtlasDefinition *player_atlas,
                  AtlasDefinition *destroyable_atlas, Texture2D *sprite_sheet_texture)
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
        .health = BULLET_DAMAGE,
    };

    state->score = 0;

    for (size_t i = 0; i < COLUMNS; ++i)
    {
        for (size_t j = 0; j < ENEMY_ROWS; ++j)
        {
            EnemyTypeInfo info = (j == 0 || j == 1) ? enemy_types->enemy_squid : enemy_types->enemy_regular;

            AtlasDefinition *atlas = info.atlas;
            AtlasDefinition *bullet_atlas = info.bullet_atlas;

            Enemy enemy = {
                .position = {.x = i, .y = j},
                .shooting =
                    (EnemyShooting){
                        .accumulator =
                            {
                                .ms_accumulated = 0,
                                .ms_to_trigger = GetRandomValue(5000, 30000),
                            },
                        .bullet_animator =
                            {
                                .texture = sprite_sheet_texture,
                                .atlas_definition = bullet_atlas,
                                .current_frame = 0,
                                .accumulator =
                                    {
                                        .ms_accumulated = 0,
                                        .ms_to_trigger = 200,
                                    },
                            },
                    },
                .animator =
                    {
                        .texture = sprite_sheet_texture,
                        .atlas_definition = atlas,
                        .current_frame = 0,
                        .accumulator =
                            {
                                .ms_accumulated = 0,
                                .ms_to_trigger = 200,
                            },
                    },
                .health = BULLET_DAMAGE,
            };
            nob_da_append(&state->enemies, enemy);
        }
    }

    for (size_t i = 0; i < 3; ++i)
    {
        int y = ENEMY_ROWS + 2;
        int x = (i + 1) * 2;
        nob_da_append(&state->destroyables, ((Destroyable){
                                                .health = DESTROYABLE_FULL_HEALTH,
                                                .animator =
                                                    {
                                                        .accumulator = {.ms_accumulated = 0, .ms_to_trigger = 0},
                                                        .atlas_definition = destroyable_atlas,
                                                        .texture = sprite_sheet_texture,
                                                        .current_frame = 0,
                                                    },
                                                .position =
                                                    {
                                                        .x = x,
                                                        .y = y,
                                                    },
                                            }));
    }
}

static bool move_player(Vector2 *position)
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

static void handle_player_shooting(Player *player, const Animator *bullet_animator)
{
    if (IsKeyDown(KEY_SPACE) && accumulator_tick(&player->shooting, GetFrameTime(), When_Tick_Ends_Keep) &&
        player->bullet.destroyed)
    {
        player->shooting.ms_accumulated = 0;
        player->bullet.position = (Vector2){
            .x = player->position.x + PLAYER_SIZE.x / 2,
            .y = player->position.y,
        };
        player->bullet.timing = (Accumulator){
            .ms_accumulated = 0,
            .ms_to_trigger = 200,
        };
        player->bullet.animator = *bullet_animator;
        player->bullet.destroyed = false;
    }
}

static void draw_sprite(const Animator *animator, float scale, const Vector2 offset, Vector2 world_position,
                        const Vector2 world_size)
{
    Vector2 position = world_to_screen(world_position, scale, offset);
    Vector2 size = Vector2Scale(world_size, scale);

    Rectangle destination_rec = {
        .width = size.x,
        .height = size.y,
        .x = position.x,
        .y = position.y,
    };

    AtlasPiece piece = animator->atlas_definition->pieces[animator->current_frame];
    Rectangle source_rec = {.x = piece.x * animator->atlas_definition->width + animator->atlas_definition->offset_width,
                            .y = piece.y * animator->atlas_definition->height +
                                 animator->atlas_definition->offset_height,
                            .height = animator->atlas_definition->height,
                            .width = animator->atlas_definition->width};

    DrawTexturePro(*animator->texture, source_rec, destination_rec, Vector2Zero(), 0.0f, WHITE);
}

static void draw_game(const State *state, float scale, const Vector2 offset)
{
    {
        nob_da_foreach(Enemy, enemy, &state->enemies)
        {
            if (enemy->health <= 0)
            {
                continue;
            }

            draw_sprite(&enemy->animator, scale, offset, enemy->position, ENEMY_SIZE);
        }
    }

    {
        nob_da_foreach(Bullet, bullet, &state->enemy_bullets)
        {
            draw_sprite(&bullet->animator, scale, offset, bullet->position, BULLET_SIZE);
        }
    }

    {
        nob_da_foreach(Destroyable, destroyable, &state->destroyables)
        {
            if (destroyable->health <= 0)
            {
                continue;
            }

            switch (destroyable->health)
            {
            case DESTROYABLE_FULL_HEALTH:
                destroyable->animator.current_frame = 0;
                break;
            case DESTROYABLE_SECOND_HEALTH:
                destroyable->animator.current_frame = 1;
                break;
            case DESTROYABLE_THIRD_HEALTH:
                destroyable->animator.current_frame = 2;
                break;
            case DESTROYABLE_FOURTH_HEALTH:
                destroyable->animator.current_frame = 3;
                break;
            default:
                NOB_UNREACHABLE("Destroyable health was unexpected!\n");
            }
            draw_sprite(&destroyable->animator, scale, offset, destroyable->position, DESTROYABLE_SIZE);
        }

        {
            draw_sprite(&state->player.animator, scale, offset, state->player.position, PLAYER_SIZE);
        }

        if (!state->player.bullet.destroyed)
        {
            draw_sprite(&state->player.bullet.animator, scale, offset, state->player.bullet.position, BULLET_SIZE);
        }
    }
}

static AtlasDefinition squid_frames = {
    .width = 16,
    .height = 16,
    .pieces_count = 2,
    .offset_height = 0,
    .offset_width = 0,
    .pieces = {{
                   .x = 0,
                   .y = 1,
               },
               {
                   .x = 1,
                   .y = 1,
               }},
};

static AtlasDefinition squid_bullet_frames = {
    .width = 16,
    .height = 16,
    .offset_width = 0,
    .offset_height = 0,
    .pieces_count = 2,
    .pieces =
        {
            {
                .x = 2,
                .y = 1,
            },
            {
                .x = 5,
                .y = 1,
            },
        },
};

static AtlasDefinition skull_frames = {
    .width = 16,
    .height = 16,
    .pieces_count = 2,
    .offset_height = 0,
    .offset_width = 0,
    .pieces = {{
                   .x = 0,
                   .y = 2,
               },
               {
                   .x = 1,
                   .y = 2,
               }},
};

static AtlasDefinition skull_bullet_frames = {
    .width = 16,
    .height = 16,
    .pieces_count = 1,
    .offset_width = 0,
    .offset_height = 0,
    .pieces =
        {
            {
                .x = 2,
                .y = 2,
            },
        },
};

static AtlasDefinition regular_frames = {
    .width = 16,
    .height = 16,
    .pieces_count = 2,
    .offset_height = 0,
    .offset_width = 0,
    .pieces = {{
                   .x = 0,
                   .y = 0,
               },
               {
                   .x = 1,
                   .y = 0,
               }},
};

static AtlasDefinition regular_bullet_frames = {
    .width = 16,
    .height = 16,
    .pieces_count = 1,
    .offset_width = 0,
    .offset_height = 0,
    .pieces =
        {
            {
                .x = 2,
                .y = 0,
            },
        },
};

static AtlasDefinition player_frames = {
    .width = 16,
    .height = 16,
    .offset_height = 0,
    .offset_width = 0,
    .pieces_count = 1,
    .pieces = {{
        .x = 4,
        .y = 0,
    }},
};

static AtlasDefinition player_bullet_atlas = {
    .width = 16,
    .height = 16,
    .offset_width = 0,
    .offset_height = 0,
    .pieces_count = 1,
    .pieces =
        {
            {
                .x = 2,
                .y = 0,
            },
        },
};

static AtlasDefinition destroyable_frames = {
    .width = 32,
    .height = 16,
    .offset_width = 16 * 3,
    .offset_height = 16,
    .pieces_count = 4,
    .pieces = {{
                   .x = 0,
                   .y = 0,
               },
               {
                   .x = 0,
                   .y = 1,
               },
               {
                   .x = 0,
                   .y = 2,
               },
               {
                   .x = 0,
                   .y = 3,
               }},
};

int main(void)
{
    InitWindow(800, 600, "Ray Invaders Game in Raylib");

    SetTargetFPS(60);

    srand(time(NULL));

    Texture2D sprite_sheet_texture = LoadTexture("resources/SpaceInvaders.png");
    Texture2D background_texture = LoadTexture("resources/background.jpg");

    EnemyTypes enemy_types = {
        .enemy_regular =
            {
                .atlas = &regular_frames,
                .bullet_atlas = &regular_bullet_frames,
            },
        .enemy_squid =
            {
                .atlas = &squid_frames,
                .bullet_atlas = &squid_bullet_frames,
            },
        .enemy_skull =
            {
                .atlas = &skull_frames,
                .bullet_atlas = &skull_bullet_frames,
            },
        .enemy_head =
            {
                .atlas = &regular_frames,
                .bullet_atlas = &regular_bullet_frames,
            },
        .enemy_horns =
            {
                .atlas = &squid_frames,
                .bullet_atlas = &squid_bullet_frames,
            },
    };

    float lastHeight = 0;
    float lastWidth = 0;

    State state = {0};
    state.status = WAITING;
    setup(&state, &enemy_types, &player_frames, &destroyable_frames, &sprite_sheet_texture);

    Animator player_bullet_animator = {
        .accumulator = {0},
        .atlas_definition = &player_bullet_atlas,
        .current_frame = 0,
        .texture = &sprite_sheet_texture,
    };

    RenderTexture2D target;

    float background_x = 0.f;
    bool background_x_dir = false;

    float background_y = 0.f;
    bool background_y_dir = false;

    Accumulator time_to_accept_input = {
        .ms_accumulated = 0,
        .ms_to_trigger = 1000,
    };

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        background_x += (background_x_dir ? -1.f : 1.f) * GetFrameTime();
        background_y += (background_y_dir ? -1.f : 1.f) * GetFrameTime();

        if (background_x > 2.f)
        {
            background_x_dir = true;
        }
        else if (background_x < -2.f)
        {
            background_x_dir = false;
        }

        if (background_y > 2.f)
        {
            background_y_dir = true;
        }
        else if (background_y < -2.f)
        {
            background_y_dir = false;
        }

        DrawTexturePro(background_texture,
                       (Rectangle){
                           .height = background_texture.height,
                           .width = background_texture.width,
                           .x = background_x,
                           .y = background_y,
                       },
                       (Rectangle){
                           .height = GetScreenHeight(),
                           .width = GetScreenWidth(),
                           .x = 0.f,
                           .y = 0.f,
                       },
                       Vector2Zero(), 0.f, WHITE);

        float width = GetScreenWidth();
        float offset_width = width * 0.05f;
        float width_for_game = width - offset_width;
        float size_x = width_for_game / COLUMNS;

        float height = GetScreenHeight();
        float offset_height = height * 0.05f;
        float height_for_game = height - offset_height;
        float size_y = (height_for_game) / (float)GAME_ROWS;

        float scale = min(size_y, size_x);

        float width_left = width - offset_width / 2;
        float actual_width_used = scale * COLUMNS;

        Vector2 offset = {
            .x = offset_width / 2 + (width_left / 2 - actual_width_used / 2),
            .y = offset_height,
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
                const size_t font_size = 25;
                Vector2 text_size = MeasureTextEx(GetFontDefault(), text, font_size, 0);
                Vector2 position = {
                    .x = GetScreenWidth() / 2,
                    .y = 20,
                };
                DrawText(text,
                         position.x - text_size.x / 2, //
                         position.y - text_size.y / 2, //
                         font_size,                    //
                         WHITE);
            }
            if (state.status == PLAYING)
            {
                nob_da_foreach(Enemy, enemy, &state.enemies)
                {
                    if (enemy->health <= 0)
                    {
                        continue;
                    }

                    if (accumulator_tick(&enemy->animator.accumulator, GetFrameTime(), When_Tick_Ends_Restart))
                    {
                        enemy->animator.current_frame =
                            (enemy->animator.current_frame + 1) % enemy->animator.atlas_definition->pieces_count;
                    }
                }

                if (accumulator_tick(&state.player.animator.accumulator, GetFrameTime(), When_Tick_Ends_Restart))
                {
                    state.player.animator.current_frame = (state.player.animator.current_frame + 1) %
                                                          state.player.animator.atlas_definition->pieces_count;
                }
            }

            draw_game(&state, scale, offset);

            bool moved = move_player(&state.player.position);
            if (moved && state.status == WAITING)
            {
                state.status = PLAYING;
            }

            bool reached_wall = false;
            float enemy_speed = 0.1f * GetFrameTime();

            if (state.status == PLAYING)
            {
                handle_player_shooting(&state.player, &player_bullet_animator);

                nob_da_foreach(Enemy, enemy, &state.enemies)
                {
                    if (enemy->health <= 0)
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
                    if (enemy->health <= 0)
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
                    if (enemy->health <= 0)
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
                    if (enemy->health <= 0)
                    {
                        continue;
                    }

                    if (accumulator_tick(&enemy->shooting.accumulator, GetFrameTime(), When_Tick_Ends_Restart))
                    {
                        Bullet bullet = {
                            .position =
                                {
                                    .x = enemy->position.x + ENEMY_SIZE.x / 2,
                                    .y = enemy->position.y + ENEMY_SIZE.y,
                                },
                            .timing =
                                {
                                    .ms_accumulated = 0,
                                    .ms_to_trigger = 200,
                                },
                            .animator = enemy->shooting.bullet_animator,
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

                        if (accumulator_tick(&bullet->animator.accumulator, GetFrameTime(), When_Tick_Ends_Restart))
                        {
                            bullet->animator.current_frame =
                                (bullet->animator.current_frame + 1) % bullet->animator.atlas_definition->pieces_count;
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

                        HitResult result = {0};
                        check_collisions(Destroyable, destroyable, &state.destroyables, DESTROYABLE_SIZE, DESTROYABLE,
                                         main_loop_on_collision);
                        const struct
                        {
                            Player *items;
                            size_t count;
                        } player_ = {.items = &state.player, .count = 1};

                        check_collisions(Player, player, &player_, PLAYER_SIZE, PLAYER, main_loop_on_collision);

                    main_loop_on_collision:
                        if (result.entity_type == PLAYER)
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
                const char *text =
                    nob_temp_sprintf("Move with A/D\nor Left/Right arrows.\n Space to shoot.\nMove to start playing.");
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
                         WHITE);
            }
            break;
        }
        case WON:
        case LOST: {
            BeginTextureMode(target);

            draw_game(&state, scale, offset);

            EndTextureMode();
            DrawTextureRec(target.texture,
                           (Rectangle){0, 0, (float)target.texture.width, (float)-target.texture.height}, Vector2Zero(),
                           RED);

            if (accumulator_tick(&time_to_accept_input, GetFrameTime(), When_Tick_Ends_Keep))
            {
                if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D) || IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))
                {
                    accumulator_reset(&time_to_accept_input);
                    state.status = PLAYING;
                    setup(&state, &enemy_types, &player_frames, &destroyable_frames, &sprite_sheet_texture);
                    continue;
                }
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
                     WHITE);
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
