#include <3ds.h>
#include <citro2d.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sstream>
#include <algorithm>

struct KeyButton {
    float x, y, w, h;
    std::string base;
    std::string shift;
    std::string ctrlAlt;
    int type; // 0 = texto, 2 = espacio, 3 = guardar, 4 = tecla muerta, 5 = formato (N/K/S)
};

// Utilidades UTF-8 para mover el cursor y borrar sin partir tildes, ñ, €, etc.
size_t utf8PrevChar(const std::string& str, size_t pos) {
    if (pos == 0) return 0;
    pos--;
    while (pos > 0 && (static_cast<unsigned char>(str[pos]) & 0xC0) == 0x80) {
        pos--;
    }
    return pos;
}

size_t utf8NextChar(const std::string& str, size_t pos) {
    if (pos >= str.size()) return str.size();
    pos++;
    while (pos < str.size() && (static_cast<unsigned char>(str[pos]) & 0xC0) == 0x80) {
        pos++;
    }
    return pos;
}

size_t utf8Length(const std::string& str) {
    size_t count = 0;
    for (size_t i = 0; i < str.size(); ) {
        i = utf8NextChar(str, i);
        count++;
    }
    return count;
}

bool isLetterKey(const std::string& base) {
    if (base == "ñ") return true;
    return (base.size() == 1 && base[0] >= 'a' && base[0] <= 'z');
}

// Guardar en SD y registrar hora
bool guardarEnSD(const std::string& contenido, std::string& ultimaHoraGuardado) {
    FILE* archivo = fopen("sdmc:/laura_documento.txt", "w");
    if (!archivo) return false;
    fwrite(contenido.c_str(), 1, contenido.size(), archivo);
    fclose(archivo);

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
    ultimaHoraGuardado = buf;
    return true;
}

int contarPalabras(const std::string& texto) {
    std::stringstream ss(texto);
    std::string palabra;
    int contador = 0;
    while (ss >> palabra) contador++;
    return contador;
}

std::string aplicarTeclaMuerta(char vocal, int tipoMuerta) {
    bool esMayuscula = (vocal >= 'A' && vocal <= 'Z');
    char vLower = esMayuscula ? (vocal + 32) : vocal;
    
    if (tipoMuerta == 1) { // Tilde aguda (´)
        if (vLower == 'a') return esMayuscula ? "Á" : "á";
        if (vLower == 'e') return esMayuscula ? "É" : "é";
        if (vLower == 'i') return esMayuscula ? "Í" : "í";
        if (vLower == 'o') return esMayuscula ? "Ó" : "ó";
        if (vLower == 'u') return esMayuscula ? "Ú" : "ú";
    } else if (tipoMuerta == 2) { // Diéresis (¨)
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

// Historial para Undo / Redo
struct EditorState {
    std::string text;
    size_t cursor;
};

int main(int argc, char* argv[]) {
    gfxInitDefault();
    ptmuInit(); // Para leer nivel de batería
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget* topScreen = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottomScreen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    C2D_TextBuf staticBuf = C2D_TextBufNew(4096);
    C2D_TextBuf dynamicBuf = C2D_TextBufNew(32768);

    // Construcción del teclado ergonómico
    std::vector<KeyButton> keys;
    float btnW = 29.5f; 
    float btnH = 22.0f; 
    float gap = 2.0f;

    // Fila superior de Formato en español: N (Negrita), K (Cursiva), S (Subrayado / Tachado)
    float fmt_y = 6.0f;
    float fmt_w = 98.0f;
    keys.push_back({8.0f, fmt_y, fmt_w, 18.0f, "N", "N", "N", 5});
    keys.push_back({111.0f, fmt_y, fmt_w, 18.0f, "K", "K", "K", 5});
    keys.push_back({214.0f, fmt_y, fmt_w, 18.0f, "S", "S", "S", 5});

    // Fila 1: Numérica
    std::string r1_base[10]  = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
    std::string r1_shift[10] = {"|", "*", "·", "$", "%", "&", "/", "(", ")", "="};
    std::string r1_ctrl[10]  = {"",  "@", "#", "ª", "+", "¬", "\\", "[", "]", "º"};
    float f1_x = 8.0f;
    float f1_y = 28.0f;
    for (int i = 0; i < 10; i++) {
        keys.push_back({f1_x + i * (btnW + gap), f1_y, btnW, btnH, r1_base[i], r1_shift[i], r1_ctrl[i], 0});
    }

    // Fila 2: QWERTY
    std::string r2_base[10]  = {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"};
    std::string r2_shift[10] = {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"};
    std::string r2_ctrl[10]  = {"",  "",  "€", "",  "",  "",  "",  "",  "",  ""};
    float f2_x = 8.0f;
    float f2_y = f1_y + btnH + 3.0f;
    for (int i = 0; i < 10; i++) {
        keys.push_back({f2_x + i * (btnW + gap), f2_y, btnW, btnH, r2_base[i], r2_shift[i], r2_ctrl[i], 0});
    }

    // Fila 3: ASDF
    std::string r3_base[10]  = {"a", "s", "d", "f", "g", "h", "j", "k", "l", "ñ"};
    std::string r3_shift[10] = {"A", "S", "D", "F", "G", "H", "J", "K", "L", "Ñ"};
    std::string r3_ctrl[10]  = {"",  "",  "",  "",  "",  "",  "",  "",  "",  "~"};
    float f3_x = 8.0f;
    float f3_y = f2_y + btnH + 3.0f;
    for (int i = 0; i < 10; i++) {
        keys.push_back({f3_x + i * (btnW + gap), f3_y, btnW, btnH, r3_base[i], r3_shift[i], r3_ctrl[i], 0});
    }

    // Fila 4: ZXCV
    std::string r4_base[10]  = {"z", "x", "c", "v", "b", "n", "m", ",", ".", "´"};
    std::string r4_shift[10] = {"Z", "X", "C", "V", "B", "N", "M", ";", ":", "¨"};
    std::string r4_ctrl[10]  = {"<", ">", "ç", "",  "",  "{", "}", "",  "…", "`"};
    float f4_x = 8.0f;
    float f4_y = f3_y + btnH + 3.0f;
    for (int i = 0; i < 9; i++) {
        keys.push_back({f4_x + i * (btnW + gap), f4_y, btnW, btnH, r4_base[i], r4_shift[i], r4_ctrl[i], 0});
    }
    keys.push_back({f4_x + 9 * (btnW + gap), f4_y, btnW, btnH, r4_base[9], r4_shift[9], r4_ctrl[9], 4});

    // Fila 5: Puntuación
    std::string puncs_b[8] = {"«", "¿", "¡", "-", "‘", "?", "!", "»"};
    std::string puncs_s[8] = {"«", "¿", "¡", "–", "“", "?", "!", "»"};
    float f5_x = 8.0f;
    float f5_y = f4_y + btnH + 3.0f;
    for (int i = 0; i < 8; i++) {
        keys.push_back({f5_x + i * (btnW + gap), f5_y, btnW, btnH, puncs_b[i], puncs_s[i], "", 0});
    }

    // Fila 6: Barra Espaciadora y Guardar
    float f6_y = f5_y + btnH + 3.0f;
    keys.push_back({8.0f, f6_y, 185.0f, 22.0f, "ESPACIO / TAB", "ESPACIO / TAB", " ", 2});
    keys.push_back({198.0f, f6_y, 114.0f, 22.0f, "GUARDAR", "GUARDAR", "GUARDAR", 3});

    // Estado del documento y edición
    std::string documento = "";
    size_t cursorPos = 0;
    bool hasSelection = false;
    size_t selAnchor = 0;
    std::string portapapeles = "";
    std::string ultimaHoraGuardado = "--:--";

    std::vector<EditorState> undoStack;
    std::vector<EditorState> redoStack;

    auto pushUndo = [&]() {
        if (!undoStack.empty() && undoStack.back().text == documento) return;
        undoStack.push_back({documento, cursorPos});
        if (undoStack.size() > 300) undoStack.erase(undoStack.begin());
        redoStack.clear();
    };

    auto deleteSelectionIfAny = [&]() -> bool {
        if (!hasSelection || selAnchor == cursorPos) {
            hasSelection = false;
            return false;
        }
        size_t start = std::min(selAnchor, cursorPos);
        size_t end = std::max(selAnchor, cursorPos);
        documento.erase(start, end - start);
        cursorPos = start;
        hasSelection = false;
        return true;
    };

    int estadoTeclaMuerta = 0;
    bool capsLock = false;
    u64 lastLPressTick = 0;

    // Estados de formato (N = Negrita, K = Cursiva, S = Subrayado, Tachado)
    bool isBold = false;
    bool isItalic = false;
    bool isUnderline = false;
    bool isStrikethrough = false;

    // Toggle Info (Botón X)
    bool showTopInfoBar = true;

    // Temporizadores de repetición estándar (más ágiles)
    const int INITIAL_DELAY = 15;
    const int REPEAT_RATE = 4;

    int bsCooldown = 0;
    int touchCooldown = 0;
    int activeTouchKeyIdx = -1;
    int dpadCooldown = 0;
    int undoCooldown = 0;

    // Funciones de movimiento de cursor
    auto moveCursorLeft = [&](bool selecting) {
        if (selecting && !hasSelection) { hasSelection = true; selAnchor = cursorPos; }
        if (!selecting) hasSelection = false;
        cursorPos = utf8PrevChar(documento, cursorPos);
    };

    auto moveCursorRight = [&](bool selecting) {
        if (selecting && !hasSelection) { hasSelection = true; selAnchor = cursorPos; }
        if (!selecting) hasSelection = false;
        cursorPos = utf8NextChar(documento, cursorPos);
    };

    auto moveCursorUp = [&](bool selecting) {
        if (selecting && !hasSelection) { hasSelection = true; selAnchor = cursorPos; }
        if (!selecting) hasSelection = false;
        size_t lineStart = documento.rfind('\n', cursorPos == 0 ? 0 : cursorPos - 1);
        if (lineStart == std::string::npos) {
            cursorPos = 0;
            return;
        }
        size_t colBytes = cursorPos - (lineStart + 1);
        size_t prevLineStart = (lineStart == 0) ? std::string::npos : documento.rfind('\n', lineStart - 1);
        size_t targetStart = (prevLineStart == std::string::npos) ? 0 : prevLineStart + 1;
        size_t prevLineLen = lineStart - targetStart;
        cursorPos = targetStart + std::min(colBytes, prevLineLen);
        while (cursorPos > targetStart && (static_cast<unsigned char>(documento[cursorPos]) & 0xC0) == 0x80) cursorPos--;
    };

    auto moveCursorDown = [&](bool selecting) {
        if (selecting && !hasSelection) { hasSelection = true; selAnchor = cursorPos; }
        if (!selecting) hasSelection = false;
        size_t lineStart = (cursorPos == 0) ? std::string::npos : documento.rfind('\n', cursorPos - 1);
        size_t actualStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
        size_t colBytes = cursorPos - actualStart;
        size_t nextNewline = documento.find('\n', cursorPos);
        if (nextNewline == std::string::npos) {
            cursorPos = documento.size();
            return;
        }
        size_t nextLineStart = nextNewline + 1;
        size_t nextLineEnd = documento.find('\n', nextLineStart);
        if (nextLineEnd == std::string::npos) nextLineEnd = documento.size();
        size_t nextLineLen = nextLineEnd - nextLineStart;
        cursorPos = nextLineStart + std::min(colBytes, nextLineLen);
        while (cursorPos > nextLineStart && (static_cast<unsigned char>(documento[cursorPos]) & 0xC0) == 0x80) cursorPos--;
    };

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        if (kDown & KEY_START) break;

        // Toggle Info con Botón X
        if (kDown & KEY_X) {
            showTopInfoBar = !showTopInfoBar;
        }

        // Doble clic rápido en L para CAPS LOCK (solo letras)
        if (kDown & KEY_L) {
            u64 currentTick = osGetTime();
            if (currentTick - lastLPressTick < 280) {
                capsLock = !capsLock;
                lastLPressTick = 0;
            } else {
                lastLPressTick = currentTick;
            }
        }

        bool isLHeld = (kHeld & KEY_L);
        bool isZLHeld = (kHeld & KEY_ZL);
        bool isZRHeld = (kHeld & KEY_ZR);
        bool isSelectHeld = (kHeld & KEY_SELECT);

        if (kDown & KEY_SELECT) {
            if (!hasSelection) {
                hasSelection = true;
                selAnchor = cursorPos;
            }
        }

        // Navegación con Cruceta (D-Pad)
        u32 dpadMask = KEY_DLEFT | KEY_DRIGHT | KEY_DUP | KEY_DDOWN;
        bool triggerDpad = false;
        if (kDown & dpadMask) {
            triggerDpad = true;
            dpadCooldown = INITIAL_DELAY;
        } else if (kHeld & dpadMask) {
            if (dpadCooldown > 0) dpadCooldown--;
            else {
                triggerDpad = true;
                dpadCooldown = REPEAT_RATE;
            }
        } else {
            dpadCooldown = 0;
        }

        if (triggerDpad) {
            if (kHeld & KEY_DLEFT) moveCursorLeft(isSelectHeld);
            if (kHeld & KEY_DRIGHT) moveCursorRight(isSelectHeld);
            if (kHeld & KEY_DUP) moveCursorUp(isSelectHeld);
            if (kHeld & KEY_DDOWN) moveCursorDown(isSelectHeld);
        }

        // Undo / Redo con Botón B (con repetición al dejar presionado)
        bool triggerB = false;
        if (kDown & KEY_B) {
            triggerB = true;
            undoCooldown = INITIAL_DELAY;
        } else if (kHeld & KEY_B) {
            if (undoCooldown > 0) undoCooldown--;
            else {
                triggerB = true;
                undoCooldown = REPEAT_RATE;
            }
        } else {
            undoCooldown = 0;
        }

        if (triggerB) {
            hasSelection = false;
            if (isZLHeld) {
                // ZL + B = REDO
                if (!redoStack.empty()) {
                    undoStack.push_back({documento, cursorPos});
                    EditorState st = redoStack.back();
                    redoStack.pop_back();
                    documento = st.text;
                    cursorPos = std::min(st.cursor, documento.size());
                }
            } else if (isLHeld) {
                // L + B = UNDO últimos 5 caracteres
                for (int step = 0; step < 5 && !undoStack.empty(); step++) {
                    redoStack.push_back({documento, cursorPos});
                    EditorState st = undoStack.back();
                    undoStack.pop_back();
                    documento = st.text;
                    cursorPos = std::min(st.cursor, documento.size());
                }
            } else {
                // B = UNDO 1 carácter
                if (!undoStack.empty()) {
                    redoStack.push_back({documento, cursorPos});
                    EditorState st = undoStack.back();
                    undoStack.pop_back();
                    documento = st.text;
                    cursorPos = std::min(st.cursor, documento.size());
                }
            }
        }

        // Copiar (Y) / Cortar (L + Y)
        if (kDown & KEY_Y) {
            if (hasSelection && selAnchor != cursorPos) {
                size_t start = std::min(selAnchor, cursorPos);
                size_t end = std::max(selAnchor, cursorPos);
                portapapeles = documento.substr(start, end - start);
                if (isLHeld) { // Cortar
                    pushUndo();
                    documento.erase(start, end - start);
                    cursorPos = start;
                    hasSelection = false;
                }
            }
        }

        // Pegar (A) / Pegar sin formato (ZL + A)
        if (kDown & KEY_A) {
            if (!portapapeles.empty()) {
                pushUndo();
                deleteSelectionIfAny();
                std::string aPegar = portapapeles;
                if (isZLHeld) {
                    // Limpiar marcadores de página si los hubiera para pegar texto plano puro
                    size_t p;
                    while ((p = aPegar.find("--- [PAGINA NUEVA] ---")) != std::string::npos) {
                        aPegar.erase(p, 22);
                    }
                }
                documento.insert(cursorPos, aPegar);
                cursorPos += aPegar.size();
            }
        }

        // Backspace con ZR (velocidad calibrada e idéntica a repetición de teclas)
        bool isBackspaceTrigger = false;
        if (kDown & KEY_ZR) {
            isBackspaceTrigger = true;
            bsCooldown = INITIAL_DELAY;
        } else if (kHeld & KEY_ZR) {
            if (bsCooldown > 0) bsCooldown--;
            else {
                isBackspaceTrigger = true;
                bsCooldown = REPEAT_RATE;
            }
        } else {
            bsCooldown = 0;
        }

        if (isBackspaceTrigger) {
            if (hasSelection && selAnchor != cursorPos) {
                pushUndo();
                deleteSelectionIfAny();
            } else if (cursorPos > 0) {
                pushUndo();
                if (isLHeld) {
                    // Borrar palabra hacia atrás
                    while (cursorPos > 0 && (documento[cursorPos - 1] == ' ' || documento[cursorPos - 1] == '\n')) {
                        size_t prev = utf8PrevChar(documento, cursorPos);
                        documento.erase(prev, cursorPos - prev);
                        cursorPos = prev;
                    }
                    while (cursorPos > 0 && documento[cursorPos - 1] != ' ' && documento[cursorPos - 1] != '\n') {
                        size_t prev = utf8PrevChar(documento, cursorPos);
                        documento.erase(prev, cursorPos - prev);
                        cursorPos = prev;
                    }
                } else if (isZLHeld) {
                    // Borrar línea hacia atrás
                    size_t lineStart = documento.rfind('\n', cursorPos - 1);
                    size_t target = (lineStart == std::string::npos) ? 0 : lineStart;
                    if (target == cursorPos && cursorPos > 0) target--;
                    documento.erase(target, cursorPos - target);
                    cursorPos = target;
                } else {
                    // Borrar 1 carácter UTF-8
                    size_t prev = utf8PrevChar(documento, cursorPos);
                    documento.erase(prev, cursorPos - prev);
                    cursorPos = prev;
                }
            }
        }

        // Enter (R) y Salto de Página (ZL + R)
        if (kDown & KEY_R) {
            pushUndo();
            deleteSelectionIfAny();
            std::string ins = isZLHeld ? "\n--- [PAGINA NUEVA] ---\n" : "\n";
            documento.insert(cursorPos, ins);
            cursorPos += ins.size();
        }

        // Procesamiento táctil con repetición estándar al dejar presionado
        touchPosition touch;
        hidTouchRead(&touch);

        int touchedIdx = -1;
        if (kHeld & KEY_TOUCH) {
            for (int i = 0; i < (int)keys.size(); i++) {
                if (touch.px >= keys[i].x && touch.px <= (keys[i].x + keys[i].w) &&
                    touch.py >= keys[i].y && touch.py <= (keys[i].y + keys[i].h)) {
                    touchedIdx = i;
                    break;
                }
            }
        }

        bool triggerTouchKey = false;
        if (kDown & KEY_TOUCH) {
            activeTouchKeyIdx = touchedIdx;
            triggerTouchKey = (touchedIdx != -1);
            touchCooldown = INITIAL_DELAY;
        } else if ((kHeld & KEY_TOUCH) && touchedIdx != -1 && touchedIdx == activeTouchKeyIdx) {
            // Repetición continua para teclas de texto y espacio
            int kType = keys[touchedIdx].type;
            if (kType == 0 || kType == 2) {
                if (touchCooldown > 0) touchCooldown--;
                else {
                    triggerTouchKey = true;
                    touchCooldown = REPEAT_RATE;
                }
            }
        } else {
            activeTouchKeyIdx = -1;
            touchCooldown = 0;
        }

        if (triggerTouchKey && activeTouchKeyIdx != -1) {
            const auto& key = keys[activeTouchKeyIdx];
            bool effectiveShift = isLHeld || (capsLock && isLetterKey(key.base));

            if (key.type == 5) { // Botones de formato: N, K, S (con alterno Tachado mediante L o ZR)
                if (key.base == "N") isBold = !isBold;
                else if (key.base == "K") isItalic = !isItalic;
                else if (key.base == "S") {
                    if (isStrikethrough) {
                        // Si ya estaba tachada, tocarla de nuevo la desactiva siempre
                        isStrikethrough = false;
                    } else if (isLHeld || isZRHeld) {
                        // Activar modo tachado con L o ZR
                        isStrikethrough = true;
                        isUnderline = false;
                    } else {
                        isUnderline = !isUnderline;
                    }
                }
            }
            else if (key.type == 4) { // Tecla muerta
                if (isZLHeld) estadoTeclaMuerta = 3;
                else if (isLHeld) estadoTeclaMuerta = 2;
                else estadoTeclaMuerta = 1;
            }
            else if (key.type == 0) { // Carácter normal
                pushUndo();
                deleteSelectionIfAny();

                std::string charToAdd = key.base;
                if (isLHeld && isZLHeld && key.base == "c") {
                    charToAdd = "Ç";
                } else if (isZLHeld && !key.ctrlAlt.empty()) {
                    charToAdd = key.ctrlAlt;
                } else if (effectiveShift && !key.shift.empty()) {
                    charToAdd = key.shift;
                }

                if (estadoTeclaMuerta > 0 && charToAdd.length() == 1 &&
                    ((charToAdd[0] >= 'a' && charToAdd[0] <= 'z') || (charToAdd[0] >= 'A' && charToAdd[0] <= 'Z'))) {
                    std::string acc = aplicarTeclaMuerta(charToAdd[0], estadoTeclaMuerta);
                    documento.insert(cursorPos, acc);
                    cursorPos += acc.size();
                    estadoTeclaMuerta = 0;
                } else {
                    if (estadoTeclaMuerta > 0) {
                        std::string dead = (estadoTeclaMuerta == 1 ? "´" : (estadoTeclaMuerta == 2 ? "¨" : "`"));
                        documento.insert(cursorPos, dead);
                        cursorPos += dead.size();
                        estadoTeclaMuerta = 0;
                    }
                    documento.insert(cursorPos, charToAdd);
                    cursorPos += charToAdd.size();
                }
            }
            else if (key.type == 2) { // Espacio / Tab limpio (5 espacios)
                pushUndo();
                deleteSelectionIfAny();
                if (estadoTeclaMuerta > 0) {
                    std::string dead = (estadoTeclaMuerta == 1 ? "´" : (estadoTeclaMuerta == 2 ? "¨" : "`"));
                    documento.insert(cursorPos, dead);
                    cursorPos += dead.size();
                    estadoTeclaMuerta = 0;
                }
                std::string ins = isLHeld ? "     " : " ";
                documento.insert(cursorPos, ins);
                cursorPos += ins.size();
            }
            else if (key.type == 3) { // Guardar
                guardarEnSD(documento, ultimaHoraGuardado);
            }
        }

        // Preparar textos dinámicos
        C2D_TextBufClear(dynamicBuf);

        // Construir representación visual del documento con cursor en la posición exacta y selección
        bool cursorVisible = (osGetTime() / 400) % 2 == 0;
        std::string cursorGlyph = cursorVisible ? "|" : " ";
        std::string docVisual = documento;

        if (hasSelection && selAnchor != cursorPos) {
            size_t sStart = std::min(selAnchor, cursorPos);
            size_t sEnd = std::max(selAnchor, cursorPos);
            docVisual.insert(sEnd, "]");
            docVisual.insert(sStart, "[");
        } else {
            docVisual.insert(cursorPos, cursorGlyph);
        }

        C2D_Text docRenderText;
        C2D_TextParse(&docRenderText, dynamicBuf, docVisual.c_str());
        C2D_TextOptimize(&docRenderText);

        // Barra inferior de estado (Conteo de palabras y caracteres en español)
        char infoEstado[128];
        snprintf(infoEstado, sizeof(infoEstado), "Pal: %d | Car: %zu | L:%s ZL:%s", 
                 contarPalabras(documento), 
                 utf8Length(documento),
                 isLHeld ? "ON" : (capsLock ? "BLQ" : "OFF"),
                 isZLHeld ? "ON" : "OFF");

        C2D_Text statusRenderText;
        C2D_TextParse(&statusRenderText, dynamicBuf, infoEstado);
        C2D_TextOptimize(&statusRenderText);

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        // --- PANTALLA SUPERIOR ---
        C2D_TargetClear(topScreen, C2D_Color32(250, 250, 245, 255));
        C2D_SceneBegin(topScreen);

        float textTopMargin = 10.0f;
        if (showTopInfoBar) {
            textTopMargin = 26.0f;
            C2D_DrawRectSolid(0, 0, 0.4f, 400, 20, C2D_Color32(45, 55, 72, 255));

            // Nivel de batería (0-5 en PTMU -> porcentaje aproximado)
            u8 batLevel = 5;
            PTMU_GetBatteryLevel(&batLevel);
            int batPct = (batLevel * 100) / 5;
            char batStr[16];
            snprintf(batStr, sizeof(batStr), "Bat: %d%%", batPct);

            // Título y última hora de guardado al centro
            char titleStr[64];
            snprintf(titleStr, sizeof(titleStr), "Laura (%s)", ultimaHoraGuardado.c_str());

            // Fecha y hora en formato DD/MM/YY HH:MM (24h)
            time_t now = time(NULL);
            struct tm* t = localtime(&now);
            char dateStr[32];
            snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%02d %02d:%02d",
                     t->tm_mday, t->tm_mon + 1, t->tm_year % 100, t->tm_hour, t->tm_min);

            C2D_Text batTxt, titleTxt, dateTxt;
            C2D_TextParse(&batTxt, dynamicBuf, batStr);
            C2D_TextOptimize(&batTxt);
            C2D_TextParse(&titleTxt, dynamicBuf, titleStr);
            C2D_TextOptimize(&titleTxt);
            C2D_TextParse(&dateTxt, dynamicBuf, dateStr);
            C2D_TextOptimize(&dateTxt);

            C2D_DrawText(&batTxt, C2D_WithColor, 8.0f, 3.0f, 0.5f, 0.38f, 0.38f, C2D_Color32(220, 235, 220, 255));
            C2D_DrawText(&titleTxt, C2D_WithColor, 155.0f, 3.0f, 0.5f, 0.38f, 0.38f, C2D_Color32(255, 255, 255, 255));
            C2D_DrawText(&dateTxt, C2D_WithColor, 292.0f, 3.0f, 0.5f, 0.38f, 0.38f, C2D_Color32(220, 225, 235, 255));
        }

        C2D_DrawText(&docRenderText, C2D_WithColor | C2D_WordWrap, 10.0f, textTopMargin, 0.5f, 0.52f, 0.52f, C2D_Color32(30, 30, 30, 255), 380.0f);

        // --- PANTALLA INFERIOR ---
        C2D_TargetClear(bottomScreen, C2D_Color32(220, 225, 235, 255));
        C2D_SceneBegin(bottomScreen);

        for (int i = 0; i < (int)keys.size(); i++) {
            const auto& key = keys[i];
            u32 colorFondo = C2D_Color32(255, 255, 255, 255);
            u32 colorTexto = C2D_Color32(30, 30, 35, 255);

            // Feedback visual si la tecla está siendo presionada
            if (i == activeTouchKeyIdx) {
                colorFondo = C2D_Color32(210, 225, 245, 255);
            }

            if (key.type == 3) {
                colorFondo = C2D_Color32(65, 130, 190, 255);
                colorTexto = C2D_Color32(255, 255, 255, 255);
            } else if (key.type == 5) {
                bool activo = (key.base == "N" && isBold) ||
                              (key.base == "K" && isItalic) ||
                              (key.base == "S" && (isUnderline || isStrikethrough));
                if (activo) {
                    colorFondo = C2D_Color32(90, 105, 120, 255);
                    colorTexto = C2D_Color32(255, 255, 255, 255);
                } else {
                    colorFondo = C2D_Color32(200, 205, 215, 255);
                    colorTexto = C2D_Color32(60, 65, 75, 255);
                }
            } else if (key.type == 4 && estadoTeclaMuerta > 0) {
                colorFondo = C2D_Color32(230, 180, 100, 255);
            }

            C2D_DrawRectSolid(key.x, key.y, 0.3f, key.w, key.h, colorFondo);

            std::string mainChar = key.base;
            bool effectiveShift = isLHeld || (capsLock && isLetterKey(key.base));

            if (key.type == 0 || key.type == 4) {
                if (isLHeld && isZLHeld && key.base == "c") mainChar = "Ç";
                else if (isZLHeld && !key.ctrlAlt.empty()) mainChar = key.ctrlAlt;
                else if (effectiveShift && !key.shift.empty()) mainChar = key.shift;
            }

            float textOffsetX = (key.type == 2) ? (key.w / 2.0f) - 38.0f : (key.w / 2.0f) - 4.0f;
            float textOffsetY = (key.type == 5) ? 2.0f : 4.0f;
            float scale = (key.type == 5) ? 0.42f : 0.38f;

            C2D_Text keyLabel;
            C2D_TextParse(&keyLabel, dynamicBuf, mainChar.c_str());
            C2D_TextOptimize(&keyLabel);
            C2D_DrawText(&keyLabel, C2D_WithColor, key.x + textOffsetX, key.y + textOffsetY, 0.5f, scale, scale, colorTexto);

            // Si la tecla S está en modo TACHADO, dibujar la línea horizontal tachando la letra S
            if (key.type == 5 && key.base == "S" && isStrikethrough) {
                float strikeX = key.x + (key.w / 2.0f) - 7.0f;
                float strikeY = key.y + (key.h / 2.0f);
                C2D_DrawRectSolid(strikeX, strikeY, 0.6f, 14.0f, 1.8f, colorTexto);
            }

            // Dibujar variantes secundarias en esquinas
            if (!isLHeld && !isZLHeld && (key.type == 0 || key.type == 4)) {
                if (!key.shift.empty() && key.shift != key.base) {
                    C2D_Text shiftLbl;
                    C2D_TextParse(&shiftLbl, dynamicBuf, key.shift.c_str());
                    C2D_TextOptimize(&shiftLbl);
                    C2D_DrawText(&shiftLbl, C2D_WithColor, key.x + 2.0f, key.y + 1.0f, 0.5f, 0.20f, 0.20f, C2D_Color32(110, 110, 120, 200));
                }
                if (!key.ctrlAlt.empty()) {
                    C2D_Text ctrlLbl;
                    C2D_TextParse(&ctrlLbl, dynamicBuf, key.ctrlAlt.c_str());
                    C2D_TextOptimize(&ctrlLbl);
                    C2D_DrawText(&ctrlLbl, C2D_WithColor, key.x + key.w - 10.0f, key.y + 1.0f, 0.5f, 0.20f, 0.20f, C2D_Color32(180, 80, 80, 220));
                }
            }
        }

        C2D_DrawRectSolid(0, 215, 0.4f, 320, 25, C2D_Color32(50, 60, 70, 255));
        C2D_TextParse(&statusRenderText, dynamicBuf, infoEstado);
        C2D_TextOptimize(&statusRenderText);
        C2D_DrawText(&statusRenderText, C2D_WithColor, 10.0f, 219.0f, 0.5f, 0.36f, 0.36f, C2D_Color32(255, 255, 255, 255));

        C3D_FrameEnd(0);
    }

    if (dynamicBuf) C2D_TextBufDelete(dynamicBuf);
    if (staticBuf) C2D_TextBufDelete(staticBuf);

    ptmuExit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();

    return 0;
}
