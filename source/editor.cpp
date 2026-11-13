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
#include <cmath>

// Definición de colores
const u32 COLOR_NEGRO   = C2D_Color32(28, 28, 30, 255);
const u32 COLOR_BLANCO  = C2D_Color32(255, 255, 255, 255);
const u32 COLOR_ROJO    = C2D_Color32(220, 50, 50, 255);
const u32 COLOR_AZUL    = C2D_Color32(50, 100, 220, 255);
const u32 COLOR_VERDE   = C2D_Color32(50, 180, 50, 255);
const u32 COLOR_AMARILLO= C2D_Color32(230, 200, 40, 255);
const u32 COLOR_NARANJO = C2D_Color32(240, 120, 30, 255);
const u32 COLOR_FUCSIA  = C2D_Color32(220, 50, 220, 255);
const u32 COLOR_MUSGO   = C2D_Color32(78, 122, 101, 255);

u32 getPaletteColor(int index) {
    switch (index) {
        case 0: return COLOR_NEGRO;
        case 1: return COLOR_BLANCO;
        case 2: return COLOR_ROJO;
        case 3: return COLOR_AZUL;
        case 4: return COLOR_VERDE;
        case 5: return COLOR_AMARILLO;
        case 6: return COLOR_NARANJO;
        case 7: return COLOR_FUCSIA;
        default: return COLOR_NEGRO;
    }
}

std::string getRTFColor(int index) {
    switch (index) {
        case 0: return "\\red28\\green28\\blue30;"; // Negro
        case 1: return "\\red255\\green255\\blue255;"; // Blanco
        case 2: return "\\red220\\green50\\blue50;";
        case 3: return "\\red50\\green100\\blue220;";
        case 4: return "\\red50\\green180\\blue50;";
        case 5: return "\\red230\\green200\\blue40;";
        case 6: return "\\red240\\green120\\blue30;";
        case 7: return "\\red220\\green50\\blue220;";
        default: return "\\red0\\green0\\blue0;";
    }
}

struct CharStyle {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strike = false;
    int align = 0; // 0: Izq, 1: Centro, 2: Der, 3: Justificado
    int lineSpacing = 0; // 0: 1.0, 1: 1.2, 2: 1.5, 3: 2.0
    int fontSize = 24; 
    int fgColor = 0; // 0 a 7
    int bgColor = 1; // 0 a 7 (Default 1: Blanco)

    bool operator==(const CharStyle& o) const {
        return bold == o.bold && italic == o.italic && underline == o.underline && 
               strike == o.strike && align == o.align && lineSpacing == o.lineSpacing && 
               fontSize == o.fontSize && fgColor == o.fgColor && bgColor == o.bgColor;
    }
    bool operator!=(const CharStyle& o) const { return !(*this == o); }
};

struct StyledChar {
    std::string glyph;
    CharStyle style;
};

struct KeyButton {
    float x, y, w, h;
    std::string base, shift, ctrlAlt;
    int type; // 0=txt, 2=espacio, 4=muerta, 5=formato(NKS), 6=alin, 7=Aa, 8=interlineado, 9=pilcrow
};

struct EditorState { std::vector<StyledChar> doc; size_t cursor; };

std::vector<StyledChar> stringToStyled(const std::string& utf8, const CharStyle& st) {
    std::vector<StyledChar> out; size_t i = 0;
    while (i < utf8.size()) {
        unsigned char c = static_cast<unsigned char>(utf8[i]); size_t len = 1;
        if ((c & 0xE0) == 0xC0) len = 2; else if ((c & 0xF0) == 0xE0) len = 3; else if ((c & 0xF8) == 0xF0) len = 4;
        if (i + len > utf8.size()) len = utf8.size() - i;
        out.push_back({utf8.substr(i, len), st}); i += len;
    }
    return out;
}

uint32_t utf8ToCodepoint(const std::string& g) {
    if (g.empty()) return 0; unsigned char c0 = static_cast<unsigned char>(g[0]);
    if (c0 < 0x80) return c0;
    if ((c0 & 0xE0) == 0xC0 && g.size() >= 2) return ((c0 & 0x1F) << 6) | (static_cast<unsigned char>(g[1]) & 0x3F);
    if ((c0 & 0xF0) == 0xE0 && g.size() >= 3) return ((c0 & 0x0F) << 12) | ((static_cast<unsigned char>(g[1]) & 0x3F) << 6) | (static_cast<unsigned char>(g[2]) & 0x3F);
    if ((c0 & 0xF8) == 0xF0 && g.size() >= 4) return ((c0 & 0x07) << 18) | ((static_cast<unsigned char>(g[1]) & 0x3F) << 12) | ((static_cast<unsigned char>(g[2]) & 0x3F) << 6) | (static_cast<unsigned char>(g[3]) & 0x3F);
    return '?';
}

bool guardarEnRTF(const std::vector<StyledChar>& doc, std::string& fileName, std::string& ultimaHoraGuardado) {
    std::string path = "sdmc:/" + fileName + ".rtf";
    FILE* archivo = fopen(path.c_str(), "w");
    if (!archivo) return false;

    fprintf(archivo, "{\\rtf1\\ansi\\ansicpg1252\\deff0\\nouicompat\n");
    fprintf(archivo, "{\\fonttbl{\\f0\\fmodern\\fcharset0 Courier New;}}\n");
    fprintf(archivo, "{\\colortbl;");
    for (int c = 0; c <= 7; c++) fprintf(archivo, "%s", getRTFColor(c).c_str());
    fprintf(archivo, "}\n\\viewkind4\\uc1\\pard\\lang3082 ");

    CharStyle currentStyle;
    bool newPar = true;

    for (size_t i = 0; i < doc.size(); i++) {
        const auto& sc = doc[i];
        
        if (newPar || sc.style.align != currentStyle.align || sc.style.lineSpacing != currentStyle.lineSpacing) {
            if (sc.style.align == 0) fprintf(archivo, "\\ql "); else if (sc.style.align == 1) fprintf(archivo, "\\qc ");
            else if (sc.style.align == 2) fprintf(archivo, "\\qr "); else if (sc.style.align == 3) fprintf(archivo, "\\qj ");
            
            if (sc.style.lineSpacing == 0) fprintf(archivo, "\\sl240\\slmult1 ");
            else if (sc.style.lineSpacing == 1) fprintf(archivo, "\\sl288\\slmult1 ");
            else if (sc.style.lineSpacing == 2) fprintf(archivo, "\\sl360\\slmult1 ");
            else if (sc.style.lineSpacing == 3) fprintf(archivo, "\\sl480\\slmult1 ");
            
            currentStyle.align = sc.style.align; currentStyle.lineSpacing = sc.style.lineSpacing; newPar = false;
        }

        if (sc.style.bold != currentStyle.bold) { fprintf(archivo, sc.style.bold ? "\\b " : "\\b0 "); currentStyle.bold = sc.style.bold; }
        if (sc.style.italic != currentStyle.italic) { fprintf(archivo, sc.style.italic ? "\\i " : "\\i0 "); currentStyle.italic = sc.style.italic; }
        if (sc.style.underline != currentStyle.underline) { fprintf(archivo, sc.style.underline ? "\\ul " : "\\ulnone "); currentStyle.underline = sc.style.underline; }
        if (sc.style.strike != currentStyle.strike) { fprintf(archivo, sc.style.strike ? "\\strike " : "\\strike0 "); currentStyle.strike = sc.style.strike; }
        if (sc.style.fontSize != currentStyle.fontSize) { fprintf(archivo, "\\fs%d ", sc.style.fontSize); currentStyle.fontSize = sc.style.fontSize; }
        if (sc.style.fgColor != currentStyle.fgColor) { fprintf(archivo, "\\cf%d ", sc.style.fgColor + 1); currentStyle.fgColor = sc.style.fgColor; }
        if (sc.style.bgColor != currentStyle.bgColor) { fprintf(archivo, "\\highlight%d ", sc.style.bgColor + 1); currentStyle.bgColor = sc.style.bgColor; }

        if (sc.glyph == "\n") { fprintf(archivo, "\\par\n"); newPar = true; }
        else if (sc.glyph == "\t") { fprintf(archivo, "\\tab "); }
        else if (sc.glyph == "\\") { fprintf(archivo, "\\\\"); }
        else if (sc.glyph == "{") { fprintf(archivo, "\\{"); }
        else if (sc.glyph == "}") { fprintf(archivo, "\\}"); }
        else {
            uint32_t cp = utf8ToCodepoint(sc.glyph);
            if (cp < 128) fputc(static_cast<char>(cp), archivo);
            else if (cp <= 0xFFFF) fprintf(archivo, "\\u%lu?", (unsigned long)cp);
            else { cp -= 0x10000; fprintf(archivo, "\\u%lu?\\u%lu?", (unsigned long)(0xD800 + (cp >> 10)), (unsigned long)(0xDC00 + (cp & 0x3FF))); }
        }
    }
    fprintf(archivo, "\n}"); fclose(archivo);

    time_t now = time(NULL); struct tm* t = localtime(&now);
    if (t) { char buf[16]; snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min); ultimaHoraGuardado = buf; }
    return true;
}

std::string aplicarTeclaMuerta(char vocal, int tipoMuerta) {
    bool isUpper = (vocal >= 'A' && vocal <= 'Z'); char vL = isUpper ? (vocal + 32) : vocal;
    if (tipoMuerta == 1) {
        if (vL=='a') return isUpper?"Á":"á"; if (vL=='e') return isUpper?"É":"é"; if (vL=='i') return isUpper?"Í":"í";
        if (vL=='o') return isUpper?"Ó":"ó"; if (vL=='u') return isUpper?"Ú":"ú";
    } else if (tipoMuerta == 2) {
        if (vL=='a') return isUpper?"Ä":"ä"; if (vL=='e') return isUpper?"Ë":"ë"; if (vL=='i') return isUpper?"Ï":"ï";
        if (vL=='o') return isUpper?"Ö":"ö"; if (vL=='u') return isUpper?"Ü":"ü";
    } else if (tipoMuerta == 3) {
        if (vL=='a') return isUpper?"À":"à"; if (vL=='e') return isUpper?"È":"è"; if (vL=='i') return isUpper?"Ì":"ì";
        if (vL=='o') return isUpper?"Ò":"ò"; if (vL=='u') return isUpper?"Ù":"ù";
    }
    std::string res = (tipoMuerta == 1 ? "´" : (tipoMuerta == 2 ? "¨" : "`")); res += vocal; return res;
}

void runEditorLoop() {
    C3D_RenderTarget* topScreen = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottomScreen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    C2D_TextBuf dynamicBuf = C2D_TextBufNew(65536);

    std::vector<KeyButton> keys; float btnW = 29.5f, btnH = 22.0f, gap = 2.0f;
    keys.push_back({4.0f, 6.0f, 38.0f, 18.0f, "N", "N", "N", 5});
    keys.push_back({44.0f, 6.0f, 38.0f, 18.0f, "K", "K", "K", 5});
    keys.push_back({84.0f, 6.0f, 38.0f, 18.0f, "S", "S", "S", 5});
    keys.push_back({124.0f, 6.0f, 42.0f, 18.0f, "ALIN", "ALIN", "ALIN", 6}); 
    keys.push_back({168.0f, 6.0f, 46.0f, 18.0f, "INT", "INT", "INT", 8}); 
    keys.push_back({216.0f, 6.0f, 50.0f, 18.0f, "Aa", "Aa", "Aa", 7}); 
    keys.push_back({268.0f, 6.0f, 48.0f, 18.0f, "PILC", "PILC", "PILC", 9}); 

    std::string r1_b[10] = {"1","2","3","4","5","6","7","8","9","0"}; std::string r1_s[10] = {"|","*","·","$","%","&","/","(",")","="}; std::string r1_c[10] = {"","@","#","ª","+","¬","\\","[","]","º"};
    for (int i = 0; i < 10; i++) keys.push_back({8.0f + i * (btnW + gap), 28.0f, btnW, btnH, r1_b[i], r1_s[i], r1_c[i], 0});
    std::string r2_b[10] = {"q","w","e","r","t","y","u","i","o","p"}; std::string r2_s[10] = {"Q","W","E","R","T","Y","U","I","O","P"};
    for (int i = 0; i < 10; i++) keys.push_back({8.0f + i * (btnW + gap), 53.0f, btnW, btnH, r2_b[i], r2_s[i], "", 0});
    std::string r3_b[10] = {"a","s","d","f","g","h","j","k","l","ñ"}; std::string r3_s[10] = {"A","S","D","F","G","H","J","K","L","Ñ"};
    for (int i = 0; i < 10; i++) keys.push_back({8.0f + i * (btnW + gap), 78.0f, btnW, btnH, r3_b[i], r3_s[i], "", 0});
    std::string r4_b[10] = {"z","x","c","v","b","n","m",",",".","´"}; std::string r4_s[10] = {"Z","X","C","V","B","N","M",";",":","¨"}; std::string r4_c[10] = {"<",">","ç","","","{","}","","…","`"};
    for (int i = 0; i < 9; i++) keys.push_back({8.0f + i * (btnW + gap), 103.0f, btnW, btnH, r4_b[i], r4_s[i], r4_c[i], 0});
    keys.push_back({8.0f + 9 * (btnW + gap), 103.0f, btnW, btnH, r4_b[9], r4_s[9], r4_c[9], 4});
    std::string puncs_b[8] = {"«","¿","¡","-","‘","?","!","»"}; std::string puncs_s[8] = {"«","¿","¡","–","“","?","!","»"};
    for (int i = 0; i < 8; i++) keys.push_back({8.0f + i * (btnW + gap), 128.0f, btnW, btnH, puncs_b[i], puncs_s[i], "", 0});
    keys.push_back({8.0f, 153.0f, 304.0f, 22.0f, "ESPACIO", "ESPACIO", "ESPACIO", 2});

    std::vector<StyledChar> documento;
    size_t cursorPos = 0;
    bool hasSelection = false; size_t selAnchor = 0; bool isSelectModeToggle = false;
    std::vector<StyledChar> portapapeles;
    std::string ultimaHoraGuardado = "--:--"; std::string fileName = "Laura_Doc";
    std::vector<EditorState> undoStack; std::vector<EditorState> redoStack;

    CharStyle activeStyle; activeStyle.fontSize = 24; // 12pt
    int estadoTeclaMuerta = 0; bool capsLock = false; u64 lastLPressTick = 0;
    bool showTopInfoBar = true; bool showPilcrow = false;
    int menuAbierto = 0; // 0: Cerrado, 1: Formato, 2: Start, 3: Manual, 4: Guardar Como, 5: Abrir
    bool flagSalir = false;

    const int INITIAL_DELAY = 15; const int REPEAT_RATE = 4;
    int bsCooldown = 0, touchCooldown = 0, activeTouchKeyIdx = -1, dpadCooldown = 0, undoCooldown = 0;
    bool menuTouchTriggered = false; // Evita el lag en menus

    auto pushUndo = [&]() {
        if (!undoStack.empty() && undoStack.back().doc.size() == documento.size()) {
            bool same = true;
            for (size_t k = 0; k < documento.size(); k++) {
                if (documento[k].glyph != undoStack.back().doc[k].glyph || documento[k].style != undoStack.back().doc[k].style) { same = false; break; }
            }
            if (same) return;
        }
        undoStack.push_back({documento, cursorPos});
        if (undoStack.size() > 100) undoStack.erase(undoStack.begin());
        redoStack.clear();
    };

    auto deleteSelection = [&]() {
        if (!hasSelection || selAnchor == cursorPos) return;
        size_t start = std::min(selAnchor, cursorPos), end = std::max(selAnchor, cursorPos);
        documento.erase(documento.begin() + start, documento.begin() + end);
        cursorPos = start; hasSelection = false; isSelectModeToggle = false;
    };

    auto measureChar = [&](const std::string& g, int size) {
        if (g == "\t") return 20.0f; // Tab width aprox
        C2D_Text t; C2D_TextParse(&t, dynamicBuf, g.c_str());
        float gw=0, gh=0; float scale = size / 48.0f;
        C2D_TextGetDimensions(&t, scale, scale, &gw, &gh);
        return (gw < 3.0f) ? (5.0f * scale * 2.0f) : gw;
    };

    while (aptMainLoop() && !flagSalir) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        
        bool isLHeld = (kHeld & KEY_L);
        bool isZLHeld = (kHeld & KEY_ZL);

        if (kDown & KEY_START) menuAbierto = 2; // Abrir Menú Start
        if (kDown & KEY_X) showTopInfoBar = !showTopInfoBar;

        if (kDown & KEY_L) {
            u64 cTick = osGetTime();
            if (cTick - lastLPressTick < 280) { capsLock = !capsLock; lastLPressTick = 0; } 
            else lastLPressTick = cTick;
        }

        if (kDown & KEY_SELECT) {
            isSelectModeToggle = !isSelectModeToggle;
            if (isSelectModeToggle) { hasSelection = true; selAnchor = cursorPos; } else hasSelection = false;
        }

        // --- MANEJO DPAD ---
        bool triggerDpad = false; u32 dpadMask = KEY_DLEFT | KEY_DRIGHT | KEY_DUP | KEY_DDOWN;
        if (kDown & dpadMask) { triggerDpad = true; dpadCooldown = INITIAL_DELAY; }
        else if (kHeld & dpadMask) { if (dpadCooldown > 0) dpadCooldown--; else { triggerDpad = true; dpadCooldown = REPEAT_RATE; } } else dpadCooldown = 0;

        if (triggerDpad && menuAbierto == 0) {
            bool sel = (kHeld & KEY_SELECT) || isSelectModeToggle;
            if (!sel) hasSelection = false; else if (!hasSelection) { hasSelection = true; selAnchor = cursorPos; }
            if (kHeld & KEY_DLEFT) { if (cursorPos > 0) cursorPos--; }
            if (kHeld & KEY_DRIGHT) { if (cursorPos < documento.size()) cursorPos++; }
            // Navegacion vertical simplificada
            if (kHeld & KEY_DUP) { int steps = 40; while(cursorPos > 0 && steps-- > 0) cursorPos--; }
            if (kHeld & KEY_DDOWN) { int steps = 40; while(cursorPos < documento.size() && steps-- > 0) cursorPos++; }
            if (cursorPos > 0 && !hasSelection) activeStyle = documento[cursorPos - 1].style;
        }

        // --- BOTON B (Cerrar Menus o Deshacer) ---
        bool triggerB = false;
        if (kDown & KEY_B) { triggerB = true; undoCooldown = INITIAL_DELAY; }
        else if (kHeld & KEY_B) { if (undoCooldown > 0) undoCooldown--; else { triggerB = true; undoCooldown = REPEAT_RATE; } } else undoCooldown = 0;

        if (triggerB) {
            if (menuAbierto != 0) {
                menuAbierto = 0; // Override: solo cierra menú
            } else {
                hasSelection = false; isSelectModeToggle = false;
                if (isZLHeld) {
                    if (!redoStack.empty()) {
                        undoStack.push_back({documento, cursorPos}); EditorState st = redoStack.back(); redoStack.pop_back();
                        documento = st.doc; cursorPos = std::min(st.cursor, documento.size());
                    }
                } else {
                    if (!undoStack.empty()) {
                        redoStack.push_back({documento, cursorPos}); EditorState st = undoStack.back(); undoStack.pop_back();
                        documento = st.doc; cursorPos = std::min(st.cursor, documento.size());
                    }
                }
            }
        }

        // --- MANEJO TÁCTIL ---
        touchPosition touch; hidTouchRead(&touch);
        int touchedIdx = -1;
        if ((kHeld & KEY_TOUCH) && menuAbierto == 0) {
            for (int i = 0; i < (int)keys.size(); i++) {
                if (touch.px >= keys[i].x && touch.px <= (keys[i].x + keys[i].w) && touch.py >= keys[i].y && touch.py <= (keys[i].y + keys[i].h)) { touchedIdx = i; break; }
            }
        }

        bool touchJustPressed = (kDown & KEY_TOUCH);
        bool triggerTouchKey = false;

        if (menuAbierto != 0) {
            if (touchJustPressed) {
                if (menuAbierto == 1) { // Formato
                    if (touch.px >= 270 && touch.px <= 295 && touch.py >= 12 && touch.py <= 37) menuAbierto = 0; // Cerrar cruz centrada
                    if (touch.px >= 30 && touch.px <= 70 && touch.py >= 50 && touch.py <= 78) activeStyle.fontSize = std::max(12, activeStyle.fontSize - 2);
                    if (touch.px >= 80 && touch.px <= 120 && touch.py >= 50 && touch.py <= 78) activeStyle.fontSize = std::min(72, activeStyle.fontSize + 2);
                    
                    for (int c = 0; c <= 7; c++) {
                        if (touch.px >= 20 + (c * 32) && touch.px <= 46 + (c * 32) && touch.py >= 110 && touch.py <= 136) activeStyle.fgColor = c;
                        if (touch.px >= 20 + (c * 32) && touch.px <= 46 + (c * 32) && touch.py >= 170 && touch.py <= 196) {
                            activeStyle.bgColor = c; if (c != 1) activeStyle.fgColor = 1; // Auto blanco
                        }
                    }
                    if (hasSelection) {
                        pushUndo(); size_t s = std::min(selAnchor, cursorPos), e = std::max(selAnchor, cursorPos);
                        for (size_t idx = s; idx < e; idx++) { documento[idx].style.fontSize = activeStyle.fontSize; documento[idx].style.fgColor = activeStyle.fgColor; documento[idx].style.bgColor = activeStyle.bgColor; }
                    }
                } else if (menuAbierto == 2) { // Start Menu
                    if (touch.px >= 270 && touch.px <= 295 && touch.py >= 12 && touch.py <= 37) menuAbierto = 0; // Cruz
                    if (touch.px >= 80 && touch.px <= 240) {
                        if (touch.py >= 50 && touch.py <= 75) { guardarEnRTF(documento, fileName, ultimaHoraGuardado); menuAbierto = 0; } // Guardar
                        if (touch.py >= 85 && touch.py <= 110) { /* Guardar como stub */ menuAbierto = 0; }
                        if (touch.py >= 120 && touch.py <= 145) { /* Abrir stub */ menuAbierto = 0; }
                        if (touch.py >= 155 && touch.py <= 180) { menuAbierto = 3; } // Manual
                        if (touch.py >= 190 && touch.py <= 215) { flagSalir = true; } // Salir
                    }
                } else if (menuAbierto == 3) { // Manual
                    if (touch.px >= 270 && touch.px <= 295 && touch.py >= 12 && touch.py <= 37) menuAbierto = 0; // Cruz
                }
            }
        } else {
            if (touchJustPressed) { activeTouchKeyIdx = touchedIdx; triggerTouchKey = (touchedIdx != -1); touchCooldown = INITIAL_DELAY; }
            else if ((kHeld & KEY_TOUCH) && touchedIdx != -1 && touchedIdx == activeTouchKeyIdx) {
                if (keys[touchedIdx].type == 0 || keys[touchedIdx].type == 2) {
                    if (touchCooldown > 0) touchCooldown--; else { triggerTouchKey = true; touchCooldown = REPEAT_RATE; }
                }
            } else { activeTouchKeyIdx = -1; touchCooldown = 0; }
        }

        if (triggerTouchKey && activeTouchKeyIdx != -1 && menuAbierto == 0) {
            const auto& key = keys[activeTouchKeyIdx];
            bool isUpper = capsLock; if (key.base.size()==1 && key.base[0]>='a' && key.base[0]<='z') isUpper = (isUpper != isLHeld);
            
            if (key.type == 7) { 
                if (cursorPos > 0 && !hasSelection) activeStyle = documento[cursorPos - 1].style;
                menuAbierto = 1; 
            }
            else if (key.type == 6) { activeStyle.align = (activeStyle.align + 1) % 4; } // Alin
            else if (key.type == 8) { activeStyle.lineSpacing = (activeStyle.lineSpacing + 1) % 4; } // Interlineado
            else if (key.type == 9) { showPilcrow = !showPilcrow; } // Pilcrow
            else if (key.type == 5) {
                if (key.base == "N") activeStyle.bold = !activeStyle.bold;
                if (key.base == "K") activeStyle.italic = !activeStyle.italic;
                if (key.base == "S") activeStyle.underline = !activeStyle.underline;
            } else if (key.type == 4) {
                if (isZLHeld) estadoTeclaMuerta = 3; else if (isLHeld) estadoTeclaMuerta = 2; else estadoTeclaMuerta = 1;
            } else if (key.type == 0 || key.type == 2) {
                pushUndo(); deleteSelection();
                std::string charToAdd = key.base;
                if (key.type == 2) {
                    charToAdd = (isLHeld || isZLHeld) ? "\t" : " ";
                } else {
                    if (isLHeld && isZLHeld && key.base == "c") charToAdd = "Ç";
                    else if (isZLHeld && !key.ctrlAlt.empty()) charToAdd = key.ctrlAlt;
                    else if (isUpper && !key.shift.empty()) charToAdd = key.shift;

                    if (estadoTeclaMuerta > 0 && charToAdd.length() == 1 && ((charToAdd[0]>='a' && charToAdd[0]<='z') || (charToAdd[0]>='A' && charToAdd[0]<='Z'))) {
                        charToAdd = aplicarTeclaMuerta(charToAdd[0], estadoTeclaMuerta); estadoTeclaMuerta = 0;
                    }
                }
                documento.insert(documento.begin() + cursorPos, {charToAdd, activeStyle}); cursorPos++;
            }
            // Aplicar estilos a la seleccion para los botones de estado
            if (hasSelection && (key.type == 5 || key.type == 6 || key.type == 8)) {
                pushUndo(); size_t s = std::min(selAnchor, cursorPos), e = std::max(selAnchor, cursorPos);
                for (size_t idx = s; idx < e; idx++) {
                    if (key.type == 5 && key.base == "N") documento[idx].style.bold = activeStyle.bold;
                    if (key.type == 5 && key.base == "K") documento[idx].style.italic = activeStyle.italic;
                    if (key.type == 5 && key.base == "S") documento[idx].style.underline = activeStyle.underline;
                    if (key.type == 6) documento[idx].style.align = activeStyle.align;
                    if (key.type == 8) documento[idx].style.lineSpacing = activeStyle.lineSpacing;
                }
            }
        }

        bool isBackspaceTrigger = false;
        if (kDown & KEY_ZR) { isBackspaceTrigger = true; bsCooldown = INITIAL_DELAY; }
        else if (kHeld & KEY_ZR) { if (bsCooldown > 0) bsCooldown--; else { isBackspaceTrigger = true; bsCooldown = REPEAT_RATE; } } else bsCooldown = 0;
        if (isBackspaceTrigger && menuAbierto == 0) {
            if (hasSelection) { pushUndo(); deleteSelection(); } 
            else if (cursorPos > 0) { pushUndo(); documento.erase(documento.begin() + (cursorPos - 1)); cursorPos--; }
        }

        // --- CÁLCULO DE LÍNEAS CON SOFT WORD WRAP (Para Render y Alineación) ---
        struct LineSeg { size_t sIdx, eIdx; float w; int align; int spacing; float maxH; };
        std::vector<LineSeg> renderLines;
        float curLW = 0; size_t lStart = 0; float maxLH = 16.5f;

        for (size_t i = 0; i < documento.size(); i++) {
            float gw = measureChar(documento[i].glyph, documento[i].style.fontSize);
            float currentLineHeight = 33.0f * (documento[i].style.fontSize / 48.0f);
            if (currentLineHeight > maxLH) maxLH = currentLineHeight;

            bool forceBreak = (documento[i].glyph == "\n");
            bool softBreak = false;

            if (documento[i].glyph == " " || documento[i].glyph == "\t") {
                float nextWordW = 0;
                for (size_t j = i + 1; j < documento.size() && documento[j].glyph != " " && documento[j].glyph != "\n" && documento[j].glyph != "\t"; j++) {
                    nextWordW += measureChar(documento[j].glyph, documento[j].style.fontSize);
                }
                if (curLW + gw + nextWordW > 380.0f && curLW > 0) softBreak = true;
            } else if (curLW + gw > 380.0f && curLW > 0) {
                softBreak = true;
            }

            if (forceBreak || softBreak) {
                if (forceBreak) curLW += gw; // Inlcuir pilcrow space
                renderLines.push_back({lStart, i, curLW, documento[lStart].style.align, documento[lStart].style.lineSpacing, maxLH});
                if (forceBreak) { lStart = i + 1; curLW = 0; } else { lStart = i; curLW = gw; }
                maxLH = 16.5f;
            } else {
                curLW += gw;
            }
        }
        if (lStart <= documento.size()) {
            int al = lStart < documento.size() ? documento[lStart].style.align : activeStyle.align;
            int sp = lStart < documento.size() ? documento[lStart].style.lineSpacing : activeStyle.lineSpacing;
            renderLines.push_back({lStart, documento.size(), curLW, al, sp, maxLH});
        }

        C2D_TextBufClear(dynamicBuf);

        // --- RENDERIZADO PANTALLA SUPERIOR ---
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(topScreen, C2D_Color32(250, 250, 245, 255));
        C2D_SceneBegin(topScreen);

        if (menuAbierto == 3) {
            // Fondo oscuro para Manual en ambas pantallas
            C2D_DrawRectSolid(0, 0, 0.4f, 400, 240, C2D_Color32(30, 30, 30, 220));
            C2D_DrawRectSolid(5, 5, 0.5f, 390, 230, C2D_Color32(245, 245, 250, 255));
            C2D_DrawRectSolid(5, 5, 0.6f, 390, 230, C2D_Color32(0,0,0,0)); // Border simulation (need 4 rects)
            C2D_DrawRectSolid(5, 5, 0.6f, 390, 3, COLOR_MUSGO); C2D_DrawRectSolid(5, 232, 0.6f, 390, 3, COLOR_MUSGO);
            C2D_DrawRectSolid(5, 5, 0.6f, 3, 230, COLOR_MUSGO); C2D_DrawRectSolid(392, 5, 0.6f, 3, 230, COLOR_MUSGO);
            
            C2D_Text mTxt; C2D_TextParse(&mTxt, dynamicBuf, "MANUAL DE USUARIO - LAURA"); C2D_TextOptimize(&mTxt);
            C2D_DrawText(&mTxt, C2D_WithColor, 20, 15, 0.7f, 0.6f, 0.6f, COLOR_NEGRO);
            C2D_Text t1; C2D_TextParse(&t1, dynamicBuf, "- D-PAD: Mover Cursor\n- X: Info Pantalla\n- L/ZL: Modificadores (Mayus / T. Muertas)\n- Y: Copiar/Cortar\n- A: Pegar\n- ZR: Borrar\n- B: Deshacer / Cerrar Menus");
            C2D_TextOptimize(&t1); C2D_DrawText(&t1, C2D_WithColor, 20, 50, 0.7f, 0.45f, 0.45f, COLOR_NEGRO);
        } else {
            float textTopMargin = 10.0f;
            if (showTopInfoBar) {
                textTopMargin = 26.0f;
                C2D_DrawRectSolid(0, 0, 0.4f, 400, 20, COLOR_NEGRO);
                u8 batLevel = 5; PTMU_GetBatteryLevel(&batLevel); int batPct = (batLevel * 100) / 5;
                char infoStr[128]; 
                time_t now = time(NULL); struct tm* t = localtime(&now);
                snprintf(infoStr, sizeof(infoStr), "Bat:%d%%   Laura - %s.rtf [%s]   %02d/%02d %02d:%02d", 
                         batPct, fileName.c_str(), ultimaHoraGuardado.c_str(), t->tm_mday, t->tm_mon+1, t->tm_hour, t->tm_min);
                C2D_Text titleTxt; C2D_TextParse(&titleTxt, dynamicBuf, infoStr); C2D_TextOptimize(&titleTxt);
                C2D_DrawText(&titleTxt, C2D_WithColor, 8.0f, 3.0f, 0.5f, 0.4f, 0.4f, COLOR_BLANCO);
                
                // Barra Inferior Top Screen
                C2D_DrawRectSolid(0, 220, 0.4f, 400, 20, C2D_Color32(230, 230, 235, 255));
                int words = 0; bool inWord=false; for(auto& c: documento) { if(c.glyph==" "||c.glyph=="\n"||c.glyph=="\t") inWord=false; else if(!inWord){ words++; inWord=true;} }
                int pages = (documento.size() / 1500) + 1;
                char statStr[128]; snprintf(statStr, sizeof(statStr), "Palabras: %d  |  Caracteres: %zu  |  Paginas est.: %d", words, documento.size(), pages);
                C2D_Text statTxt; C2D_TextParse(&statTxt, dynamicBuf, statStr); C2D_TextOptimize(&statTxt);
                C2D_DrawText(&statTxt, C2D_WithColor, 10.0f, 223.0f, 0.5f, 0.4f, 0.4f, COLOR_NEGRO);
            }

            float curY = textTopMargin;
            bool cursorVisible = (osGetTime() / 400) % 2 == 0;
            size_t selStart = hasSelection ? std::min(selAnchor, cursorPos) : 0; size_t selEnd = hasSelection ? std::max(selAnchor, cursorPos) : 0;

            for (const auto& line : renderLines) {
                float startX = 10.0f;
                if (line.align == 1) startX = std::max(10.0f, (400.0f - line.w) / 2.0f);
                else if (line.align == 2) startX = std::max(10.0f, 390.0f - line.w);

                float curX = startX;
                float lineMult = (line.spacing == 0) ? 1.0f : ((line.spacing == 1) ? 1.2f : ((line.spacing == 2) ? 1.5f : 2.0f));

                for (size_t idx = line.sIdx; idx <= line.eIdx; idx++) {
                    if (idx == cursorPos && cursorVisible && menuAbierto == 0) { C2D_DrawRectSolid(curX, curY, 0.75f, 1.5f, line.maxH, COLOR_MUSGO); }
                    if (idx == line.eIdx || idx == documento.size()) break;

                    const auto& sc = documento[idx]; float textScale = sc.style.fontSize / 48.0f;
                    
                    if (sc.glyph == "\n") {
                        if (showPilcrow) {
                            C2D_Text pTxt; C2D_TextParse(&pTxt, dynamicBuf, "¶"); C2D_TextOptimize(&pTxt);
                            C2D_DrawText(&pTxt, C2D_WithColor, curX, curY, 0.5f, textScale, textScale, C2D_Color32(150, 150, 150, 255));
                        }
                        break; // End line visually
                    }

                    float gWidth = measureChar(sc.glyph, sc.style.fontSize);
                    u32 fg = getPaletteColor(sc.style.fgColor); u32 bg = sc.style.bgColor != 1 ? getPaletteColor(sc.style.bgColor) : 0;
                    
                    if (bg != 0) C2D_DrawRectSolid(curX, curY, 0.25f, gWidth, line.maxH, bg);
                    if (hasSelection && idx >= selStart && idx < selEnd) C2D_DrawRectSolid(curX, curY, 0.3f, gWidth, line.maxH, C2D_Color32(180, 210, 245, 180));

                    if (sc.glyph != "\t" && sc.glyph != " ") {
                        C2D_Text glyphText; C2D_TextParse(&glyphText, dynamicBuf, sc.glyph.c_str()); C2D_TextOptimize(&glyphText);
                        
                        // Simulacion de cursiva via rotacion (shear manual con sub-render no está nativo, así que rotamos levemente)
                        float slantRot = sc.style.italic ? 0.15f : 0.0f;
                        C2D_DrawText(&glyphText, C2D_WithColor, curX + (sc.style.italic ? 2.0f:0), curY, 0.5f, textScale, textScale, fg);
                        
                        if (sc.style.bold) C2D_DrawText(&glyphText, C2D_WithColor, curX + (sc.style.italic ? 2.5f:0.5f), curY, 0.51f, textScale, textScale, fg);
                        if (sc.style.underline) C2D_DrawRectSolid(curX, curY + line.maxH - 2.0f, 0.6f, gWidth, 1.5f, fg);
                        if (sc.style.strike) C2D_DrawRectSolid(curX, curY + (line.maxH * 0.55f), 0.65f, gWidth, 1.5f, fg);
                    }
                    curX += gWidth + (sc.style.bold ? 1.0f : 0.0f);
                }
                curY += line.maxH * lineMult;
            }
        }

        // --- RENDERIZADO PANTALLA INFERIOR ---
        C2D_TargetClear(bottomScreen, C2D_Color32(220, 225, 235, 255));
        C2D_SceneBegin(bottomScreen);

        auto drawCloseX = [&](float bx, float by) {
            C2D_DrawRectSolid(bx, by, 0.6f, 25, 25, COLOR_ROJO);
            C2D_Text xTxt; C2D_TextParse(&xTxt, dynamicBuf, "X"); C2D_TextOptimize(&xTxt);
            C2D_DrawText(&xTxt, C2D_WithColor, bx + 6, by + 2, 0.7f, 0.5f, 0.5f, COLOR_BLANCO);
        };

        if (menuAbierto == 1) { // FORMATO
            C2D_DrawRectSolid(10, 10, 0.5f, 300, 205, C2D_Color32(245, 245, 250, 255));
            C2D_DrawRectSolid(10, 10, 0.5f, 300, 28, C2D_Color32(60, 80, 100, 255));
            C2D_Text titleTxt; C2D_TextParse(&titleTxt, dynamicBuf, "AJUSTES DE FORMATO"); C2D_TextOptimize(&titleTxt);
            C2D_DrawText(&titleTxt, C2D_WithColor, 20, 16, 0.6f, 0.42f, 0.42f, COLOR_BLANCO);
            drawCloseX(270, 12);

            char sizeStr[32]; snprintf(sizeStr, sizeof(sizeStr), "TAMANO: %d pt", activeStyle.fontSize / 2);
            C2D_Text szTxt; C2D_TextParse(&szTxt, dynamicBuf, sizeStr); C2D_TextOptimize(&szTxt);
            C2D_DrawText(&szTxt, C2D_WithColor, 20, 52, 0.6f, 0.4f, 0.4f, COLOR_NEGRO);
            
            C2D_DrawRectSolid(30, 70, 0.6f, 40, 28, C2D_Color32(200, 200, 200, 255));
            C2D_Text minusTxt; C2D_TextParse(&minusTxt, dynamicBuf, "-"); C2D_TextOptimize(&minusTxt); C2D_DrawText(&minusTxt, C2D_WithColor, 47, 73, 0.7f, 0.5f, 0.5f, COLOR_NEGRO);
            C2D_DrawRectSolid(80, 70, 0.6f, 40, 28, C2D_Color32(200, 200, 200, 255));
            C2D_Text plusTxt; C2D_TextParse(&plusTxt, dynamicBuf, "+"); C2D_TextOptimize(&plusTxt); C2D_DrawText(&plusTxt, C2D_WithColor, 96, 73, 0.7f, 0.5f, 0.5f, COLOR_NEGRO);
            
            C2D_Text fgTxt; C2D_TextParse(&fgTxt, dynamicBuf, "COLOR FUENTE:"); C2D_TextOptimize(&fgTxt); C2D_DrawText(&fgTxt, C2D_WithColor, 20, 110, 0.6f, 0.4f, 0.4f, COLOR_NEGRO);
            for (int c = 0; c <= 7; c++) {
                C2D_DrawRectSolid(20 + (c * 32) - 1, 130 - 1, 0.55f, 28, 28, COLOR_NEGRO); // Borde negro
                C2D_DrawRectSolid(20 + (c * 32), 130, 0.6f, 26, 26, getPaletteColor(c));
                if (activeStyle.fgColor == c) C2D_DrawRectSolid(20 + (c * 32), 158, 0.6f, 26, 3, COLOR_NEGRO);
            }

            C2D_Text bgTxt; C2D_TextParse(&bgTxt, dynamicBuf, "COLOR FONDO:"); C2D_TextOptimize(&bgTxt); C2D_DrawText(&bgTxt, C2D_WithColor, 20, 170, 0.6f, 0.4f, 0.4f, COLOR_NEGRO);
            for (int c = 0; c <= 7; c++) {
                C2D_DrawRectSolid(20 + (c * 32) - 1, 190 - 1, 0.55f, 28, 28, COLOR_NEGRO);
                C2D_DrawRectSolid(20 + (c * 32), 190, 0.6f, 26, 26, getPaletteColor(c));
                if (activeStyle.bgColor == c) C2D_DrawRectSolid(20 + (c * 32), 218, 0.6f, 26, 3, COLOR_NEGRO);
            }
        } else if (menuAbierto == 2) { // START
            C2D_DrawRectSolid(10, 10, 0.5f, 300, 220, C2D_Color32(245, 245, 250, 255));
            C2D_DrawRectSolid(10, 10, 0.5f, 300, 28, COLOR_MUSGO);
            C2D_Text titleTxt; C2D_TextParse(&titleTxt, dynamicBuf, "MENU PRINCIPAL"); C2D_TextOptimize(&titleTxt);
            C2D_DrawText(&titleTxt, C2D_WithColor, 20, 16, 0.6f, 0.42f, 0.42f, COLOR_BLANCO);
            drawCloseX(270, 12);
            
            auto drawBtn = [&](float y, const char* label) {
                C2D_DrawRectSolid(80, y, 0.6f, 160, 25, C2D_Color32(220, 220, 225, 255));
                C2D_Text lTxt; C2D_TextParse(&lTxt, dynamicBuf, label); C2D_TextOptimize(&lTxt);
                C2D_DrawText(&lTxt, C2D_WithColor, 90, y+5, 0.7f, 0.4f, 0.4f, COLOR_NEGRO);
            };
            drawBtn(50, "GUARDAR"); drawBtn(85, "GUARDAR COMO..."); drawBtn(120, "ABRIR");
            drawBtn(155, "MANUAL"); drawBtn(190, "SALIR");
            
            C2D_Text foot; C2D_TextParse(&foot, dynamicBuf, "(c) Laura Dev 2026"); C2D_TextOptimize(&foot);
            C2D_DrawText(&foot, C2D_WithColor, 105, 220, 0.7f, 0.35f, 0.35f, C2D_Color32(150,150,150,255));
        } else if (menuAbierto == 3) {
            C2D_DrawRectSolid(0, 0, 0.4f, 320, 240, C2D_Color32(30, 30, 30, 220));
            C2D_DrawRectSolid(5, 5, 0.5f, 310, 230, C2D_Color32(245, 245, 250, 255));
            C2D_DrawRectSolid(5, 5, 0.6f, 310, 3, COLOR_MUSGO); C2D_DrawRectSolid(5, 232, 0.6f, 310, 3, COLOR_MUSGO);
            C2D_DrawRectSolid(5, 5, 0.6f, 3, 230, COLOR_MUSGO); C2D_DrawRectSolid(312, 5, 0.6f, 3, 230, COLOR_MUSGO);
            drawCloseX(275, 12);
            C2D_Text t2; C2D_TextParse(&t2, dynamicBuf, "Para navegar archivos, utilice el\nmenu Abrir. Para ver funciones\ntactiles, observe los iconos del\nteclado.");
            C2D_TextOptimize(&t2); C2D_DrawText(&t2, C2D_WithColor, 15, 40, 0.7f, 0.45f, 0.45f, COLOR_NEGRO);
        } else {
            // Teclado normal
            for (int i = 0; i < (int)keys.size(); i++) {
                const auto& key = keys[i];
                u32 cFondo = C2D_Color32(255, 255, 255, 255); u32 cTexto = COLOR_NEGRO;

                if (i == activeTouchKeyIdx) cFondo = C2D_Color32(210, 225, 245, 255);
                if (key.type >= 5 && key.type != 7) {
                    bool activo = false;
                    if (key.base == "N") activo = activeStyle.bold;
                    else if (key.base == "K") activo = activeStyle.italic;
                    else if (key.base == "S") activo = activeStyle.underline;
                    else if (key.type == 9) activo = showPilcrow;
                    if (activo) { cFondo = COLOR_MUSGO; cTexto = COLOR_BLANCO; }
                    else { cFondo = C2D_Color32(200, 205, 215, 255); cTexto = C2D_Color32(60, 65, 75, 255); }
                }

                C2D_DrawRectSolid(key.x, key.y, 0.3f, key.w, key.h, cFondo);

                if (key.type == 6) { // Alineación con Íconos (finos)
                    float bx = key.x + (key.w/2.0f) - 8.0f; float by = key.y + 5.0f; u32 ic = cTexto; float lh = 1.5f;
                    if (activeStyle.align == 0) { C2D_DrawRectSolid(bx, by, 0.4f, 16, lh, ic); C2D_DrawRectSolid(bx, by+4.0f, 0.4f, 10, lh, ic); C2D_DrawRectSolid(bx, by+8.0f, 0.4f, 14, lh, ic); }
                    else if (activeStyle.align == 1) { C2D_DrawRectSolid(bx, by, 0.4f, 16, lh, ic); C2D_DrawRectSolid(bx+3.0f, by+4.0f, 0.4f, 10, lh, ic); C2D_DrawRectSolid(bx+1.0f, by+8.0f, 0.4f, 14, lh, ic); }
                    else if (activeStyle.align == 2) { C2D_DrawRectSolid(bx, by, 0.4f, 16, lh, ic); C2D_DrawRectSolid(bx+6.0f, by+4.0f, 0.4f, 10, lh, ic); C2D_DrawRectSolid(bx+2.0f, by+8.0f, 0.4f, 14, lh, ic); }
                    else { C2D_DrawRectSolid(bx, by, 0.4f, 16, lh, ic); C2D_DrawRectSolid(bx, by+4.0f, 0.4f, 16, lh, ic); C2D_DrawRectSolid(bx, by+8.0f, 0.4f, 16, lh, ic); }
                } else if (key.type == 8) { // Interlineado
                    std::string ls = activeStyle.lineSpacing == 0 ? "1.0" : (activeStyle.lineSpacing == 1 ? "1.2" : (activeStyle.lineSpacing == 2 ? "1.5" : "2.0"));
                    C2D_Text lTxt; C2D_TextParse(&lTxt, dynamicBuf, ls.c_str()); C2D_TextOptimize(&lTxt);
                    C2D_DrawText(&lTxt, C2D_WithColor, key.x + 10, key.y + 4, 0.5f, 0.4f, 0.4f, cTexto);
                } else {
                    std::string mainChar = key.base; float sX = (key.type >= 5) ? 0.40f : 0.38f;
                    float txX = (key.type == 2) ? 60.0f : (key.w / 2.0f) - 4.0f; if(key.type>=5) txX = (key.w/2.0f)-10.0f;
                    float txY = (key.type >= 5) ? 2.5f : 4.0f;
                    
                    if (key.type == 2) { // Dibujar ícono Tabulacion (–>|) en Espacio
                        u32 cSec = C2D_Color32(110, 110, 120, 200);
                        C2D_DrawRectSolid(key.x + 8.0f, key.y + 11.0f, 0.4f, 15.0f, 1.5f, cSec); // Flecha line
                        C2D_DrawRectSolid(key.x + 18.0f, key.y + 8.0f, 0.4f, 1.5f, 7.0f, cSec); // Flecha up
                        C2D_DrawRectSolid(key.x + 19.0f, key.y + 9.0f, 0.4f, 1.5f, 5.0f, cSec);
                        C2D_DrawRectSolid(key.x + 20.0f, key.y + 10.0f, 0.4f, 1.5f, 3.0f, cSec);
                        C2D_DrawRectSolid(key.x + 25.0f, key.y + 7.0f, 0.4f, 2.0f, 9.0f, cSec); // Barra vertical
                    }
                    
                    C2D_Text keyLabel; C2D_TextParse(&keyLabel, dynamicBuf, mainChar.c_str()); C2D_TextOptimize(&keyLabel);
                    C2D_DrawText(&keyLabel, C2D_WithColor, key.x + txX, key.y + txY, 0.5f, sX, sX, cTexto);
                }
                
                if (!isLHeld && !isZLHeld && (key.type == 0 || key.type == 4)) {
                    if (!key.shift.empty() && key.shift != key.base) {
                        C2D_Text sLbl; C2D_TextParse(&sLbl, dynamicBuf, key.shift.c_str()); C2D_TextOptimize(&sLbl);
                        C2D_DrawText(&sLbl, C2D_WithColor, key.x + 2, key.y + 1, 0.5f, 0.20f, 0.20f, C2D_Color32(110, 110, 120, 200));
                    }
                    if (!key.ctrlAlt.empty()) {
                        C2D_Text cLbl; C2D_TextParse(&cLbl, dynamicBuf, key.ctrlAlt.c_str()); C2D_TextOptimize(&cLbl);
                        C2D_DrawText(&cLbl, C2D_WithColor, key.x + key.w - 10, key.y + 1, 0.5f, 0.20f, 0.20f, C2D_Color32(180, 80, 80, 220));
                    }
                }
            }
        }

        C2D_DrawRectSolid(0, 220, 0.4f, 320, 20, C2D_Color32(230, 230, 235, 255));
        char stStr[64]; snprintf(stStr, sizeof(stStr), "L: %s  |  ZL: %s", isLHeld?"ON":"OFF", isZLHeld?"ON":"OFF");
        C2D_Text statusTxt; C2D_TextParse(&statusTxt, dynamicBuf, stStr); C2D_TextOptimize(&statusTxt);
        C2D_DrawText(&statusTxt, C2D_WithColor, 10.0f, 223.0f, 0.5f, 0.4f, 0.4f, COLOR_NEGRO);
        if (isLHeld) C2D_DrawRectSolid(22.0f, 234.0f, 0.45f, 15, 2, COLOR_MUSGO); // Subrayado verde musgo para ON
        if (isZLHeld) C2D_DrawRectSolid(72.0f, 234.0f, 0.45f, 15, 2, COLOR_MUSGO);

        C3D_FrameEnd(0);
    }
    C2D_TextBufDelete(dynamicBuf);
}
