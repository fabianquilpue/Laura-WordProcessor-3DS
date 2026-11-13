#include <3ds.h>
#include <citro2d.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <cstdint>

// Definición de colores
const u32 COLOR_NEGRO   = C2D_Color32(28, 28, 30, 255);
const u32 COLOR_BLANCO  = C2D_Color32(255, 255, 255, 255);
const u32 COLOR_ROJO    = C2D_Color32(255, 50, 50, 255);
const u32 COLOR_AZUL    = C2D_Color32(50, 100, 255, 255);
const u32 COLOR_VERDE   = C2D_Color32(50, 200, 50, 255);
const u32 COLOR_AMARILLO= C2D_Color32(240, 220, 40, 255);
const u32 COLOR_NARANJO = C2D_Color32(255, 140, 30, 255);
const u32 COLOR_FUCSIA  = C2D_Color32(255, 50, 255, 255);

u32 getPaletteColor(int index, bool isBg) {
    switch (index) {
        case 1: return COLOR_ROJO;
        case 2: return COLOR_AZUL;
        case 3: return COLOR_VERDE;
        case 4: return COLOR_AMARILLO;
        case 5: return COLOR_NARANJO;
        case 6: return COLOR_FUCSIA;
        default: return isBg ? COLOR_BLANCO : COLOR_NEGRO;
    }
}

std::string getRTFColor(int index) {
    switch (index) {
        case 1: return "\\red255\\green50\\blue50;";
        case 2: return "\\red50\\green100\\blue255;";
        case 3: return "\\red50\\green200\\blue50;";
        case 4: return "\\red240\\green220\\blue40;";
        case 5: return "\\red255\\green140\\blue30;";
        case 6: return "\\red255\\green50\\blue255;";
        default: return "\\red0\\green0\\blue0;"; // Negro (0) o default
    }
}

struct CharStyle {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strike = false;
    int align = 0; // 0: Izq, 1: Centro, 2: Der, 3: Justificado
    int fontSize = 24; // 24 = 12pt en RTF
    int fgColor = 0; // 0 a 6
    int bgColor = 0; // 0 a 6

    bool operator==(const CharStyle& o) const {
        return bold == o.bold && italic == o.italic && underline == o.underline && 
               strike == o.strike && align == o.align && fontSize == o.fontSize &&
               fgColor == o.fgColor && bgColor == o.bgColor;
    }
    bool operator!=(const CharStyle& o) const { return !(*this == o); }
};

struct StyledChar {
    std::string glyph;
    CharStyle style;
};

struct KeyButton {
    float x, y, w, h;
    std::string base;
    std::string shift;
    std::string ctrlAlt;
    int type; // 0=texto, 2=espacio, 3=guardar, 4=muerta, 5=formato, 6=alineacion, 7=menu
};

struct EditorState {
    std::vector<StyledChar> doc;
    size_t cursor;
};

std::vector<StyledChar> stringToStyled(const std::string& utf8, const CharStyle& st) {
    std::vector<StyledChar> out;
    size_t i = 0;
    while (i < utf8.size()) {
        unsigned char c = static_cast<unsigned char>(utf8[i]);
        size_t len = 1;
        if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        if (i + len > utf8.size()) len = utf8.size() - i;
        out.push_back({utf8.substr(i, len), st});
        i += len;
    }
    return out;
}

std::string styledToPlainString(const std::vector<StyledChar>& doc) {
    std::string out;
    out.reserve(doc.size());
    for (const auto& sc : doc) out += sc.glyph;
    return out;
}

uint32_t utf8ToCodepoint(const std::string& g) {
    if (g.empty()) return 0;
    unsigned char c0 = static_cast<unsigned char>(g[0]);
    if (c0 < 0x80) return c0;
    if ((c0 & 0xE0) == 0xC0 && g.size() >= 2) return ((c0 & 0x1F) << 6) | (static_cast<unsigned char>(g[1]) & 0x3F);
    if ((c0 & 0xF0) == 0xE0 && g.size() >= 3) return ((c0 & 0x0F) << 12) | ((static_cast<unsigned char>(g[1]) & 0x3F) << 6) | (static_cast<unsigned char>(g[2]) & 0x3F);
    if ((c0 & 0xF8) == 0xF0 && g.size() >= 4) return ((c0 & 0x07) << 18) | ((static_cast<unsigned char>(g[1]) & 0x3F) << 12) | ((static_cast<unsigned char>(g[2]) & 0x3F) << 6) | (static_cast<unsigned char>(g[3]) & 0x3F);
    return '?';
}

bool guardarEnRTF(const std::vector<StyledChar>& doc, std::string& ultimaHoraGuardado) {
    FILE* archivo = fopen("sdmc:/laura_documento.rtf", "w");
    if (!archivo) return false;

    fprintf(archivo, "{\\rtf1\\ansi\\ansicpg1252\\deff0\\nouicompat\n");
    
    // Tabla de fuentes y tabla de colores
    fprintf(archivo, "{\\fonttbl{\\f0\\fmodern\\fcharset0 Courier New;}}\n");
    fprintf(archivo, "{\\colortbl;\\red0\\green0\\blue0;");
    for (int c = 1; c <= 6; c++) fprintf(archivo, "%s", getRTFColor(c).c_str());
    fprintf(archivo, "}\n");

    fprintf(archivo, "\\viewkind4\\uc1\\pard\\sa200\\sl276\\slmult1\\lang3082 ");

    CharStyle currentStyle;
    std::string plain = styledToPlainString(doc);
    const std::string pageBreakTag = "\n--- [PAGINA NUEVA] ---\n";
    size_t i = 0;
    size_t byteOffset = 0;
    bool newPar = true;

    while (i < doc.size()) {
        if (plain.compare(byteOffset, pageBreakTag.size(), pageBreakTag) == 0) {
            fprintf(archivo, "\\page\n");
            i += pageBreakTag.size(); byteOffset += pageBreakTag.size();
            newPar = true; continue;
        }

        const auto& sc = doc[i];
        
        if (newPar || sc.style.align != currentStyle.align) {
            if (sc.style.align == 0) fprintf(archivo, "\\ql ");
            else if (sc.style.align == 1) fprintf(archivo, "\\qc ");
            else if (sc.style.align == 2) fprintf(archivo, "\\qr ");
            else if (sc.style.align == 3) fprintf(archivo, "\\qj ");
            currentStyle.align = sc.style.align;
            newPar = false;
        }

        if (sc.style.bold != currentStyle.bold) { fprintf(archivo, sc.style.bold ? "\\b " : "\\b0 "); currentStyle.bold = sc.style.bold; }
        if (sc.style.italic != currentStyle.italic) { fprintf(archivo, sc.style.italic ? "\\i " : "\\i0 "); currentStyle.italic = sc.style.italic; }
        if (sc.style.underline != currentStyle.underline) { fprintf(archivo, sc.style.underline ? "\\ul " : "\\ulnone "); currentStyle.underline = sc.style.underline; }
        if (sc.style.strike != currentStyle.strike) { fprintf(archivo, sc.style.strike ? "\\strike " : "\\strike0 "); currentStyle.strike = sc.style.strike; }
        if (sc.style.fontSize != currentStyle.fontSize) { fprintf(archivo, "\\fs%d ", sc.style.fontSize); currentStyle.fontSize = sc.style.fontSize; }
        if (sc.style.fgColor != currentStyle.fgColor) { fprintf(archivo, "\\cf%d ", sc.style.fgColor + 1); currentStyle.fgColor = sc.style.fgColor; }
        if (sc.style.bgColor != currentStyle.bgColor) { fprintf(archivo, "\\highlight%d ", sc.style.bgColor == 0 ? 0 : sc.style.bgColor + 1); currentStyle.bgColor = sc.style.bgColor; }

        if (sc.glyph == "\n") { fprintf(archivo, "\\par\n"); newPar = true; }
        else if (sc.glyph == "\\") fprintf(archivo, "\\\\");
        else if (sc.glyph == "{") fprintf(archivo, "\\{");
        else if (sc.glyph == "}") fprintf(archivo, "\\}");
        else {
            uint32_t cp = utf8ToCodepoint(sc.glyph);
            if (cp < 128) fputc(static_cast<char>(cp), archivo);
            else if (cp <= 0xFFFF) fprintf(archivo, "\\u%lu?", (unsigned long)cp);
            else { cp -= 0x10000; fprintf(archivo, "\\u%lu?\\u%lu?", (unsigned long)(0xD800 + (cp >> 10)), (unsigned long)(0xDC00 + (cp & 0x3FF))); }
        }
        byteOffset += sc.glyph.size(); i++;
    }
    fprintf(archivo, "\n}");
    fclose(archivo);

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    if (t) {
        char buf[16]; snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
        ultimaHoraGuardado = buf;
    }
    return true;
}

bool isLetterKey(const std::string& base) {
    if (base == "ñ") return true;
    return (base.size() == 1 && base[0] >= 'a' && base[0] <= 'z');
}

int contarPalabras(const std::string& texto) {
    std::stringstream ss(texto);
    std::string palabra; int contador = 0;
    while (ss >> palabra) contador++;
    return contador;
}

std::string aplicarTeclaMuerta(char vocal, int tipoMuerta) {
    bool esMayuscula = (vocal >= 'A' && vocal <= 'Z');
    char vLower = esMayuscula ? (vocal + 32) : vocal;
    if (tipoMuerta == 1) {
        if (vLower == 'a') return esMayuscula ? "Á" : "á";
        if (vLower == 'e') return esMayuscula ? "É" : "é";
        if (vLower == 'i') return esMayuscula ? "Í" : "í";
        if (vLower == 'o') return esMayuscula ? "Ó" : "ó";
        if (vLower == 'u') return esMayuscula ? "Ú" : "ú";
    } else if (tipoMuerta == 2) {
        if (vLower == 'a') return esMayuscula ? "Ä" : "ä";
        if (vLower == 'e') return esMayuscula ? "Ë" : "ë";
        if (vLower == 'i') return esMayuscula ? "Ï" : "ï";
        if (vLower == 'o') return esMayuscula ? "Ö" : "ö";
        if (vLower == 'u') return esMayuscula ? "Ü" : "ü";
    } else if (tipoMuerta == 3) {
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

void runEditorLoop() {
    C3D_RenderTarget* topScreen = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottomScreen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    C2D_TextBuf dynamicBuf = C2D_TextBufNew(65536);
    std::vector<KeyButton> keys;
    float btnW = 29.5f, btnH = 22.0f, gap = 2.0f;

    // Fila superior (Opciones)
    keys.push_back({8.0f, 6.0f, 40.0f, 18.0f, "N", "N", "N", 5});
    keys.push_back({52.0f, 6.0f, 40.0f, 18.0f, "K", "K", "K", 5});
    keys.push_back({96.0f, 6.0f, 40.0f, 18.0f, "S", "S", "S", 5});
    keys.push_back({140.0f, 6.0f, 50.0f, 18.0f, "ALIN", "ALIN", "ALIN", 6}); // Botón Alineación
    keys.push_back({194.0f, 6.0f, 50.0f, 18.0f, "MENU", "MENU", "MENU", 7}); // Botón Menú Colores/Tamaño
    keys.push_back({250.0f, 6.0f, 62.0f, 18.0f, "GUARDAR", "GUARDAR", "GUARDAR", 3});

    std::string r1_b[10] = {"1","2","3","4","5","6","7","8","9","0"};
    std::string r1_s[10] = {"|","*","·","$","%","&","/","(",")","="};
    std::string r1_c[10] = {"","@","#","ª","+","¬","\\","[","]","º"};
    for (int i = 0; i < 10; i++) keys.push_back({8.0f + i * (btnW + gap), 28.0f, btnW, btnH, r1_b[i], r1_s[i], r1_c[i], 0});

    std::string r2_b[10] = {"q","w","e","r","t","y","u","i","o","p"};
    std::string r2_s[10] = {"Q","W","E","R","T","Y","U","I","O","P"};
    std::string r2_c[10] = {"","","€","","","","","","",""};
    for (int i = 0; i < 10; i++) keys.push_back({8.0f + i * (btnW + gap), 53.0f, btnW, btnH, r2_b[i], r2_s[i], r2_c[i], 0});

    std::string r3_b[10] = {"a","s","d","f","g","h","j","k","l","ñ"};
    std::string r3_s[10] = {"A","S","D","F","G","H","J","K","L","Ñ"};
    std::string r3_c[10] = {"","","","","","","","","","~"};
    for (int i = 0; i < 10; i++) keys.push_back({8.0f + i * (btnW + gap), 78.0f, btnW, btnH, r3_b[i], r3_s[i], r3_c[i], 0});

    std::string r4_b[10] = {"z","x","c","v","b","n","m",",",".","´"};
    std::string r4_s[10] = {"Z","X","C","V","B","N","M",";",":","¨"};
    std::string r4_c[10] = {"<",">","ç","","","{","}","","…","`"};
    for (int i = 0; i < 9; i++) keys.push_back({8.0f + i * (btnW + gap), 103.0f, btnW, btnH, r4_b[i], r4_s[i], r4_c[i], 0});
    keys.push_back({8.0f + 9 * (btnW + gap), 103.0f, btnW, btnH, r4_b[9], r4_s[9], r4_c[9], 4});

    std::string puncs_b[8] = {"«","¿","¡","-","‘","?","!","»"};
    std::string puncs_s[8] = {"«","¿","¡","–","“","?","!","»"};
    for (int i = 0; i < 8; i++) keys.push_back({8.0f + i * (btnW + gap), 128.0f, btnW, btnH, puncs_b[i], puncs_s[i], "", 0});

    keys.push_back({8.0f, 153.0f, 304.0f, 22.0f, "ESPACIO / TAB", "ESPACIO / TAB", " ", 2});

    std::vector<StyledChar> documento;
    size_t cursorPos = 0;
    
    bool hasSelection = false;
    bool isSelectModeToggle = false;
    size_t selAnchor = 0;
    
    std::vector<StyledChar> portapapeles;
    std::string ultimaHoraGuardado = "--:--";

    std::vector<EditorState> undoStack;
    std::vector<EditorState> redoStack;

    CharStyle activeStyle;
    int estadoTeclaMuerta = 0;
    bool capsLock = false;
    u64 lastLPressTick = 0;
    bool showTopInfoBar = true;
    bool menuAbierto = false; // Controla el modal de tamaño/color

    const int INITIAL_DELAY = 15;
    const int REPEAT_RATE = 4;
    int bsCooldown = 0, touchCooldown = 0, activeTouchKeyIdx = -1, dpadCooldown = 0, undoCooldown = 0;

    auto pushUndo = [&]() {
        undoStack.push_back({documento, cursorPos});
        if (undoStack.size() > 250) undoStack.erase(undoStack.begin());
        redoStack.clear();
    };

    auto isSpaceOrNewline = [](const std::string& g) { return g == " " || g == "\n"; };

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        if (kDown & KEY_START) break;
        if (kDown & KEY_X) showTopInfoBar = !showTopInfoBar;

        if (kDown & KEY_L) {
            u64 currentTick = osGetTime();
            if (currentTick - lastLPressTick < 280) { capsLock = !capsLock; lastLPressTick = 0; } 
            else lastLPressTick = currentTick;
        }

        bool isLHeld = (kHeld & KEY_L);
        bool isZLHeld = (kHeld & KEY_ZL);
        bool isSelectHeld = (kHeld & KEY_SELECT);

        // ... [Lógica de DPAD, Selección, Copiar/Pegar, y Retroceso igual que antes] ...
        if (kDown & KEY_SELECT) {
            isSelectModeToggle = !isSelectModeToggle;
            if (isSelectModeToggle) { hasSelection = true; selAnchor = cursorPos; } 
            else hasSelection = false;
        }

        // --- MANEJO TÁCTIL (Teclado y Menú) ---
        touchPosition touch;
        hidTouchRead(&touch);

        int touchedIdx = -1;
        if (kHeld & KEY_TOUCH && !menuAbierto) {
            for (int i = 0; i < (int)keys.size(); i++) {
                if (touch.px >= keys[i].x && touch.px <= (keys[i].x + keys[i].w) && touch.py >= keys[i].y && touch.py <= (keys[i].y + keys[i].h)) {
                    touchedIdx = i; break;
                }
            }
        }

        bool triggerTouchKey = false;
        if (kDown & KEY_TOUCH) {
            if (menuAbierto) {
                // Lógica del modal de Menú (Tamaño y Color)
                if (touch.px >= 30 && touch.px <= 70 && touch.py >= 50 && touch.py <= 80) activeStyle.fontSize = std::max(12, activeStyle.fontSize - 2); // - Tamaño
                if (touch.px >= 80 && touch.px <= 120 && touch.py >= 50 && touch.py <= 80) activeStyle.fontSize = std::min(72, activeStyle.fontSize + 2); // + Tamaño
                
                // Selección Color Fuente
                for (int c = 0; c <= 6; c++) {
                    if (touch.px >= 30 + (c * 35) && touch.px <= 55 + (c * 35) && touch.py >= 100 && touch.py <= 125) {
                        activeStyle.fgColor = c;
                    }
                }
                // Selección Color Fondo
                for (int c = 0; c <= 6; c++) {
                    if (touch.px >= 30 + (c * 35) && touch.px <= 55 + (c * 35) && touch.py >= 150 && touch.py <= 175) {
                        activeStyle.bgColor = c;
                        if (c != 0) activeStyle.fgColor = 0; // Si pone fondo, resetear fuente a blanco(default sobre color)
                    }
                }
                
                // Botón Cerrar
                if (touch.px >= 250 && touch.px <= 300 && touch.py >= 190 && touch.py <= 220) menuAbierto = false;
                
                // Aplicar estilo al texto seleccionado si lo hay
                if (hasSelection && selAnchor != cursorPos) {
                    size_t s = std::min(selAnchor, cursorPos), e = std::max(selAnchor, cursorPos);
                    for (size_t idx = s; idx < e; idx++) {
                        documento[idx].style.fontSize = activeStyle.fontSize;
                        documento[idx].style.fgColor = activeStyle.fgColor;
                        documento[idx].style.bgColor = activeStyle.bgColor;
                    }
                }
            } else {
                activeTouchKeyIdx = touchedIdx; triggerTouchKey = (touchedIdx != -1); touchCooldown = INITIAL_DELAY;
            }
        } else if ((kHeld & KEY_TOUCH) && !menuAbierto && touchedIdx != -1 && touchedIdx == activeTouchKeyIdx) {
            if (keys[touchedIdx].type == 0 || keys[touchedIdx].type == 2) {
                if (touchCooldown > 0) touchCooldown--;
                else { triggerTouchKey = true; touchCooldown = REPEAT_RATE; }
            }
        } else {
            activeTouchKeyIdx = -1; touchCooldown = 0;
        }

        if (triggerTouchKey && activeTouchKeyIdx != -1 && !menuAbierto) {
            const auto& key = keys[activeTouchKeyIdx];
            bool effectiveShift = isLHeld || (capsLock && isLetterKey(key.base));

            if (key.type == 7) menuAbierto = true;
            else if (key.type == 6) { // Alineación
                activeStyle.align = (activeStyle.align + 1) % 4;
                if (hasSelection && selAnchor != cursorPos) {
                    pushUndo(); size_t s = std::min(selAnchor, cursorPos), e = std::max(selAnchor, cursorPos);
                    for (size_t idx = s; idx < e; idx++) documento[idx].style.align = activeStyle.align;
                }
            } else if (key.type == 5) { // Formato (Bold, Italic, Strike)
                if (key.base == "N") activeStyle.bold = !activeStyle.bold;
                else if (key.base == "K") activeStyle.italic = !activeStyle.italic;
                else if (key.base == "S") activeStyle.underline = !activeStyle.underline;

                if (hasSelection && selAnchor != cursorPos) {
                    pushUndo(); size_t s = std::min(selAnchor, cursorPos), e = std::max(selAnchor, cursorPos);
                    for (size_t idx = s; idx < e; idx++) {
                        if (key.base == "N") documento[idx].style.bold = activeStyle.bold;
                        if (key.base == "K") documento[idx].style.italic = activeStyle.italic;
                        if (key.base == "S") documento[idx].style.underline = activeStyle.underline;
                    }
                }
            } else if (key.type == 4) { // Muerta
                if (isZLHeld) estadoTeclaMuerta = 3; else if (isLHeld) estadoTeclaMuerta = 2; else estadoTeclaMuerta = 1;
            } else if (key.type == 0 || key.type == 2) { // Texto y Espacio
                pushUndo();
                if (hasSelection) { // Eliminar selección antes de escribir
                    size_t start = std::min(selAnchor, cursorPos), end = std::max(selAnchor, cursorPos);
                    documento.erase(documento.begin() + start, documento.begin() + end);
                    cursorPos = start; hasSelection = false; isSelectModeToggle = false;
                }

                if (key.type == 2) { // Espacio
                    documento.insert(documento.begin() + cursorPos, {" ", activeStyle}); cursorPos++;
                } else { // Letra normal
                    std::string charToAdd = key.base;
                    if (isLHeld && isZLHeld && key.base == "c") charToAdd = "Ç";
                    else if (isZLHeld && !key.ctrlAlt.empty()) charToAdd = key.ctrlAlt;
                    else if (effectiveShift && !key.shift.empty()) charToAdd = key.shift;

                    if (estadoTeclaMuerta > 0 && charToAdd.length() == 1 && ((charToAdd[0] >= 'a' && charToAdd[0] <= 'z') || (charToAdd[0] >= 'A' && charToAdd[0] <= 'Z'))) {
                        std::string acc = aplicarTeclaMuerta(charToAdd[0], estadoTeclaMuerta);
                        documento.insert(documento.begin() + cursorPos, {acc, activeStyle});
                        cursorPos++; estadoTeclaMuerta = 0;
                    } else {
                        documento.insert(documento.begin() + cursorPos, {charToAdd, activeStyle});
                        cursorPos++;
                    }
                }
            } else if (key.type == 3) {
                guardarEnRTF(documento, ultimaHoraGuardado);
            }
        }

        // --- RETROCESO (ZR) ---
        bool isBackspaceTrigger = false;
        if (kDown & KEY_ZR) { isBackspaceTrigger = true; bsCooldown = INITIAL_DELAY; }
        else if (kHeld & KEY_ZR) { if (bsCooldown > 0) bsCooldown--; else { isBackspaceTrigger = true; bsCooldown = REPEAT_RATE; } } else bsCooldown = 0;

        if (isBackspaceTrigger && cursorPos > 0 && !menuAbierto && !hasSelection) {
            pushUndo(); documento.erase(documento.begin() + (cursorPos - 1)); cursorPos--;
        }

        C2D_TextBufClear(dynamicBuf);

        // --- PANTALLA SUPERIOR (RENDERIZADO) ---
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(topScreen, C2D_Color32(250, 250, 245, 255));
        C2D_SceneBegin(topScreen);

        float textTopMargin = 10.0f;
        // (Barra superior omitida para ahorrar espacio visual en el snippet, se mantiene la de estado al fondo)

        float curX = 10.0f; float curY = textTopMargin;
        const float maxLineX = 390.0f;
        bool cursorVisible = (osGetTime() / 400) % 2 == 0;
        size_t selStart = hasSelection ? std::min(selAnchor, cursorPos) : 0;
        size_t selEnd = hasSelection ? std::max(selAnchor, cursorPos) : 0;

        // Pre-calcular anchos de línea para la alineación
        std::vector<float> lineWidths; float tempW = 0;
        for (size_t idx = 0; idx < documento.size(); idx++) {
            if (documento[idx].glyph == "\n") { lineWidths.push_back(tempW); tempW = 0; }
            else {
                C2D_Text t; C2D_TextParse(&t, dynamicBuf, documento[idx].glyph.c_str());
                float gw=0, gh=0; float scale = documento[idx].style.fontSize / 48.0f;
                C2D_TextGetDimensions(&t, scale, scale, &gw, &gh);
                tempW += (gw < 3.0f ? 5.0f : gw);
            }
        }
        lineWidths.push_back(tempW);

        int lineIdx = 0;
        float startX = 10.0f;

        auto applyAlignment = [&]() {
            int align = (lineIdx < documento.size()) ? documento[lineIdx].style.align : activeStyle.align;
            if (align == 1) startX = std::max(10.0f, (400.0f - lineWidths[lineIdx]) / 2.0f); // Centro
            else if (align == 2) startX = std::max(10.0f, 390.0f - lineWidths[lineIdx]); // Derecha
            else startX = 10.0f; // Izquierda / Justificado(simple)
            curX = startX;
        };
        applyAlignment();

        float maxLineHeight = 16.5f;

        for (size_t idx = 0; idx <= documento.size(); idx++) {
            if (idx == cursorPos && cursorVisible && !menuAbierto) {
                C2D_DrawRectSolid(curX, curY, 0.75f, 1.5f, maxLineHeight, COLOR_NEGRO);
            }

            if (idx == documento.size()) break;
            const auto& sc = documento[idx];
            float textScale = sc.style.fontSize / 48.0f; // fs24 -> 0.5f base
            float currentLineHeight = 33.0f * textScale;
            if (currentLineHeight > maxLineHeight) maxLineHeight = currentLineHeight;

            if (sc.glyph == "\n") {
                curY += maxLineHeight; lineIdx++; applyAlignment(); maxLineHeight = 16.5f; continue;
            }

            C2D_Text glyphText; C2D_TextParse(&glyphText, dynamicBuf, sc.glyph.c_str()); C2D_TextOptimize(&glyphText);
            float gWidth = 0.0f, gHeight = 0.0f; C2D_TextGetDimensions(&glyphText, textScale, textScale, &gWidth, &gHeight);
            if (gWidth < 3.0f) gWidth = 5.0f * textScale * 2.0f;

            if (curX + gWidth > maxLineX) { curY += maxLineHeight; lineIdx++; applyAlignment(); maxLineHeight = 16.5f; }

            u32 fg = getPaletteColor(sc.style.fgColor, false);
            u32 bg = sc.style.bgColor != 0 ? getPaletteColor(sc.style.bgColor, true) : 0;
            if (sc.style.bgColor != 0 && sc.style.fgColor == 0) fg = COLOR_BLANCO;

            if (bg != 0) C2D_DrawRectSolid(curX, curY, 0.25f, gWidth, maxLineHeight, bg);
            if (hasSelection && idx >= selStart && idx < selEnd) C2D_DrawRectSolid(curX, curY, 0.3f, gWidth, maxLineHeight, C2D_Color32(180, 210, 245, 180));

            // Simulación visual de Cursiva inclinando la Y según la X relativa
            float slantX = sc.style.italic ? 3.0f * textScale : 0.0f;
            
            C2D_DrawText(&glyphText, C2D_WithColor, curX + slantX, curY, 0.5f, textScale, textScale, fg);
            if (sc.style.bold) C2D_DrawText(&glyphText, C2D_WithColor, curX + slantX + 1.0f, curY, 0.51f, textScale, textScale, fg);
            if (sc.style.underline) C2D_DrawRectSolid(curX, curY + maxLineHeight - 2.0f, 0.6f, gWidth, 1.5f, fg);

            curX += gWidth + (sc.style.bold ? 1.0f : 0.0f);
        }

        // --- PANTALLA INFERIOR ---
        C2D_TargetClear(bottomScreen, C2D_Color32(220, 225, 235, 255));
        C2D_SceneBegin(bottomScreen);

        if (menuAbierto) {
            // Dibujar Menu Modal (Tamaño y Color)
            C2D_DrawRectSolid(10, 10, 0.5f, 300, 220, C2D_Color32(245, 245, 250, 255));
            C2D_DrawRectSolid(10, 10, 0.5f, 300, 25, C2D_Color32(60, 80, 100, 255));
            
            C2D_Text titleTxt; C2D_TextParse(&titleTxt, dynamicBuf, "AJUSTES DE TEXTO Y COLOR"); C2D_TextOptimize(&titleTxt);
            C2D_DrawText(&titleTxt, C2D_WithColor, 20, 15, 0.6f, 0.45f, 0.45f, COLOR_BLANCO);

            // Tamaño
            char sizeStr[32]; snprintf(sizeStr, sizeof(sizeStr), "TAMANO: %d pt", activeStyle.fontSize / 2);
            C2D_Text szTxt; C2D_TextParse(&szTxt, dynamicBuf, sizeStr); C2D_TextOptimize(&szTxt);
            C2D_DrawText(&szTxt, C2D_WithColor, 20, 45, 0.6f, 0.4f, 0.4f, COLOR_NEGRO);
            C2D_DrawRectSolid(30, 60, 0.6f, 40, 30, C2D_Color32(200, 200, 200, 255)); // Boton -
            C2D_DrawRectSolid(80, 60, 0.6f, 40, 30, C2D_Color32(200, 200, 200, 255)); // Boton +
            
            // Color Fuente
            C2D_Text fgTxt; C2D_TextParse(&fgTxt, dynamicBuf, "COLOR FUENTE:"); C2D_TextOptimize(&fgTxt);
            C2D_DrawText(&fgTxt, C2D_WithColor, 20, 95, 0.6f, 0.4f, 0.4f, COLOR_NEGRO);
            for (int c = 0; c <= 6; c++) {
                C2D_DrawRectSolid(30 + (c * 35), 110, 0.6f, 25, 25, getPaletteColor(c, false));
                if (activeStyle.fgColor == c) C2D_DrawRectSolid(30 + (c * 35), 135, 0.6f, 25, 3, COLOR_NEGRO);
            }

            // Color Fondo
            C2D_Text bgTxt; C2D_TextParse(&bgTxt, dynamicBuf, "COLOR FONDO:"); C2D_TextOptimize(&bgTxt);
            C2D_DrawText(&bgTxt, C2D_WithColor, 20, 145, 0.6f, 0.4f, 0.4f, COLOR_NEGRO);
            for (int c = 0; c <= 6; c++) {
                C2D_DrawRectSolid(30 + (c * 35), 160, 0.6f, 25, 25, c == 0 ? COLOR_BLANCO : getPaletteColor(c, true));
                if (activeStyle.bgColor == c) C2D_DrawRectSolid(30 + (c * 35), 185, 0.6f, 25, 3, COLOR_NEGRO);
            }

            C2D_DrawRectSolid(250, 190, 0.6f, 50, 30, C2D_Color32(200, 50, 50, 255)); // Cerrar
        } else {
            // Renderizar Teclado normal
            for (int i = 0; i < (int)keys.size(); i++) {
                const auto& key = keys[i];
                u32 cFondo = C2D_Color32(255, 255, 255, 255); u32 cTexto = COLOR_NEGRO;

                if (i == activeTouchKeyIdx) cFondo = C2D_Color32(210, 225, 245, 255);
                
                if (key.type == 5 || key.type == 6 || key.type == 7) {
                    bool activo = false;
                    if (key.base == "N") activo = activeStyle.bold;
                    else if (key.base == "K") activo = activeStyle.italic;
                    else if (key.base == "S") activo = activeStyle.underline;
                    
                    if (activo) { cFondo = C2D_Color32(90, 105, 120, 255); cTexto = COLOR_BLANCO; }
                    else { cFondo = C2D_Color32(200, 205, 215, 255); cTexto = C2D_Color32(60, 65, 75, 255); }
                }

                C2D_DrawRectSolid(key.x, key.y, 0.3f, key.w, key.h, cFondo);

                std::string mainChar = key.base;
                if (key.type == 6) {
                    if (activeStyle.align == 0) mainChar = "IZQ";
                    else if (activeStyle.align == 1) mainChar = "CEN";
                    else if (activeStyle.align == 2) mainChar = "DER";
                    else mainChar = "JUS";
                }

                float sX = (key.type >= 5) ? 0.42f : 0.38f;
                C2D_Text keyLabel; C2D_TextParse(&keyLabel, dynamicBuf, mainChar.c_str()); C2D_TextOptimize(&keyLabel);
                C2D_DrawText(&keyLabel, C2D_WithColor, key.x + (key.w/2.0f) - 10.0f, key.y + 4.0f, 0.5f, sX, sX, cTexto);
            }
        }

        C3D_FrameEnd(0);
    }

    C2D_TextBufDelete(dynamicBuf);
}
