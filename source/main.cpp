#include <3ds.h>
#include <citro2d.h>
#include <string>

int main(int argc, char* argv[]) {
    // 1. Inicializar subsistemas esenciales de la 3DS
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CBFSZ);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    
    // Inicializar el sistema de archivos ROMFS para leer fuentes y recursos
    romfsInit();

    // 2. Crear los objetivos de renderizado para las pantallas superior e inferior
    C3D_RenderTarget* topScreen = C2D_CreateTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottomScreen = C2D_CreateTarget(GFX_BOTTOM, GFX_LEFT);

    // 3. Bucle principal de la aplicación
    while (aptMainLoop()) {
        // Escanear entradas de los botones de la consola
        hidScanInput();
        u32 kDown = hidKeysDown();

        // Salir de la aplicación presionando el botón START
        if (kDown & KEY_START) break;

        // Iniciar el fotograma gráfico
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        
        // --- Pantalla Superior ---
        C2D_TargetClear(topScreen, C2D_Color32(35, 35, 45, 255)); // Fondo gris oscuro elegante
        C2D_SceneBegin(topScreen);
        // Aquí puedes dibujar elementos de texto o la interfaz principal de Laura

        // --- Pantalla Inferior ---
        C2D_TargetClear(bottomScreen, C2D_Color32(25, 25, 35, 255));
        C2D_SceneBegin(bottomScreen);
        // Aquí puedes dibujar los controles táctiles o el teclado virtual de edición

        // Finalizar el fotograma
        C3D_FrameEnd(0);
    }

    // 4. Liberar recursos y cerrar servicios de forma segura al salir
    romfsExit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();

    return 0;
}
