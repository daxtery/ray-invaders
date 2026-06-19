#include "game.h"
#include "raylib.h"
#include "raymath.h"
#include "stdlib.h"

typedef struct
{
    int frame;
} GameState;

GameState *our_state = NULL;

void *init(void)
{
    our_state = malloc(sizeof(GameState));
    our_state->frame = 0;
    return our_state;
}

void load(void *state)
{
    our_state = state;
}

void unload(void)
{
}

void loop(Texture2D plantTexture)
{
    BeginDrawing();

    ClearBackground(WHITE);

    size_t upper_corner_x = 17;
    size_t upper_corner_y = 0;
    size_t sprite_width = 416;
    size_t sprite_height = 416;

    size_t distance_between_sprites_x = 3;

    int frame = our_state->frame;
    int max_frames = 6;

    Rectangle sourceRec = {.x = upper_corner_x + frame * (distance_between_sprites_x + sprite_width),
                           .y = upper_corner_y,
                           .height = sprite_height,
                           .width = sprite_width};

    Rectangle destRec = {500, 500, sprite_width, sprite_height};

    DrawTexturePro(plantTexture, sourceRec, destRec, Vector2Zero(), 0.0f, WHITE);

    if (IsKeyPressed(KEY_RIGHT))
    {
        frame = (frame + 1) % max_frames;
    }
    else if (IsKeyPressed(KEY_LEFT))
    {
        frame = (frame - 1 + max_frames) % max_frames;
    }

    our_state->frame = frame;
    EndDrawing();
}
