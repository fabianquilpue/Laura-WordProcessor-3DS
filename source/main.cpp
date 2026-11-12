#include <3ds.h>
#include <citro2d.h>
#include <string>
#include <vector>
#include <cstdio>

// Estructura para representar cada tecla virtual en la pantalla inferior
struct KeyButton {
    float x, y, w, h;
    std::string label;
    char character;
    int action; // 0 = carácter normal, 1 = borrar (backspace), 2 = espacio, 3 = guardar
};

// Función para guardar el documento en la tarjeta SD
bool guardarEnSD(const std::string& contenido) {
    FILE* archivo = fopen("sdmc:/laura_documento.txt", "w");
    if (!archivo) return false;
    fwrite(contenido.c_str(), 1, contenido.size(), archivo);
    fclose(archivo);
    return true;
}

int main(int argc, char* argv[]) {
    // 1. Inicializar subsistemas esenciales de la 3DS
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    // 2. Crear los objetivos de renderizado para ambas pantallas
    C3D_RenderTarget* topScreen = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottomScreen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    // Buffers de texto estáticos
    C2D_TextBuf staticBuf = C2D_TextBufNew(2048);
    C2D_TextBuf dynamicBuf = C2D_TextBufNew(8192);

    C2D_Text headerText;
    C2D_TextParse(&headerText, staticBuf, "Laura - Procesador de Texto");
    C2D_TextOptimize(&headerText);

    // 3. Configurar la distribución del teclado táctil en la pantalla inferior (320x240)
    std::vector<KeyButton> keys;
    
    // Fila 1: Q W E R T Y U I O P
    std::string row1 = "QWERTYUIOP";
    float startX = 10.0f;
    float startY = 35.0f;
    float btnW = 28.0f;
    float btnH = 32.0f;
    float gap = 2.0f;

    for (size_t i = 0; i < row1.length(); i++) {
        keys.push_back({startX + i * (btnW + gap), startY, btnW, btnH, std::string(1, row1[i]), row1[i], 0});
    }

    // Fila 2: A S D F G H J K L
    std::string row2 = "ASDFGHJKL";
    float startX2 = 20.0f;
    float startY2 = startY + btnH + 5.0f;
    for (size_t i = 0; i < row2.length(); i++) {
        keys.push_back({startX2 + i * (btnW + gap), startY2, btnW, btnH, std::string(1, row2[i]), row2[i], 0});
    }

    // Fila 3: Z X C V B N M y Tecla Borrar
    std::string row3 = "ZXCVBNM";
    float startX3 = 35.0f;
    float startY3 = startY2 + btnH + 5.0f;
    for (size_t i = 0; i < row3.length(); i++) {
        keys.push_back({startX3 + i * (btnW + gap), startY3, btnW, btnH, std::string(1, row3[i]), row3[i], 0});
    }
    // Botón Backspace
    keys.push_back({startX3 + row3.length() * (btnW + gap), startY3, 45.0f, btnH, "DEL", 0, 1});

    // Fila 4: Espacio y Botón Guardar
    float startY4 = startY3 + btnH + 5.0f;
    keys.push_back({35.0f, startY4, 140.0f, 30.0f, "ESPACIO", ' ', 2});
    keys.push_back({185.0f, startY4, 95.0f, 30.0f, "GUARDAR", 0, 3});

    std::string documento = "";
    std::string estadoMensaje = "Toca las teclas en la pantalla inferior.";

    // 4. Bucle principal de la aplicación
    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        // Salir al menú Homebrew presionando START
        if (kDown & KEY_START) break;

        // Leer la posición táctil en la pantalla inferior
        touchPosition touch;
        hidTouchRead(&touch);

        if (kDown & KEY_TOUCH) {
            // Comprobar si el toque colisiona con alguna tecla virtual
            for (const auto& key : keys) {
                if (touch.px >= key.x && touch.px <= (key.x + key.w) &&
                    touch.py >= key.y && touch.py <= (key.y + key.h)) {
                    
                    if (key.action == 0) { // Carácter normal
                        documento += key.character;
                        estadoMensaje = "Tecla presionada: " + key.label;
                    } 
                    else if (key.action == 1) { // Borrar (Backspace)
                        if (!documento.empty()) {
                            documento.pop_back();
                        }
                        estadoMensaje = "Caracter borrado.";
                    } 
                    else if (key.action == 2) { // Espacio
                        documento += " ";
                        estadoMensaje = "Espacio anadido.";
                    } 
                    else if (key.action == 3) { // Guardar en SD
                        if (guardarEnSD(documento)) {
                            estadoMensaje = "Guardado en sdmc:/laura_documento.txt";
                        } else {
                            estadoMensaje = "Error al guardar en SD.";
                        }
                    }
                    break;
                }
            }
        }

        // Actualizar textos dinámicos en el buffer
        C2D_TextBufClear(dynamicBuf);
        C2D_Text docRenderText, statusRenderText;
        
        const char* textoMostrar = documento.empty() ? "(Comienza a escribir con el lapiz...)" : documento.c_str();
        C2D_TextParse(&docRenderText, dynamicBuf, textoMostrar);
        C2D_TextOptimize(&docRenderText);

        C2D_TextParse(&statusRenderText, dynamicBuf, estadoMensaje.c_str());
        C2D_TextOptimize(&statusRenderText);

        // Iniciar el fotograma gráfico
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        // --- PANTALLA SUPERIOR (Hoja de documento) ---
        C2D_TargetClear(topScreen, C2D_Color32(250, 250, 245, 255)); // Fondo papel claro
        C2D_SceneBegin(topScreen);

        // Barra de título superior
        C2D_DrawRectSolid(0, 0, 0.4f, 400, 24, C2D_Color32(45, 55, 72, 255));
        C2D_DrawText(&headerText, C2D_WithColor, 10.0f, 4.0f, 0.5f, 0.5f, 0.5f, C2D_Color32(255, 255, 255, 255));

        // Texto del documento con ajuste de línea automático
        C2D_DrawText(&docRenderText, C2D_WithColor | C2D_WordWrap, 10.0f, 34.0f, 0.5f, 0.52f, 0.52f, C2D_Color32(30, 30, 30, 255), 380.0f);

        // --- PANTALLA INFERIOR (Teclado Táctil y Estado) ---
        C2D_TargetClear(bottomScreen, C2D_Color32(230, 235, 242, 255)); // Fondo gris suave
        C2D_SceneBegin(bottomScreen);

        // Dibujar cada tecla virtual del teclado
        for (const auto& key : keys) {
            u32 colorFondo = C2D_Color32(255, 255, 255, 255);
            u32 colorTexto = C2D_Color32(30, 30, 35, 255);

            if (key.action == 3) { // Destacar el botón de guardar
                colorFondo = C2D_Color32(65, 130, 190, 255);
                colorTexto = C2D_Color32(255, 255, 255, 255);
            }

            C2D_DrawRectSolid(key.x, key.y, 0.3f, key.w, key.h, colorFondo);

            C2D_Text keyLabel;
            C2D_TextParse(&keyLabel, dynamicBuf, key.label.c_str());
            C2D_TextOptimize(&keyLabel);
            C2D_DrawText(&keyLabel, C2D_WithColor, key.x + 6.0f, key.y + 6.0f, 0.5f, 0.42f, 0.42f, colorTexto);
        }

        // Barra de estado inferior
        C2D_DrawRectSolid(0, 215, 0.4f, 320, 25, C2D_Color32(50, 60, 70, 255));
        C2D_DrawText(&statusRenderText, C2D_WithColor, 10.0f, 219.0f, 0.5f, 0.45f, 0.45f, C2D_Color32(255, 255, 255, 255));

        // Finalizar fotograma
        C3D_FrameEnd(0);
    }

    // 5. Liberar recursos al salir
    if (dynamicBuf) C2D_TextBufDelete(dynamicBuf);
    if (staticBuf) C2D_TextBufDelete(staticBuf);

    C2D_Fini();
    C3D_Fini();
    gfxExit();

    return 0;
}
