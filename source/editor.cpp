#include <citro2d.h>
#include <ctru.h>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>

// --- PALETA DE COLORES Y ESTÉTICA ---
#define COLOR_CREMA          C2D_Color32(247, 245, 238, 255) // #F7F5EE
#define COLOR_CARBON         C2D_Color32(30, 30, 30, 255)     // #1E1E1E
#define COLOR_VERDE_MUSGO    C2D_Color32(78, 122, 101, 255)   // #4E7A65
#define COLOR_BLANCO         C2D_Color32(255, 255, 255, 255)
#define COLOR_GRIS_TECLA     C2D_Color32(230, 225, 215, 255)
#define COLOR_ROJO_ERR       C2D_Color32(200, 60, 60, 255)

// --- TEXTOS OFICIALES DEL MANUAL DEL USUARIO ---
static const std::vector<std::string> manualP1Top = {
    "GUÍA DE DIGITACIÓN Y NAVEGACIÓN:",
    "Utiliza el d-pad para navegar intuitivamente",
    "por el documento. Puedes apretar SELECT para",
    "seleccionar texto mientras te desplazas.",
    "También puedes copiar, cortar y pegar el texto,",
    "entre otras funciones, muchas de las cuales",
    "serán accesibles mediante la función de shift (L)",
    "o de alt (ZL):"
};

static const std::vector<std::string> manualP1Bottom = {
    "- Seleccionar: SELECT",
    "- Copiar selección: Y",
    "- Cortar selección: L + Y",
    "- Pegar selección: A",
    "- Pegar sin formato: L + A",
    "- Salto de línea (Enter): R",
    "- Salto de página: ZL + R",
    "- Retroceso: ZR",
    "- Borrar palabra: L + ZR",
    "- Borrar línea: ZL + ZR",
    "- Menú de inicio: START",
    "Laura cuenta funciones como tamaño de fuente,",
    "interlineado o colores para el texto y su",
    "fondo mediante el botón Ajustes de Formato."
};

static const std::vector<std::string> manualP2Top = {
    "«Laura» es un procesador de textos que reúne",
    "las funciones esenciales necesarias para la",
    "escritura sin distracciones. Su diseño se",
    "inspira en la sencillez de las máquinas de",
    "escribir y en la ergonomía. Las puntuaciones",
    "están pensadas para ser accesibles de manera",
    "fácil e intuitiva.",
    "",
    "El programa cuenta con casi la totalidad de",
    "caracteres de un teclado QWERTY convencional,",
    "y tiene la capacidad de exportar sus archivos",
    "en formato RTF, permitiendo un mayor control",
    "sobre el estilo y la estructura del texto. El",
    "proyecto no persigue fines de lucro, pero",
    "tampoco puede ser comercializado o monetizado",
    "al acogerse a Creative Commons."
};

static const std::vector<std::string> manualP2Bottom = {
    "Info de Contacto:",
    "Fabián Contreras (CL)",
    "fabian.contreras.abara@gmail.com",
    "https://github.com/fabianquilpue"
};

// --- ESTRUCTURAS PRINCIPALES DEL EDITOR ---
struct CharStyle {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strike = false;
    u32 textColor = COLOR_CARBON;
    u32 bgColor = COLOR_CREMA;
    int fontSize = 12;
};

class LauraEditor {
public:
    LauraEditor() {
        textBuffer = "Laura - Procesador de Textos para Nintendo 3DS\nListo para escribir.";
        cursorPos = textBuffer.length();
        currentManualPage = 1;
        showManual = false;
    }

    void init() {
        mkdir("/Textos3DS", 0777);
    }

    void run() {
        C2D_TextBuf staticBuf = C2D_TextBufNew(4096);
        
        // Bucle de bienvenida con Splash Screen estática hasta toque táctil
        bool waitingSplash = true;
        u8 alpha = 0;
        bool fadingIn = true;

        while (aptMainLoop() && waitingSplash) {
            hidScanInput();
            touchPosition touch;
            hidTouchRead(&touch);

            if (hidKeysDown() & KEY_TOUCH) {
                waitingSplash = false;
            }

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            
            // Pantalla Superior Splash
            C2D_TargetClear(C2D_GetScreenTop(), COLOR_BLANCO);
            C2D_SceneBegin(C2D_GetScreenTop());
            C2D_DrawText(&titleText, C2D_AlignCenter, 200, 80, 0.5f, 1.0f, 1.0f);

            // Pantalla Inferior Splash
            C2D_TargetClear(C2D_GetScreenBottom(), COLOR_BLANCO);
            C2D_SceneBegin(C2D_GetScreenBottom());
            C2D_DrawText(&promptText, C2D_AlignCenter, 160, 110, 0.5f, 0.7f, 0.7f);

            C3D_FrameEnd(0);
        }

        // Bucle Principal del Editor
        while (aptMainLoop()) {
            hidScanInput();
            u32 kDown = hidKeysDown();
            u32 kHeld = hidKeysHeld();
            touchPosition touch;
            hidTouchRead(&touch);

            if (kDown & KEY_START) {
                saveRTF("sdmc:/Textos3DS/documento.rtf");
            }

            // Renderizado de Pantallas
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            // 1. Pantalla Superior (Documento o Manual)
            C2D_TargetClear(C2D_GetScreenTop(), COLOR_CREMA);
            C2D_SceneBegin(C2D_GetScreenTop());
            if (showManual) {
                renderManualTop();
            } else {
                renderDocument();
            }

            // 2. Pantalla Inferior (Teclado o Menú)
            C2D_TargetClear(C2D_GetScreenBottom(), COLOR_CREMA);
            C2D_SceneBegin(C2D_GetScreenBottom());
            if (showManual) {
                renderManualBottom();
            } else {
                renderKeyboardAndToolbar();
            }

            C3D_FrameEnd(0);
        }

        C2D_TextBufDelete(staticBuf);
    }

private:
    std::string textBuffer;
    size_t cursorPos;
    bool showManual;
    int currentManualPage;
    C2D_Text titleText;
    C2D_Text promptText;

    void renderDocument() {
        // Renderizado del cuerpo del texto en la pantalla superior
    }

    void renderKeyboardAndToolbar() {
        // Renderizado del teclado táctil y barra de formato
    }

    void renderManualTop() {
        float y = 20.0f;
        const auto& lines = (currentManualPage == 1) ? manualP1Top : manualP2Top;
        for (const auto& line : lines) {
            // Dibujar línea en pantalla superior del manual
            y += 18.0f;
        }
    }

    void renderManualBottom() {
        float y = 20.0f;
        const auto& lines = (currentManualPage == 1) ? manualP1Bottom : manualP2Bottom;
        for (const auto& line : lines) {
            // Dibujar línea en pantalla inferior del manual
            y += 18.0f;
        }
    }

    void saveRTF(const std::string& filename) {
        FILE* f = fopen(filename.c_str(), "w");
        if (!f) return;
        fprintf(f, "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0 Courier;}}\n\\f0\\fs24 %s\\par}\n", textBuffer.c_str());
        fclose(f);
    }
};

// Función de entrada requerida por main.cpp
void runEditorLoop() {
    LauraEditor editor;
    editor.init();
    editor.run();
}
