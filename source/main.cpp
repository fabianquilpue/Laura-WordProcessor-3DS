#include <3ds.h>
#include <citro2d.h>
#include <string>

int main(int argc, char* argv[]) {
    // Inicializar servicios esenciales de la 3DS
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CBFSZ);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    romfsInit(); // Vital si tu programa lee fuentes o assets desde romfs

    // Pantalla superior e inferior
    C3D_RenderTarget* topScreen = C2D_CreateTarget(GFX_TOP, GFX_LEFT);

    // Bucle principal de la aplicación
    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        // Salir con START
        if (kDown & KEY_START) break;

        // Renderizado gráfico
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(topScreen, C2D_Color32(30, 30, 30, 255)); // Fondo gris oscuro
        
        C2D_SceneBegin(topScreen);
        // Aquí se dibuja tu interfaz de texto de Laura

        C3D_FrameEnd(0);
    }

    // Liberar recursos al cerrar
    romfsExit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
