#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#define NOB_EXPERIMENTAL_DELETE_OLD
#include "nob.h"

#define SRC_FOLDER "src/"
#define BUILD_FOLDER "build/"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (!mkdir_if_not_exists(BUILD_FOLDER))
    {
        return 1;
    }

    Cmd cmd = {0};

    cmd_append(&cmd, "cc", "-fdiagnostics-color=always", "-Wall", "-Wextra");
    cmd_append(&cmd, "-g");
    cmd_append(&cmd, "-rdynamic");
    cmd_append(&cmd, "-o", "main", SRC_FOLDER "main.c");
    cmd_append(&cmd, "-I./libs/raylib-5.5_linux_amd64/include/");
    cmd_append(&cmd, "-I./" SRC_FOLDER);
    cmd_append(&cmd, "-I.");
    cmd_append(&cmd, "-L.");
    cmd_append(&cmd, "-L./libs/raylib-5.5_linux_amd64/lib/");
    cmd_append(&cmd, "-l:libraylib.a");
    cmd_append(&cmd, "-lm");
    cmd_append(&cmd, "-Wl,-rpath,'$ORIGIN'");
    if (!cmd_run(&cmd))
    {
        return 1;
    }

    // gcc -g -shared -fPIC -o libgame.so src/game.c   -I./libs/raylib-5.5_linux_amd64/include
    // -L./libs/raylib-5.5_linux_amd64/lib/   -l:libraylib.so   -Wl,-rpath,'$ORIGIN/libs/raylib-5.5_linux_amd64/lib/'

    return 0;
}
