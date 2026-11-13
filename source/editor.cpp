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

struct CharStyle {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strike = false;

    bool operator==(const CharStyle& o) const {
        return bold == o.bold && italic == o.italic && underline == o.underline && strike == o.strike;
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
    int type; // 0 = texto, 2 = espacio, 3 = guardar, 4 = tecla muerta, 5 = formato
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
    fprintf(archivo, "{\\fonttbl{\\f0\\fmodern\\fcharset0 Courier New;}}\n");
    fprintf(archivo, "\\viewkind4\\uc1\\pard\\sa200\\sl276\\slmult1\\f0\\fs24\\lang3082 ");

    CharStyle currentStyle;
    std::string plain = styledToPlainString(doc);
    const std::string pageBreakTag = "\n--- [PAGINA NUEVA] ---\n";
    size_t i = 0;
    size_t byteOffset = 0;

    while (i < doc.size()) {
        if (plain.compare(byteOffset, pageBreakTag.size(), pageBreakTag) == 0) {
            fprintf(archivo, "\\page\n");
            size_t tagChars = pageBreakTag.size();
            i += tagChars;
            byteOffset += tagChars;
            continue;
        }

        const auto& sc = doc[i];
        if (sc.style.bold != currentStyle.bold) { 
            fprintf(archivo, sc.style.bold ? "\\b " : "\\b0 "); 
            currentStyle.bold = sc.style.bold; 
        }
        if (sc.style.italic != currentStyle.italic) { 
            fprintf(archivo, sc.style.italic ? "\\i " : "\\i0 "); 
            currentStyle.italic = sc.style.italic; 
        }
        if (sc.style.underline != currentStyle.underline) { 
            fprintf(archivo, sc.style.underline ? "\\ul " : "\\ulnone "); 
            currentStyle.underline = sc.style.underline; 
        }
        if (sc.style.strike != currentStyle.strike) { 
            fprintf(archivo, sc.style.strike ? "\\strike " : "\\strike0 "); 
            currentStyle.strike = sc.style.strike; 
        }

        if (sc.glyph == "\n") fprintf(archivo, "\\par\n");
        else if (sc.glyph == "\\") fprintf(archivo, "\\\\");
        else if (sc.glyph == "{") fprintf(archivo, "\\{");
        else if (sc.glyph == "}") fprintf(archivo, "\\}");
        else {
            uint32_t cp = utf8ToCodepoint(sc.glyph);
            if (cp < 128) fputc(static_cast<char>(cp), archivo);
            else if (cp <= 0xFFFF) fprintf(archivo, "\\u%lu?", (unsigned long)cp);
            else { 
                cp -= 0x10000; 
                fprintf(archivo, "\\u%lu?\\u%lu?", (unsigned long)(0xD800 + (cp >> 10)), (unsigned long)(0xDC00 + (cp & 0x3FF))); 
            }
        }
        byteOffset += sc.glyph.size();
        i++;
    }

    if (currentStyle.bold) fprintf(archivo, "\\b0 ");
    if (currentStyle.italic) fprintf(archivo, "\\i0 ");
    if (currentStyle.underline) fprintf(archivo, "\\ulnone ");
    if (currentStyle.strike) fprintf(archivo, "\\strike0 ");
    fprintf(archivo, "\n}");
    fclose(archivo);

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    if (t) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
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
    std::string palabra;
    int contador = 0;
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

    keys.push_back({8.0f, 6.0f, 98.0f, 18.0f, "N", "N", "N", 5});
    keys.push_back({111.0f, 6.0f, 98.0f, 18.0f, "K", "K", "K", 5});
    keys.push_back({214.0f, 6.0f, 98.0f, 18.0f, "S", "S", "S", 5});

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

    keys.push_back({8.0f, 153.0f, 185.0f, 22.0f, "ESPACIO / TAB", "ESPACIO / TAB", " ", 2});
    keys.push_back({198.0f, 153.0f, 114.0f, 22.0f, "GUARDAR", "GUARDAR", "GUARDAR", 3});

    std::vector<StyledChar> documento;
    size_t cursorPos = 0;
    
    bool hasSelection = false;
    bool isSelectModeToggle = false;
    size_t selAnchor = 0;
    
    std::vector<StyledChar> portapapeles;
    std::string ultimaHoraGuardado = "--:--";

    std::vector<EditorState> undoStack;
    std::vector<EditorState> redoStack;

    auto pushUndo = [&]() {
        if (!undoStack.empty() && undoStack.back().doc.size() == documento.size()) {
            bool same = true;
            for (size_t k = 0; k < documento.size(); k++) {
                if (documento[k].glyph != undoStack.back().doc[k].glyph || documento[k].style != undoStack.back().doc[k].style) { same = false; break; }
            }
            if (same) return;
        }
        undoStack.push_back({documento, cursorPos});
        if (undoStack.size() > 250) undoStack.erase(undoStack.begin());
        redoStack.clear();
    };

    auto deleteSelectionIfAny = [&]() -> bool {
        if (!hasSelection || selAnchor == cursorPos) { hasSelection = false; isSelectModeToggle = false; return false; }
        size_t start = std::min(selAnchor, cursorPos);
        size_t end = std::max(selAnchor, cursorPos);
        documento.erase(documento.begin() + start, documento.begin() + end);
        cursorPos = start;
        hasSelection = false;
        isSelectModeToggle = false;
        return true;
    };

    int estadoTeclaMuerta = 0;
    bool capsLock = false;
    u64 lastLPressTick = 0;
    CharStyle activeStyle;
    bool showTopInfoBar = true;

    const int INITIAL_DELAY = 15;
    const int REPEAT_RATE = 4;

    int bsCooldown = 0, touchCooldown = 0, activeTouchKeyIdx = -1, dpadCooldown = 0, undoCooldown = 0;

    auto isSpaceOrNewline = [](const std::string& g) { return g == " " || g == "\n"; };

    auto moveCursorLeft = [&](bool selecting, bool jump) {
        if (selecting && !hasSelection) { hasSelection = true; selAnchor = cursorPos; }
        if (!selecting) hasSelection = false;
        if (jump) {
            while (cursorPos > 0 && isSpaceOrNewline(documento[cursorPos - 1].glyph)) cursorPos--;
            while (cursorPos > 0 && !isSpaceOrNewline(documento[cursorPos - 1].glyph)) cursorPos--;
        } else if (cursorPos > 0) cursorPos--;
        if (!hasSelection && cursorPos > 0) activeStyle = documento[cursorPos - 1].style;
    };

    auto moveCursorRight = [&](bool selecting, bool jump) {
        if (selecting && !hasSelection) { hasSelection = true; selAnchor = cursorPos; }
        if (!selecting) hasSelection = false;
        if (jump) {
            while (cursorPos < documento.size() && !isSpaceOrNewline(documento[cursorPos].glyph)) cursorPos++;
            while (cursorPos < documento.size() && isSpaceOrNewline(documento[cursorPos].glyph)) cursorPos++;
        } else if (cursorPos < documento.size()) cursorPos++;
        if (!hasSelection && cursorPos > 0) activeStyle = documento[cursorPos - 1].style;
    };

    auto moveCursorUp = [&](bool selecting, bool jump) {
        if (selecting && !hasSelection) { hasSelection = true; selAnchor = cursorPos; }
        if (!selecting) hasSelection = false;
        if (jump) {
            if (cursorPos > 0) cursorPos--;
            while (cursorPos > 0 && documento[cursorPos - 1].glyph != "\n") cursorPos--;
        } else {
            size_t lineStart = cursorPos;
            while (lineStart > 0 && documento[lineStart - 1].glyph != "\n") lineStart--;
            if (lineStart == 0) { cursorPos = 0; return; }
            size_t col = cursorPos - lineStart;
            size_t prevLineStart = lineStart - 1;
            while (prevLineStart > 0 && documento[prevLineStart - 1].glyph != "\n") prevLineStart--;
            size_t prevLineLen = (lineStart - 1) - prevLineStart;
            cursorPos = prevLineStart + std::min(col, prevLineLen);
        }
        if (!hasSelection && cursorPos > 0) activeStyle = documento[cursorPos - 1].style;
    };

    auto moveCursorDown = [&](bool selecting, bool jump) {
        if (selecting && !hasSelection) { hasSelection = true; selAnchor = cursorPos; }
        if (!selecting) hasSelection = false;
        if (jump) {
            while (cursorPos < documento.size() && documento[cursorPos].glyph != "\n") cursorPos++;
            if (cursorPos < documento.size()) cursorPos++; 
        } else {
            size_t lineStart = cursorPos;
            while (lineStart > 0 && documento[lineStart - 1].glyph != "\n") lineStart--;
            size_t col = cursorPos - lineStart;
            size_t lineEnd = cursorPos;
            while (lineEnd < documento.size() && documento[lineEnd].glyph != "\n") lineEnd++;
            if (lineEnd >= documento.size()) { cursorPos = documento.size(); return; }
            size_t nextLineStart = lineEnd + 1;
            size_t nextLineEnd = nextLineStart;
            while (nextLineEnd < documento.size() && documento[nextLineEnd].glyph != "\n") nextLineEnd++;
            cursorPos = nextLineStart + std::min(col, nextLineEnd - nextLineStart);
        }
        if (!hasSelection && cursorPos > 0) activeStyle = documento[cursorPos - 1].style;
    };

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

        if (kDown & KEY_SELECT) {
            isSelectModeToggle = !isSelectModeToggle;
            if (isSelectModeToggle) { hasSelection = true; selAnchor = cursorPos; } 
            else hasSelection = false;
        }

        u32 dpadMask = KEY_DLEFT | KEY_DRIGHT | KEY_DUP | KEY_DDOWN;
        bool triggerDpad = false;
        if (kDown & dpadMask) { triggerDpad = true; dpadCooldown = INITIAL_DELAY; }
        else if (kHeld & dpadMask) {
            if (dpadCooldown > 0) dpadCooldown--;
            else { triggerDpad = true; dpadCooldown = REPEAT_RATE; }
        } else dpadCooldown = 0;

        bool selecting = isSelectHeld || isSelectModeToggle;
        if (triggerDpad) {
            if (kHeld & KEY_DLEFT) moveCursorLeft(selecting, isLHeld);
            if (kHeld & KEY_DRIGHT) moveCursorRight(selecting, isLHeld);
            if (kHeld & KEY_DUP) moveCursorUp(selecting, isLHeld);
            if (kHeld & KEY_DDOWN) moveCursorDown(selecting, isLHeld);
        }

        bool triggerB = false;
        if (kDown & KEY_B) { triggerB = true; undoCooldown = INITIAL_DELAY; }
        else if (kHeld & KEY_B) {
            if (undoCooldown > 0) undoCooldown--;
            else { triggerB = true; undoCooldown = REPEAT_RATE; }
        } else undoCooldown = 0;

        if (triggerB) {
            hasSelection = false; isSelectModeToggle = false;
            if (isZLHeld) {
                if (!redoStack.empty()) {
                    undoStack.push_back({documento, cursorPos});
                    EditorState st = redoStack.back(); redoStack.pop_back();
                    documento = st.doc; cursorPos = std::min(st.cursor, documento.size());
                }
            } else if (isLHeld) {
                for (int step = 0; step < 5 && !undoStack.empty(); step++) {
                    redoStack.push_back({documento, cursorPos});
                    EditorState st = undoStack.back(); undoStack.pop_back();
                    documento = st.doc; cursorPos = std::min(st.cursor, documento.size());
                }
            } else {
                if (!undoStack.empty()) {
                    redoStack.push_back({documento, cursorPos});
                    EditorState st = undoStack.back(); undoStack.pop_back();
                    documento = st.doc; cursorPos = std::min(st.cursor, documento.size());
                }
            }
        }

        // --- COPIAR / CORTAR (Y) ---
        if (kDown & KEY_Y) {
            if (hasSelection && selAnchor != cursorPos) {
                size_t start = std::min(selAnchor, cursorPos);
                size_t end = std::max(selAnchor, cursorPos);
                portapapeles.assign(documento.begin() + start, documento.begin() + end);

                if (isLHeld || isZLHeld) {
                    pushUndo();
                    documento.erase(documento.begin() + start, documento.begin() + end);
                    cursorPos = start;
                }
                hasSelection = false; isSelectModeToggle = false;
            }
        }

        // --- PEGAR (A) ---
        if (kDown & KEY_A) {
            if (!portapapeles.empty()) {
                pushUndo(); deleteSelectionIfAny();
                std::vector<StyledChar> aPegar = portapapeles;
                if (isLHeld || isZLHeld) {
                    for (auto& sc : aPegar) sc.style = activeStyle;
                }
                documento.insert(documento.begin() + cursorPos, aPegar.begin(), aPegar.end());
                cursorPos += aPegar.size();
                isSelectModeToggle = false;
            }
        }

        bool isBackspaceTrigger = false;
        if (kDown & KEY_ZR) { isBackspaceTrigger = true; bsCooldown = INITIAL_DELAY; }
        else if (kHeld & KEY_ZR) {
            if (bsCooldown > 0) bsCooldown--;
            else { isBackspaceTrigger = true; bsCooldown = REPEAT_RATE; }
        } else bsCooldown = 0;

        if (isBackspaceTrigger) {
            if (hasSelection && selAnchor != cursorPos) {
                pushUndo(); deleteSelectionIfAny();
            } else if (cursorPos > 0) {
                pushUndo();
                if (isLHeld) {
                    while (cursorPos > 0 && isSpaceOrNewline(documento[cursorPos - 1].glyph)) { documento.erase(documento.begin() + (cursorPos - 1)); cursorPos--; }
                    while (cursorPos > 0 && !isSpaceOrNewline(documento[cursorPos - 1].glyph)) { documento.erase(documento.begin() + (cursorPos - 1)); cursorPos--; }
                } else if (isZLHeld) {
                    size_t target = cursorPos;
                    while (target > 0 && documento[target - 1].glyph != "\n") target--;
                    if (target == cursorPos && cursorPos > 0) target--;
                    documento.erase(documento.begin() + target, documento.begin() + cursorPos);
                    cursorPos = target;
                } else {
                    documento.erase(documento.begin() + (cursorPos - 1));
                    cursorPos--;
                }
            }
        }

        if (kDown & KEY_R) {
            pushUndo(); deleteSelectionIfAny();
            std::string ins = isZLHeld ? "\n--- [PAGINA NUEVA] ---\n" : "\n";
            auto styledIns = stringToStyled(ins, activeStyle);
            documento.insert(documento.begin() + cursorPos, styledIns.begin(), styledIns.end());
            cursorPos += styledIns.size();
        }

        touchPosition touch;
        hidTouchRead(&touch);

        int touchedIdx = -1;
        if (kHeld & KEY_TOUCH) {
            for (int i = 0; i < (int)keys.size(); i++) {
                if (touch.px >= keys[i].x && touch.px <= (keys[i].x + keys[i].w) && touch.py >= keys[i].y && touch.py <= (keys[i].y + keys[i].h)) {
                    touchedIdx = i; break;
                }
            }
        }

        bool triggerTouchKey = false;
        if (kDown & KEY_TOUCH) {
            activeTouchKeyIdx = touchedIdx; triggerTouchKey = (touchedIdx != -1); touchCooldown = INITIAL_DELAY;
        } else if ((kHeld & KEY_TOUCH) && touchedIdx != -1 && touchedIdx == activeTouchKeyIdx) {
            if (keys[touchedIdx].type == 0 || keys[touchedIdx].type == 2) {
                if (touchCooldown > 0) touchCooldown--;
                else { triggerTouchKey = true; touchCooldown = REPEAT_RATE; }
            }
        } else {
            activeTouchKeyIdx = -1; touchCooldown = 0;
        }

        if (triggerTouchKey && activeTouchKeyIdx != -1) {
            const auto& key = keys[activeTouchKeyIdx];
            bool effectiveShift = isLHeld || (capsLock && isLetterKey(key.base));

            if (key.type == 5) {
                if (key.base == "N") {
                    activeStyle.bold = !activeStyle.bold;
                    if (hasSelection && selAnchor != cursorPos) {
                        pushUndo(); size_t s = std::min(selAnchor, cursorPos), e = std::max(selAnchor, cursorPos);
                        for (size_t idx = s; idx < e; idx++) documento[idx].style.bold = activeStyle.bold;
                    }
                } else if (key.base == "K") {
                    activeStyle.italic = !activeStyle.italic;
                    if (hasSelection && selAnchor != cursorPos) {
                        pushUndo(); size_t s = std::min(selAnchor, cursorPos), e = std::max(selAnchor, cursorPos);
                        for (size_t idx = s; idx < e; idx++) documento[idx].style.italic = activeStyle.italic;
                    }
                } else if (key.base == "S") {
                    if (isLHeld) {
                        activeStyle.strike = !activeStyle.strike;
                    } else if (isZLHeld) {
                        bool nuevoEstado = !(activeStyle.underline && activeStyle.strike);
                        activeStyle.underline = nuevoEstado;
                        activeStyle.strike = nuevoEstado;
                    } else {
                        activeStyle.underline = !activeStyle.underline;
                    }
                    
                    if (hasSelection && selAnchor != cursorPos) {
                        pushUndo(); size_t s = std::min(selAnchor, cursorPos), e = std::max(selAnchor, cursorPos);
                        for (size_t idx = s; idx < e; idx++) { 
                            documento[idx].style.underline = activeStyle.underline; 
                            documento[idx].style.strike = activeStyle.strike; 
                        }
                    }
                }
            } else if (key.type == 4) {
                if (isZLHeld) estadoTeclaMuerta = 3; else if (isLHeld) estadoTeclaMuerta = 2; else estadoTeclaMuerta = 1;
            } else if (key.type == 0) {
                pushUndo(); deleteSelectionIfAny();
                std::string charToAdd = key.base;
                if (isLHeld && isZLHeld && key.base == "c") charToAdd = "Ç";
                else if (isZLHeld && !key.ctrlAlt.empty()) charToAdd = key.ctrlAlt;
                else if (effectiveShift && !key.shift.empty()) charToAdd = key.shift;

                if (estadoTeclaMuerta > 0 && charToAdd.length() == 1 && ((charToAdd[0] >= 'a' && charToAdd[0] <= 'z') || (charToAdd[0] >= 'A' && charToAdd[0] <= 'Z'))) {
                    std::string acc = aplicarTeclaMuerta(charToAdd[0], estadoTeclaMuerta);
                    documento.insert(documento.begin() + cursorPos, {acc, activeStyle});
                    cursorPos++; estadoTeclaMuerta = 0;
                } else {
                    if (estadoTeclaMuerta > 0) {
                        std::string dead = (estadoTeclaMuerta == 1 ? "´" : (estadoTeclaMuerta == 2 ? "¨" : "`"));
                        documento.insert(documento.begin() + cursorPos, {dead, activeStyle});
                        cursorPos++; estadoTeclaMuerta = 0;
                    }
                    documento.insert(documento.begin() + cursorPos, {charToAdd, activeStyle});
                    cursorPos++;
                }
            } else if (key.type == 2) {
                pushUndo(); deleteSelectionIfAny();
                if (estadoTeclaMuerta > 0) {
                    std::string dead = (estadoTeclaMuerta == 1 ? "´" : (estadoTeclaMuerta == 2 ? "¨" : "`"));
                    documento.insert(documento.begin() + cursorPos, {dead, activeStyle});
                    cursorPos++; estadoTeclaMuerta = 0;
                }
                int spaces = isLHeld ? 5 : 1;
                for (int sp = 0; sp < spaces; sp++) { 
                    documento.insert(documento.begin() + cursorPos, {" ", activeStyle}); 
                    cursorPos++; 
                }
            } else if (key.type == 3) {
                guardarEnRTF(documento, ultimaHoraGuardado);
            }
        }

        C2D_TextBufClear(dynamicBuf);

        std::string plainDoc = styledToPlainString(documento);
        char infoEstado[128];
        snprintf(infoEstado, sizeof(infoEstado), "Pal: %d | Car: %zu | L:%s ZL:%s", 
                 contarPalabras(plainDoc), documento.size(), isLHeld ? "ON" : (capsLock ? "BLQ" : "OFF"), isZLHeld ? "ON" : "OFF");

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

            u8 batLevel = 5; PTMU_GetBatteryLevel(&batLevel); int batPct = (batLevel * 100) / 5;
            char batStr[16]; snprintf(batStr, sizeof(batStr), "Bat: %d%%", batPct);
            char titleStr[64]; snprintf(titleStr, sizeof(titleStr), "Laura (%s)", ultimaHoraGuardado.c_str());

            time_t now = time(NULL); struct tm* t = localtime(&now);
            char dateStr[32];
            if (t) snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%02d %02d:%02d", t->tm_mday, t->tm_mon + 1, t->tm_year % 100, t->tm_hour, t->tm_min);
            else snprintf(dateStr, sizeof(dateStr), "--/--/-- --:--");

            C2D_Text batTxt, titleTxt, dateTxt;
            C2D_TextParse(&batTxt, dynamicBuf, batStr); C2D_TextOptimize(&batTxt);
            C2D_TextParse(&titleTxt, dynamicBuf, titleStr); C2D_TextOptimize(&titleTxt);
            C2D_TextParse(&dateTxt, dynamicBuf, dateStr); C2D_TextOptimize(&dateTxt);

            C2D_DrawText(&batTxt, C2D_WithColor, 8.0f, 3.0f, 0.5f, 0.38f, 0.38f, C2D_Color32(220, 235, 220, 255));
            C2D_DrawText(&titleTxt, C2D_WithColor, 155.0f, 3.0f, 0.5f, 0.38f, 0.38f, C2D_Color32(255, 255, 255, 255));
            C2D_DrawText(&dateTxt, C2D_WithColor, 292.0f, 3.0f, 0.5f, 0.38f, 0.38f, C2D_Color32(220, 225, 235, 255));
        }

        float curX = 10.0f;
        float curY = textTopMargin;
        const float maxLineX = 390.0f;
        const float lineHeight = 16.5f;
        const float textScale = 0.50f;
        bool cursorVisible = (osGetTime() / 400) % 2 == 0;

        size_t selStart = hasSelection ? std::min(selAnchor, cursorPos) : 0;
        size_t selEnd = hasSelection ? std::max(selAnchor, cursorPos) : 0;

        for (size_t idx = 0; idx <= documento.size(); idx++) {
            if (idx == cursorPos && cursorVisible) {
                C2D_DrawRectSolid(curX, curY + 1.0f, 0.75f, 1.5f, lineHeight - 2.0f, C2D_Color32(78, 122, 101, 255));
            }

            if (idx == documento.size()) break;

            const auto& sc = documento[idx];
            if (sc.glyph == "\n") {
                if (hasSelection && idx >= selStart && idx < selEnd) C2D_DrawRectSolid(curX, curY, 0.25f, 5.0f, lineHeight, C2D_Color32(180, 210, 245, 255));
                curX = 10.0f; curY += lineHeight; continue;
            }

            C2D_Text glyphText;
            C2D_TextParse(&glyphText, dynamicBuf, sc.glyph.c_str());
            C2D_TextOptimize(&glyphText);

            float gWidth = 0.0f, gHeight = 0.0f;
            C2D_TextGetDimensions(&glyphText, textScale, textScale, &gWidth, &gHeight);
            if (gWidth < 3.0f) gWidth = 5.0f;

            if (curX + gWidth > maxLineX) { curX = 10.0f; curY += lineHeight; }

            if (hasSelection && idx >= selStart && idx < selEnd) {
                C2D_DrawRectSolid(curX, curY, 0.25f, gWidth + 0.5f, lineHeight, C2D_Color32(180, 210, 245, 255));
            }

            u32 inkColor = sc.style.italic ? C2D_Color32(35, 55, 95, 255) : C2D_Color32(28, 28, 30, 255);
            float italicShiftX = sc.style.italic ? 2.2f : 0.0f;

            C2D_DrawText(&glyphText, C2D_WithColor, curX + italicShiftX, curY, 0.5f, textScale, textScale, inkColor);
            if (sc.style.bold) C2D_DrawText(&glyphText, C2D_WithColor, curX + italicShiftX + 0.85f, curY, 0.51f, textScale, textScale, inkColor);
            if (sc.style.underline) C2D_DrawRectSolid(curX, curY + lineHeight - 2.5f, 0.6f, gWidth + 0.5f, 1.2f, inkColor);
            if (sc.style.strike) C2D_DrawRectSolid(curX, curY + (lineHeight * 0.52f), 0.65f, gWidth + 0.5f, 1.3f, inkColor);

            curX += gWidth + (sc.style.bold ? 0.5f : 0.0f);
        }

        // --- PANTALLA INFERIOR ---
        C2D_TargetClear(bottomScreen, C2D_Color32(220, 225, 235, 255));
        C2D_SceneBegin(bottomScreen);

        for (int i = 0; i < (int)keys.size(); i++) {
            const auto& key = keys[i];
            u32 colorFondo = C2D_Color32(255, 255, 255, 255);
            u32 colorTexto = C2D_Color32(30, 30, 35, 255);

            if (i == activeTouchKeyIdx) colorFondo = C2D_Color32(210, 225, 245, 255);

            if (key.type == 3) {
                colorFondo = C2D_Color32(65, 130, 190, 255); colorTexto = C2D_Color32(255, 255, 255, 255);
            } else if (key.type == 5) {
                bool activo = false;
                if (key.base == "N") activo = activeStyle.bold;
                else if (key.base == "K") activo = activeStyle.italic;
                else if (key.base == "S") {
                    if (isLHeld) activo = activeStyle.strike;
                    else if (isZLHeld) activo = (activeStyle.underline && activeStyle.strike);
                    else activo = activeStyle.underline;
                }

                if (activo) { colorFondo = C2D_Color32(90, 105, 120, 255); colorTexto = C2D_Color32(255, 255, 255, 255); }
                else { colorFondo = C2D_Color32(200, 205, 215, 255); colorTexto = C2D_Color32(60, 65, 75, 255); }
            } else if (key.type == 4 && estadoTeclaMuerta > 0) {
                colorFondo = C2D_Color32(230, 180, 100, 255);
            }

            C2D_DrawRectSolid(key.x, key.y, 0.3f, key.w, key.h, colorFondo);

            std::string mainChar = key.base;
            bool effectiveShift = isLHeld || (capsLock && isLetterKey(key.base));

            if (key.type == 5 && key.base == "S") {
                if (isLHeld) mainChar = "T";
                else if (isZLHeld) mainChar = "S/T";
            }

            if (key.type == 0 || key.type == 4) {
                if (isLHeld && isZLHeld && key.base == "c") mainChar = "Ç";
                else if (isZLHeld && !key.ctrlAlt.empty()) mainChar = key.ctrlAlt;
                else if (effectiveShift && !key.shift.empty()) mainChar = key.shift;
            }

            float textOffsetX = (key.type == 2) ? (key.w / 2.0f) - 38.0f : (key.w / 2.0f) - 4.0f;
            float textOffsetY = (key.type == 5) ? 2.0f : 4.0f;
            float scale = (key.type == 5) ? 0.42f : 0.38f;

            C2D_Text keyLabel; C2D_Text
