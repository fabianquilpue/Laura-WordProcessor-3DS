#include <3ds.h>
#include <citro2d.h>
#include <string>
#include <vector>
#include <cstdio>
#include <sstream>

// Estructura para representar cada tecla con sus tres capas (Base, Shift/L, CtrlAlt/R)
struct KeyButton {
    float x, y, w, h;
    std::string label;
    std::string base;
    std::string shift;
    std::string ctrlAlt;
    int type; // 0 = texto/carácter, 1 = borrar (Backspace), 2 = espacio, 3 = guardar
};

// Función para guardar el documento en la tarjeta SD
bool guardarEnSD(const std::string& contenido) {
    FILE* archivo = fopen("sdmc:/laura_documento.txt", "w");
    if (!archivo) return false;
    fwrite(contenido.c_str(), 1, contenido.size(), archivo);
    fclose(archivo);
    return true;
}

// Función auxiliar para conteo de palabras
int contarPalabras(const std::string& texto) {
    std::stringstream ss(texto);
    std::string palabra;
    int contador = 0;
    while (ss >> palabra) contador++;
    return contador;
}

int main(int argc, char* argv[]) {
    // 1. Inicializar subsistemas de la 3DS
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget* topScreen = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottomScreen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    C2D_TextBuf staticBuf = C2D_TextBufNew(2048);
    C2D_TextBuf dynamicBuf = C2D_TextBufNew(8192);

    C2D_Text headerText;
    C2D_TextParse(&headerText, staticBuf, "Laura - Procesador de Texto");
    C2D_TextOptimize(&headerText);

    // 2. Construcción optimizada y proporcionada del teclado multicapa (Pantalla inferior 320x240)
    std::vector<KeyButton> keys;
    float btnW = 26.0f;
    float btnH = 24.0f;
    float gap = 3.0f;

    // Fila 1: Números y símbolos superiores (11 teclas)
    std::string r1_base  = "º1234567890";
    std::string r1_shift = "ª*\"#$%&/()=";
    std::string r1_ctrl  = " |@@  [[]]+";
    float f1_x = 8.0f;
    float f1_y = 15.0f;
    for (int i = 0; i < 11; i++) {
        std::string b(1, r1_base[i]);
        std::string s(1, r1_shift[i]);
        std::string c(1, r1_ctrl[i]);
        keys.push_back({f1_x + i * (btnW + gap), f1_y, btnW, btnH, b, b, s, c, 0});
    }

    // Fila 2: Q W E R T Y U I O P (10 teclas)
    std::string r2_base  = "qwertyuiop";
    std::string r2_shift = "QWERTYUIOP";
    float f2_x = 22.0f;
    float f2_y = f1_y + btnH + 3.0f;
    for (int i = 0; i < 10; i++) {
        std::string b(1, r2_base[i]);
        std::string s(1, r2_shift[i]);
        std::string c = (r2_base[i] == 'e') ? "€" : "";
        keys.push_back({f2_x + i * (btnW + gap), f2_y, btnW, btnH, b, b, s, c, 0});
    }

    // Fila 3: A S D F G H J K L Ñ ´ (11 teclas)
    std::string r3_base  = "asdfghjklñ´";
    std::string r3_shift = "ASDFGHJKLÑ'";
    float f3_x = 10.0f;
    float f3_y = f2_y + btnH + 3.0f;
    for (int i = 0; i < 11; i++) {
        std::string b(1, r3_base[i]);
        std::string s(1, r3_shift[i]);
        std::string c = (r3_base[i] == 'a') ? "~" : "";
        keys.push_back({f3_x + i * (btnW + gap), f3_y, btnW, btnH, b, b, s, c, 0});
    }

    // Fila 4: ¿ ¡ Z X C V B N M ! ? + Backspace (11 elementos)
    std::string r4_base  = "¿¡zxcvbnm!?";
    std::string r4_shift = "¿¡ZXCVBNM!?";
    float f4_x = 8.0f;
    float f4_y = f3_y + btnH + 3.0f;
    for (int i = 0; i < 10; i++) {
        std::string b(1, r4_base[i]);
        std::string s(1, r4_shift[i]);
        std::string c = "";
        if (r4_base[i] == 'z') c = "{";
        if (r4_base[i] == 'c') c = "ç";
        if (r4_base[i] == 'm') c = "}";
        keys.push_back({f4_x + i * (btnW + gap), f4_y, btnW, btnH, b, b, s, c, 0});
    }
    // Botón Backspace
    keys.push_back({f4_x + 10 * (btnW + gap), f4_y, 42.0f, btnH, "DEL", "DEL", "DEL", "DEL", 1});

    // Fila 5: Franja de puntuación (« , . … : ; - — ») (9 teclas)
    std::string puncs[9] = {"«", ",", ".", "…", ":", ";", "-", "—", "»"};
    float f5_x = 22.0f;
    float f5_y = f4_y + btnH + 3.0f;
    for (int i = 0; i < 9; i++) {
        keys.push_back({f5_x + i * (28.0f + gap), f5_y, 28.0f, btnH, puncs[i], puncs[i], puncs[i], puncs[i], 0});
    }

    // Fila 6: Barra Espaciadora y Botón Guardar
    float f6_y = f5_y + btnH + 3.0f;
    keys.push_back({15.0f, f6_y, 160.0f, 25.0f, "ESPACIO", "ESPACIO", "TAB", " ", 2});
    keys.push_back({182.0f, f6_y, 125.0f, 25.0f, "GUARDAR", "GUARDAR", "GUARDAR", "GUARDAR", 3});

    std::string documento = "TE AMO LAURITA";
    std::string estadoMensaje = "Teclado multicapa completo activo.";

    // 3. Bucle principal de la aplicación
    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        if (kDown & KEY_START) break;

        // Modificadores físicos L (Shift) y R (Ctrl+Alt)
        bool isShift = (kHeld & KEY_L);
        bool isCtrlAlt = (kHeld & KEY_R);

        touchPosition touch;
        hidTouchRead(&touch);

        if (kDown & KEY_TOUCH) {
            for (const auto& key : keys) {
                if (touch.px >= key.x && touch.px <= (key.x + key.w) &&
                    touch.py >= key.y && touch.py <= (key.y + key.h)) {
                    
                    if (key.type == 0) { // Carácter normal
                        std::string charToAdd = key.base;
                        if (isCtrlAlt && !key.ctrlAlt.empty()) charToAdd = key.ctrlAlt;
                        else if (isShift) charToAdd = key.shift;

                        documento += charToAdd;
                        estadoMensaje = "Agregado: " + charToAdd;
                    } 
                    else if (key.type == 1) { // Backspace
                        if (!documento.empty()) documento.pop_back();
                        estadoMensaje = "Caracter borrado.";
                    } 
                    else if (key.type == 2) { // Espacio / Tab
                        if (isShift) documento += "\t";
                        else documento += " ";
                        estadoMensaje = isShift ? "Tabulacion." : "Espacio.";
                    } 
                    else if (key.type == 3) { // Guardar
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

        // Preparar textos para renderizado
        C2D_TextBufClear(dynamicBuf);
        C2D_Text docRenderText, statusRenderText;
        
        const char* textoMostrar = documento.empty() ? "(Escribe con el teclado tactil...)" : documento.c_str();
        C2D_TextParse(&docRenderText, dynamicBuf, textoMostrar);
        C2D_TextOptimize(&docRenderText);

        int palabras = contarPalabras(documento);
        int caracteres = documento.length();
        char infoEstado[128];
        snprintf(infoEstado, sizeof(infoEstado), "Palabras: %d | Caracteres: %d | L: %s | R: %s", 
                 palabras, caracteres, isShift ? "ON" : "OFF", isCtrlAlt ? "ON" : "OFF");

        C2D_TextParse(&statusRenderText, dynamicBuf, infoEstado);
        C2D_TextOptimize(&statusRenderText);

        // Iniciar fotograma gráfico
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        // --- PANTALLA SUPERIOR ---
        C2D_TargetClear(topScreen, C2D_Color32(250, 250, 245, 255));
        C2D_SceneBegin(topScreen);

        C2D_DrawRectSolid(0, 0, 0.4f, 400, 24, C2D_Color32(45, 55, 72, 255));
        C2D_DrawText(&headerText, C2D_WithColor, 10.0f, 4.0f, 0.5f, 0.5f, 0.5f, C2D_Color32(255, 255, 255, 255));
        C2D_DrawText(&docRenderText, C2D_WithColor | C2D_WordWrap, 10.0f, 34.0f, 0.5f, 0.52f, 0.52f, C2D_Color32(30, 30, 30, 255), 380.0f);

        // --- PANTALLA INFERIOR (Teclado Completo Multicapa) ---
        C2D_TargetClear(bottomScreen, C2D_Color32(225, 230, 238, 255));
        C2D_SceneBegin(bottomScreen);

        for (const auto& key : keys) {
            u32 colorFondo = C2D_Color32(255, 255, 255, 255);
            u32 colorTexto = C2D_Color32(30, 30, 35, 255);

            if (key.type == 3) { // Guardar
                colorFondo = C2D_Color32(65, 130, 190, 255);
                colorTexto = C2D_Color32(255, 255, 255, 255);
            }

            // Dibujar fondo de tecla
            C2D_DrawRectSolid(key.x, key.y, 0.3f, key.w, key.h, colorFondo);

            // Determinar carácter principal
            std::string mainChar = key.base;
            if (isCtrlAlt && !key.ctrlAlt.empty()) mainChar = key.ctrlAlt;
            else if (isShift) mainChar = key.shift;

            if (key.type > 0) mainChar = key.label;

            // Dibujar carácter principal centrado
            C2D_Text keyLabel;
            C2D_TextParse(&keyLabel, dynamicBuf, mainChar.c_str());
            C2D_TextOptimize(&keyLabel);
            C2D_DrawText(&keyLabel, C2D_WithColor, key.x + (key.w / 2.0f) - 3.0f, key.y + 3.0f, 0.5f, 0.35f, 0.35f, colorTexto);

            // Mostrar variante Shift en la esquina superior izquierda si aplica
            if (key.type == 0 && !isShift && !isCtrlAlt) {
                if (!key.shift.empty() && key.shift != key.base) {
                    C2D_Text shiftLbl;
                    C2D_TextParse(&shiftLbl, dynamicBuf, key.shift.c_str());
                    C2D_TextOptimize(&shiftLbl);
                    C2D_DrawText(&shiftLbl, C2D_WithColor, key.x + 2.0f, key.y + 1.0f, 0.5f, 0.20f, 0.20f, C2D_Color32(120, 120, 130, 200));
                }
            }
        }

        // Barra de estado inferior táctil
        C2D_DrawRectSolid(0, 215, 0.4f, 320, 25, C2D_Color32(50, 60, 70, 255));
        C2D_DrawText(&statusRenderText, C2D_WithColor, 10.0f, 219.0f, 0.5f, 0.42f, 0.42f, C2D_Color32(255, 255, 255, 255));

        C3D_FrameEnd(0);
    }

    if (dynamicBuf) C2D_TextBufDelete(dynamicBuf);
    if (staticBuf) C2D_TextBufDelete(staticBuf);

    C2D_Fini();
    C3D_Fini();
    gfxExit();

    return 0;
}
