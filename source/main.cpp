#include <3ds.h>
#include <citro2d.h>
#include <string>
#include <vector>
#include <cstdio>
#include <sstream>

// Estructura para representar cada tecla con sus capas (Base, Shift/L, CtrlAlt/ZL) y ayudas visuales
struct KeyButton {
    float x, y, w, h;
    std::string base;
    std::string shift;
    std::string ctrlAlt;
    int type; // 0 = texto, 2 = espacio, 3 = guardar, 4 = tecla muerta (tilde)
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

// Función para procesar vocales con tilde o diéresis (Teclas muertas)
std::string aplicarTeclaMuerta(char vocal, int tipoMuerta) {
    bool esMayuscula = (vocal >= 'A' && vocal <= 'Z');
    char vLower = esMayuscula ? (vocal + 32) : vocal;
    
    if (tipoMuerta == 1) { // Tilde aguda (´)
        if (vLower == 'a') return esMayuscula ? "Á" : "á";
        if (vLower == 'e') return esMayuscula ? "É" : "é";
        if (vLower == 'i') return esMayuscula ? "Í" : "í";
        if (vLower == 'o') return esMayuscula ? "Ó" : "ó";
        if (vLower == 'u') return esMayuscula ? "Ú" : "ú";
    } else if (tipoMuerta == 2) { // Cremillas / Diéresis (¨)
        if (vLower == 'a') return esMayuscula ? "Ä" : "ä";
        if (vLower == 'e') return esMayuscula ? "Ë" : "ë";
        if (vLower == 'i') return esMayuscula ? "Ï" : "ï";
        if (vLower == 'o') return esMayuscula ? "Ö" : "ö";
        if (vLower == 'u') return esMayuscula ? "Ü" : "ü";
    } else if (tipoMuerta == 3) { // Tilde abierta / grave (`)
        if (vLower == 'a') return esMayuscula ? "À" : "à";
        if (vLower == 'e') return esMayuscula ? "È" : "è";
        if (vLower == 'i') return esMayuscula ? "Ì" : "ì";
        if (vLower == 'o') return esMayuscula ? "Ò" : "ò";
        if (vLower == 'u') return esMayuscula ? "Ù" : "ù";
    }
    
    std::string res = (tipoMuerta == 1 ? "´" : (tipoMuerta == 2 ? "¨" : "`"));
    res += vocal;
    return res;
}

int main(int argc, char* argv[]) {
    // 1. Inicializar subsistemas de la 3DS
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget* topScreen = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottomScreen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    C2D_TextBuf staticBuf = C2D_TextBufNew(4096);
    C2D_TextBuf dynamicBuf = C2D_TextBufNew(16384);

    C2D_Text headerText;
    C2D_TextParse(&headerText, staticBuf, "Laura - Procesador de Texto");
    C2D_TextOptimize(&headerText);

    // 2. Construcción del teclado ergonómico (Pantalla inferior 320x240)
    std::vector<KeyButton> keys;
    float btnW = 29.5f; 
    float btnH = 24.0f; 
    float gap = 2.0f;

    // Fila 1: Numérica exacta (1/|/[no], 2/*/@, 3/·/#, 4/$/ª, 5/%/+, 6/&/¬, 7//\, 8/(, 9/), 0/=/º)
    std::string r1_base  = "1234567890";
    std::string r1_shift = "*·$%&/()="; // Nota: '7' usa índice especial para '/'
    std::string r1_ctrl  = "|@#%+¬\\   "; // Nota: '1' usa '|', '7' usa '\\'
    float f1_x = 8.0f;
    float f1_y = 12.0f;
    for (int i = 0; i < 10; i++) {
        std::string b(1, r1_base[i]);
        std::string s = (i == 6) ? "/" : std::string(1, r1_shift[i]);
        std::string c = "";
        if (i == 0) c = "|";
        else if (i == 6) c = "\\";
        else if (i < r1_ctrl.length() && r1_ctrl[i] != ' ') c = std::string(1, r1_ctrl[i]);
        
        keys.push_back({f1_x + i * (btnW + gap), f1_y, btnW, btnH, b, s, c, 0});
    }

    // Fila 2: QWERTY (con e/E/€)
    std::string r2_base  = "qwertyuiop";
    std::string r2_shift = "QWERTYUIOP";
    float f2_x = 8.0f;
    float f2_y = f1_y + btnH + 3.0f;
    for (int i = 0; i < 10; i++) {
        std::string b(1, r2_base[i]);
        std::string s(1, r2_shift[i]);
        std::string c = (r2_base[i] == 'e') ? "€" : "";
        keys.push_back({f2_x + i * (btnW + gap), f2_y, btnW, btnH, b, s, c, 0});
    }

    // Fila 3: ASDF (con ñ/Ñ/~)
    std::string r3_base[10]  = {"a", "s", "d", "f", "g", "h", "j", "k", "l", "ñ"};
    std::string r3_shift[10] = {"A", "S", "D", "F", "G", "H", "J", "K", "L", "Ñ"};
    std::string r3_ctrl[10]  = {"",  "",  "",  "",  "",  "",  "",  "",  "",  "~"};
    float f3_x = 8.0f;
    float f3_y = f2_y + btnH + 3.0f;
    for (int i = 0; i < 10; i++) {
        keys.push_back({f3_x + i * (btnW + gap), f3_y, btnW, btnH, r3_base[i], r3_shift[i], r3_ctrl[i], 0});
    }

    // Fila 4: ZXCV (z/Z/<, x/X/>, c/C/ç, v, b, n/N/{, m/M/}, ,/; , ./:/… , ´/¨/`)
    std::string r4_base[10]  = {"z", "x", "c", "v", "b", "n", "m", ",", ".", "´"};
    std::string r4_shift[10] = {"Z", "X", "C", "V", "B", "N", "M", ";", ":", "¨"};
    std::string r4_ctrl[10]  = {"<", ">", "ç", "",  "",  "{", "}", "",  "",  "`"};
    float f4_x = 8.0f;
    float f4_y = f3_y + btnH + 3.0f;
    for (int i = 0; i < 9; i++) {
        keys.push_back({f4_x + i * (btnW + gap), f4_y, btnW, btnH, r4_base[i], r4_shift[i], r4_ctrl[i], 0});
    }
    // Décima tecla de la fila 4 es la Tilde (Tecla Muerta)
    keys.push_back({f4_x + 9 * (btnW + gap), f4_y, btnW, btnH, r4_base[9], r4_shift[9], r4_ctrl[9], 4});

    // Fila 5: Puntuación exacta («, ¿, ¡, -/–, ‘/“, ?, !, »)
    std::string puncs_b[8] = {"«", "¿", "¡", "-", "‘", "?", "!", "»"};
    std::string puncs_s[8] = {"«", "¿", "¡", "–", "“", "?", "!", "»"};
    float f5_x = 8.0f;
    float f5_y = f4_y + btnH + 3.0f;
    for (int i = 0; i < 8; i++) {
        keys.push_back({f5_x + i * (btnW + gap), f5_y, btnW, btnH, puncs_b[i], puncs_s[i], "", 0});
    }

    // Fila 6: Barra Espaciadora amplia y Botón Guardar
    float f6_y = f5_y + btnH + 3.0f;
    keys.push_back({8.0f, f6_y, 185.0f, 25.0f, "ESPACIO", "TAB", " ", 2});
    keys.push_back({198.0f, f6_y, 114.0f, 25.0f, "GUARDAR", "GUARDAR", "GUARDAR", 3});

    std::string documento = "";
    std::string estadoMensaje = "Listo para escribir.";
    int estadoTeclaMuerta = 0; // 0 = sin tilde, 1 = tilde aguda (´), 2 = dieresis (¨), 3 = tilde abierta (`)

    // 3. Bucle principal de la aplicación
    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        if (kDown & KEY_START) break;

        // Nuevos modificadores físicos asignados:
        bool isShift = (kHeld & KEY_L);
        bool isCtrlAlt = (kHeld & KEY_ZL);
        bool isEnter = (kDown & KEY_R);
        bool isBackspace = (kDown & KEY_ZR) || (kHeld & KEY_ZR);

        // Funciones físicas de los gatillos R y ZR
        if (isBackspace) {
            if (!documento.empty()) {
                documento.pop_back();
                estadoMensaje = "Borrado con ZR.";
            }
        }

        if (isEnter) {
            if (isShift) {
                documento += "\n\n"; // L + R = Salto de párrafo
                estadoMensaje = "Salto de parrafo.";
            } else if (isCtrlAlt) {
                documento += "\n--- [PAGINA] ---\n"; // ZL + R = Salto de página
                estadoMensaje = "Salto de pagina.";
            } else {
                documento += "\n"; // R = Enter normal
                estadoMensaje = "Enter.";
            }
        }

        touchPosition touch;
        hidTouchRead(&touch);

        if (kDown & KEY_TOUCH) {
            for (const auto& key : keys) {
                if (touch.px >= key.x && touch.px <= (key.x + key.w) &&
                    touch.py >= key.y && touch.py <= (key.y + key.h)) {
                    
                    if (key.type == 4) { // Tecla muerta
                        if (isCtrlAlt) estadoTeclaMuerta = 3; // `
                        else if (isShift) estadoTeclaMuerta = 2; // ¨
                        else estadoTeclaMuerta = 1; // ´
                        estadoMensaje = "Tecla muerta activa.";
                    }
                    else if (key.type == 0) { // Carácter normal
                        std::string charToAdd = key.base;
                        if (isCtrlAlt && !key.ctrlAlt.empty()) charToAdd = key.ctrlAlt;
                        else if (isShift) charToAdd = key.shift;

                        // Regla especial para Ç mayúscula: L + ZL + c
                        if (isShift && isCtrlAlt && key.base == "c") {
                            charToAdd = "Ç";
                        }

                        // Si hay tecla muerta activa y se presiona una vocal
                        if (estadoTeclaMuerta > 0 && charToAdd.length() == 1 && 
                            ((charToAdd[0] >= 'a' && charToAdd[0] <= 'z') || (charToAdd[0] >= 'A' && charToAdd[0] <= 'Z'))) {
                            documento += aplicarTeclaMuerta(charToAdd[0], estadoTeclaMuerta);
                            estadoTeclaMuerta = 0;
                        } else {
                            if (estadoTeclaMuerta > 0) {
                                documento += (estadoTeclaMuerta == 1 ? "´" : (estadoTeclaMuerta == 2 ? "¨" : "`"));
                                estadoTeclaMuerta = 0;
                            }
                            documento += charToAdd;
                        }
                        estadoMensaje = "Agregado: " + charToAdd;
                    } 
                    else if (key.type == 2) { // Espacio / Tab
                        if (estadoTeclaMuerta > 0) {
                            documento += (estadoTeclaMuerta == 1 ? "´" : (estadoTeclaMuerta == 2 ? "¨" : "`"));
                            estadoTeclaMuerta = 0;
                        }
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

        // Limpiar búfer dinámico
        C2D_TextBufClear(dynamicBuf);

        C2D_Text docRenderText, statusRenderText;
        bool cursorVisible = (osGetTime() / 400) % 2 == 0;
        std::string textoConCursor = documento + (cursorVisible ? "|" : " ");
        
        C2D_TextParse(&docRenderText, dynamicBuf, textoConCursor.c_str());
        C2D_TextOptimize(&docRenderText);

        int palabras = contarPalabras(documento);
        int caracteres = documento.length();
        char infoEstado[128];
        snprintf(infoEstado, sizeof(infoEstado), "Palabras: %d | Caracteres: %d | L:%s ZL:%s", 
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

        // --- PANTALLA INFERIOR (Teclado Multicapa Visual con L y ZL) ---
        C2D_TargetClear(bottomScreen, C2D_Color32(220, 225, 235, 255));
        C2D_SceneBegin(bottomScreen);

        for (const auto& key : keys) {
            u32 colorFondo = C2D_Color32(255, 255, 255, 255);
            u32 colorTexto = C2D_Color32(30, 30, 35, 255);

            if (key.type == 3) { // Guardar
                colorFondo = C2D_Color32(65, 130, 190, 255);
                colorTexto = C2D_Color32(255, 255, 255, 255);
            } else if (key.type == 4 && estadoTeclaMuerta > 0) {
                colorFondo = C2D_Color32(230, 180, 100, 255);
            }

            C2D_DrawRectSolid(key
