#include <3ds.h>
#include <citro2d.h>

void runEditorLoop();

int main(int argc, char* argv[]) {
    gfxInitDefault();
    ptmuInit();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    runEditorLoop();

    C2D_Fini();
    C3D_Fini();
    ptmuExit();
    gfxExit();
    return 0;
}
