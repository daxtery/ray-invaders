#include "game.h"
#include "raylib.h"
#include "raymath.h"
#include "stdlib.h"
#include <stdio.h>
#include <string.h>

#include <errno.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <dlfcn.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

#define GAME_LIB_NAME "libgame.so"
#define GAME_LIB_PATH "./" GAME_LIB_NAME

#define SRC_FOLDER "src/"

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

bool compile_game_library(void)
{
    printf("🛠️ [Engine] Changes detected in game.c! Recompiling...\n");

    Cmd cmd = {0};

    cmd_append(&cmd, "cc", "-Wall", "-Wextra");
    cmd_append(&cmd, "-g");
    cmd_append(&cmd, "-shared");
    cmd_append(&cmd, "-fPIC");
    cmd_append(&cmd, "-o", GAME_LIB_NAME, SRC_FOLDER "game.c");
    cmd_append(&cmd, "-I./libs/raylib-5.5_linux_amd64/include/");
    cmd_append(&cmd, "-L./libs/raylib-5.5_linux_amd64/lib/");
    cmd_append(&cmd, "-l:libraylib.so");
    cmd_append(&cmd, "-Wl,-rpath,/home/daxtery/sts-combat/libs/raylib-5.5_linux_amd64/lib/");
    if (cmd_run(&cmd))
    {
        printf("✅ [Engine] Compilation successful!\n");
        return true;
    }

    fprintf(stderr, "❌ [Engine] Compilation FAILED\n");
    return false;
}

typedef struct
{
    void *handle;
    loopfn loop;
    initfn init;
    loadfn load;
    unloadfn unload;
} Library;

Library load_library(void)
{
    Library lib = {0};
    lib.handle = dlopen(GAME_LIB_PATH, RTLD_LAZY | RTLD_GLOBAL);
    if (!lib.handle)
    {
        fprintf(stderr, "❌ [Engine] Failed to load library: %s\n", dlerror());
        return lib;
    }

    lib.loop = (loopfn)dlsym(lib.handle, "loop");
    lib.init = (initfn)dlsym(lib.handle, "init");
    lib.load = (loadfn)dlsym(lib.handle, "load");
    lib.unload = (unloadfn)dlsym(lib.handle, "unload");

    if (!lib.loop || !lib.init || !lib.load || !lib.unload)
    {
        fprintf(stderr, "❌ [Engine] Failed to load functions from library: %s\n", dlerror());
        dlclose(lib.handle);
        lib.handle = NULL;
    }

    return lib;
}

int main(void)
{
    // 1. Initialize Inotify in non-blocking mode
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0)
    {
        perror("inotify_init1");
        return 1;
    }

    inotify_add_watch(fd, SRC_FOLDER, IN_MODIFY);

    InitWindow(800, 450, "Raylib - Fixed Grid Spritesheet");

    int monitor = GetCurrentMonitor();
    int screenWidth = GetMonitorWidth(monitor);
    int screenHeight = GetMonitorHeight(monitor);
    SetWindowSize(screenWidth, screenHeight);
    ToggleFullscreen();

    SetTargetFPS(60);

    compile_game_library();
    Library lib = load_library();
    if (!lib.handle)
    {
        fprintf(stderr, "❌ [Engine] Failed to load library\n");
        exit(EXIT_FAILURE);
    }

    // Call the init function
    void *state = lib.init();
    if (!state)
    {
        fprintf(stderr, "❌ [Engine] Failed to initialize library\n");
        exit(EXIT_FAILURE);
    }

    // Call the load function
    lib.load(state);
    // Load the black-background spritesheet
    Texture2D plantTexture = LoadTexture("resources/plant_spritesheet.png");

    char buffer[BUF_LEN];

    while (!WindowShouldClose())
    {
        int length = read(fd, buffer, BUF_LEN);

        if (length > 0)
        {
            int i = 0;
            bool reload_triggered = false;

            while (i < length)
            {
                struct inotify_event *event = (struct inotify_event *)&buffer[i];
                if (event->len)
                {
                    // Check if the modified file is our library
                    if (strcmp(event->name, "game.c") == 0)
                    {
                        reload_triggered = true;
                    }
                }
                i += EVENT_SIZE + event->len;
            }

            // 4. Reload the library safely
            if (reload_triggered)
            {
                usleep(50000);

                if (compile_game_library())
                {
                    // 3. If compilation succeeded, reload the new shared object
                    if (lib.handle)
                        dlclose(lib.handle);

                    lib = load_library();
                    if (lib.handle)
                    {
                        lib.load(state);
                        printf("⚡ [Engine] Live reloaded game logic!\n");
                    }
                    else
                    {
                        fprintf(stderr, "❌ [Engine] Failed to reload game logic: %s\n", dlerror());
                    }
                }
            }
        }

        lib.loop(plantTexture);
    }

    UnloadTexture(plantTexture);
    CloseWindow();

    return 0;
}
