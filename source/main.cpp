#include <3ds.h>
#include <citro2d.h>

// Declaración de la función externa implementada en editor.cpp
void runEditorLoop();

int main(int argc, char* argv[]) {
    // Inicialización de servicios básicos para la Nintendo 3DS
    romfsInit();
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    // Ejecutar el bucle principal del editor de textos Laura
    runEditorLoop();

    // Cierre ordenado de los servicios al salir
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    romfsExit();
    return 0;
}
