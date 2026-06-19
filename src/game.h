#include "raylib.h"

void loop(Texture2D plantTexture);
void *init(void);
void load(void *state);
void unload(void);

// the function signature as a ptr
typedef void (*loopfn)(Texture2D);
typedef void *(*initfn)(void);
typedef void (*loadfn)(void *);
typedef void (*unloadfn)(void);
