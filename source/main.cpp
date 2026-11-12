#include <3ds.h>
#include <citro2d.h>
#include <string>

int main(int argc, char* argv[]) {
    // 1. Inicializar subsistemas esenciales de la 3DS
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    // 2. Crear los objetivos de renderizado para las pantallas superior e inferior
    C3D_RenderTarget* topScreen = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottomScreen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    // Buffer de texto y objeto de texto para la fuente por defecto del sistema
    C2D_TextBuf textBuf = C2D_TextBufNew(4096);
    C2D_Text titleText;
    C2D_TextParse(&titleText, textBuf, "Laura - Procesador de Texto (Sistema)");
    C2D_TextOptimize(&titleText);

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
        C2D_TargetClear(topScreen, C2D_Color32(40, 40, 50, 255)); // Fondo gris oscuro
        C2D_SceneBegin(topScreen);
        
        // Dibujar el título utilizando la fuente por defecto del sistema
        C2D_DrawText(&titleText, C2D_WithColor, 16.0f, 16.0f, 0.5f, 0.65f, 0.65f, C2D_Color32(255, 255, 255, 255));

        // --- Pantalla Inferior ---
        C2D_TargetClear(bottomScreen, C2D_Color32(30, 30, 40, 255));
        C2D_SceneBegin(bottomScreen);
        // Zona de edición o controles limpios sin menús de fuentes

        // Finalizar el fotograma
        C3D_FrameEnd(0);
    }

    // 4. Liberar recursos y cerrar servicios de forma segura al salir
    if (textBuf) C2D_TextBufDelete(textBuf);
    
    C2D_Fini();
    C3D_Fini();
    gfxExit();

    return 0;
}
