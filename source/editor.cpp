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
#include <dirent.h>

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
        case 0: return COLOR_NEGRO; case 1: return COLOR_BLANCO; case 2: return COLOR_ROJO; case 3: return COLOR_AZUL;
        case 4: return COLOR_VERDE; case 5: return COLOR_AMARILLO; case 6: return COLOR_NARANJO; case 7: return COLOR_FUCSIA;
        default: return COLOR_NEGRO;
    }
}

std::string getRTFColor(int index) {
    switch (index) {
        case 0: return "\\red28\\green28\\blue30;"; case 1: return "\\red255\\green255\\blue255;"; case 2: return "\\red220\\green50\\blue50;";
        case 3: return "\\red50\\green100\\blue220;"; case 4: return "\\red50\\green180\\blue50;"; case 5: return "\\red230\\green200\\blue40;";
        case 6: return "\\red240\\green120\\blue30;"; case 7: return "\\red220\\green50\\blue220;"; default: return "\\red0\\green0\\blue0;";
    }
}

struct CharStyle {
    bool bold = false, italic = false, underline = false, strike = false;
    int align = 0, lineSpacing = 0, fontSize = 24, fgColor = 0, bgColor = 1, indent = 0;
    bool operator==(const CharStyle& o) const {
        return bold == o.bold && italic == o.italic && underline == o.underline && strike == o.strike && 
               align == o.align && lineSpacing == o.lineSpacing && fontSize == o.fontSize && fgColor == o.fgColor && bgColor == o.bgColor && indent == o.indent;
    }
    bool operator!=(const CharStyle& o) const { return !(*this == o); }
};

struct StyledChar { std::string glyph; CharStyle style; };
struct KeyButton { float x, y, w, h; std::string base, shift, ctrlAlt; int type; };
struct EditorState { std::vector<StyledChar> doc; size_t cursor; };

uint32_t utf8ToCodepoint(const std::string& g) {
    if (g.empty()) return 0;
    unsigned char c0 = static_cast<unsigned char>(g[0]);
    if (c0 < 0x80) return c0;
    if ((c0 & 0xE0) == 0xC0 && g.size() >= 2) return ((c0 & 0x1F) << 6) | (static_cast<unsigned char>(g[1]) & 0x3F);
    if ((c0 & 0xF0) == 0xE0 && g.size() >= 3) return ((c0 & 0x0F) << 12) | ((static_cast<unsigned char>(g[1]) & 0x3F) << 6) | (static_cast<unsigned char>(g[2]) & 0x3F);
    if ((c0 & 0xF8) == 0xF0 && g.size() >= 4) return ((c0 & 0x07) << 18) | ((static_cast<unsigned char>(g[1]) & 0x3F) << 12) | ((static_cast<unsigned char>(g[2]) & 0x3F) << 6) | (static_cast<unsigned char>(g[3]) & 0x3F);
    return '?';
}

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

bool guardarEnRTF(const std::vector<StyledChar>& doc, std::string& fileName, std::string& ultimaHoraGuardado) {
    std::string path = "sdmc:/" + fileName + ".rtf";
    FILE* archivo = fopen(path.c_str(), "w");
    if (!archivo) return false;

    fprintf(archivo, "{\\rtf1\\ansi\\ansicpg1252\\deff0\\nouicompat\n");
    fprintf(archivo, "{\\fonttbl{\\f0\\fmodern\\fcharset0 Courier New;}}\n");
    fprintf(archivo, "{\\colortbl;");
    for (int c = 0; c <= 7; c++) fprintf(archivo, "%s", getRTFColor(c).c_str());
    fprintf(archivo, "}\n\\viewkind4\\uc1\\pard\\lang3082 ");

    CharStyle currentStyle; bool newPar = true;
    for (size_t i = 0; i < doc.size(); i++) {
        const auto& sc = doc[i];
        if (newPar || sc.style.align != currentStyle.align || sc.style.lineSpacing != currentStyle.lineSpacing || sc.style.indent != currentStyle.indent) {
            if (sc.style.align == 0) fprintf(archivo, "\\ql "); else if (sc.style.align == 1) fprintf(archivo, "\\qc ");
            else if (sc.style.align == 2) fprintf(archivo, "\\qr "); else if (sc.style.align == 3) fprintf(archivo, "\\qj ");
            
            if (sc.style.lineSpacing == 0) fprintf(archivo, "\\sl240\\slmult1 "); else if (sc.style.lineSpacing == 1) fprintf(archivo, "\\sl288\\slmult1 ");
            else if (sc.style.lineSpacing == 2) fprintf(archivo, "\\sl360\\slmult1 "); else if (sc.style.lineSpacing == 3) fprintf(archivo, "\\sl480\\slmult1 ");
            
            fprintf(archivo, "\\li%d ", sc.style.indent * 360);
            
            currentStyle.align = sc.style.align; currentStyle.lineSpacing = sc.style.lineSpacing; currentStyle.indent = sc.style.indent; newPar = false;
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
            if (cp < 128) fputc(static_cast<char>(cp), archivo); else if (cp <= 0xFFFF) fprintf(archivo, "\\u%lu?", (unsigned long)cp);
            else { cp -= 0x10000; fprintf(archivo, "\\u%lu?\\u%lu?", (unsigned long)(0xD800 + (cp >> 10)), (unsigned long)(0xDC00 + (cp & 0x3FF))); }
        }
    }
    fprintf(archivo, "\n}"); fclose(archivo);

    time_t now = time(NULL); struct tm* t = localtime(&now);
    if (t) { char buf[32]; snprintf(buf, sizeof(buf), "%02d/%02d/%02d %02d:%02d", t->tm_mday, t->tm_mon+1, t->tm_year%100, t->tm_hour, t->tm_min); ultimaHoraGuardado = buf; }
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
    } else if (tipoMuerta == 4) {
        if (vL=='a') return isUpper?"Â":"â"; if (vL=='e') return isUpper?"Ê":"ê"; if (vL=='i') return isUpper?"Î":"î";
        if (vL=='o') return isUpper?"Ô":"ô"; if (vL=='u') return isUpper?"Û":"û";
    } else if (tipoMuerta == 5) {
        if (vL=='n') return isUpper?"Ñ":"ñ";
    }
    std::string res = (tipoMuerta == 1 ? "´" : (tipoMuerta == 2 ? "¨" : (tipoMuerta == 3 ? "`" : (tipoMuerta == 4 ? "^" : "~")))); res += vocal; return res;
}

void runEditorLoop() {
    C3D_RenderTarget* topScreen = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottomScreen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    C2D_TextBuf dynamicBuf = C2D_TextBufNew(65536);

    // --- PANTALLA DE BIENVENIDA (SPLASH SCREEN CON FADE) ---
    {
        C2D_SpriteSheet splashSheet = C2D_SpriteSheetLoad("romfs:/splash.t3x");
        C2D_Image splashImg = {0};
        bool hasSplash = (splashSheet != NULL);
        if (hasSplash) splashImg = C2D_SpriteSheetGetImage(splashSheet, 0);

        bool splashActive = true;
        int fadeAlpha = 255;
        bool fadingIn = true;
        bool fadingOut = false;

        while (aptMainLoop() && splashActive) {
            hidScanInput();
            u32 kDown = hidKeysDown();
            touchPosition touch; hidTouchRead(&touch);

            if (!fadingOut && (kDown || (kDown & KEY_TOUCH))) {
                fadingOut = true;
            }

            if (fadingIn) {
                fadeAlpha -= 12;
                if (fadeAlpha <= 0) { fadeAlpha = 0; fadingIn = false; }
            } else if (fadingOut) {
                fadeAlpha += 15;
                if (fadeAlpha >= 255) { fadeAlpha = 255; splashActive = false; }
            }

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            
            C2D_TargetClear(topScreen, COLOR_BLANCO);
            C2D_SceneBegin(topScreen);

            if (hasSplash) {
                C2D_DrawImageAt(splashImg, (400.0f - 96.0f) / 2.0f, 20.0f, 0.5f, NULL, 1.0f, 1.0f);
            }

            C2D_Text tTitle, tSub1, tSub2, tCopy;
            C2D_TextParse(&tTitle, dynamicBuf, "Laura"); C2D_TextOptimize(&tTitle);
            C2D_TextParse(&tSub1, dynamicBuf, "Procesador de textos para la Nintendo 3DS"); C2D_TextOptimize(&tSub1);
            C2D_TextParse(&tSub2, dynamicBuf, "Word processor for the Nintendo 3DS"); C2D_TextOptimize(&tSub2);
            C2D_TextParse(&tCopy, dynamicBuf, "© Laura Dev 2026"); C2D_TextOptimize(&tCopy);

            float tw=0, th=0;
            C2D_TextGetDimensions(&tTitle, 1.2f, 1.2f, &tw, &th);
            C2D_DrawText(&tTitle, C2D_WithColor, (400.0f - tw)/2.0f, 125.0f, 0.6f, 1.2f, 1.2f, COLOR_NEGRO);

            C2D_TextGetDimensions(&tSub1, 0.4f, 0.4f, &tw, &th);
            C2D_DrawText(&tSub1, C2D_WithColor, (400.0f - tw)/2.0f, 165.0f, 0.6f, 0.4f, 0.4f, COLOR_NEGRO);

            C2D_TextGetDimensions(&tSub2, 0.4f, 0.4f, &tw, &th);
            C2D_DrawText(&tSub2, C2D_WithColor, (400.0f - tw)/2.0f, 185.0f, 0.6f, 0.4f, 0.4f, COLOR_NEGRO);

            C2D_TextGetDimensions(&tCopy, 0.32f, 0.32f, &tw, &th);
            C2D_DrawText(&tCopy, C2D_WithColor, (400.0f - tw)/2.0f, 215.0f, 0.6f, 0.32f, 0.32f, C2D_Color32(120, 120, 120, 255));

            if (fadeAlpha > 0) {
                C2D_DrawRectSolid(0, 0, 0.9f, 400, 240, C2D_Color32(0, 0, 0, fadeAlpha));
            }

            C2D_TargetClear(bottomScreen, COLOR_BLANCO);
            C2D_SceneBegin(bottomScreen);

            if (!fadingOut && (osGetTime() / 500) % 2 == 0) {
                C2D_Text tPrompt;
                C2D_TextParse(&tPrompt, dynamicBuf, "Toca la pantalla para continuar"); C2D_TextOptimize(&tPrompt);
                C2D_TextGetDimensions(&tPrompt, 0.45f, 0.45f, &tw, &th);
                C2D_DrawText(&tPrompt, C2D_WithColor, (320.0f - tw)/2.0f, 110.0f, 0.6f, 0.45f, 0.45f, COLOR_MUSGO);
            }

            if (fadeAlpha > 0) {
                C2D_DrawRectSolid(0, 0, 0.9f, 320, 240, C2D_Color32(0, 0, 0, fadeAlpha));
            }

            C3D_FrameEnd(0);
        }
        if (splashSheet) C2D_SpriteSheetFree(splashSheet);
        C2D_TextBufClear(dynamicBuf);
    }

    // --- TECLADO Y DISEÑO ---
    std::vector<KeyButton> keys; 
    float btnW = 29.5f, btnH = 31.0f, gap = 2.0f;
    float row0 = 4.0f, row1 = 30.0f, row2 = 64.0f, row3 = 98.0f, row4 = 132.0f, row5 = 166.0f, row6 = 202.0f;

    keys.push_back({4.0f, row0, 42.0f, 22.0f, "N", "N", "N", 5});
    keys.push_back({48.0f, row0, 42.0f, 22.0f, "K", "K", "K", 5});
    keys.push_back({92.0f, row0, 42.0f, 22.0f, "S", "S", "S", 5});
    keys.push_back({136.0f, row0, 42.0f, 22.0f, "ALIN", "ALIN", "ALIN", 6}); 
    keys.push_back({180.0f, row0, 42.0f, 22.0f, "Aa", "Aa", "Aa", 7}); 
    keys.push_back({224.0f, row0, 42.0f, 22.0f, "¶", "¶", "¶", 9}); 
    keys.push_back({268.0f, row0, 48.0f, 22.0f, "<-", "<-", "<-", 10}); 

    std::string r1_b[10] = {"1","2","3","4","5","6","7","8","9","0"}; 
    std::string r1_s[10] = {"*","\"","·","$","%","&","/","<",">","="}; 
    std::string r1_c[10] = {"|","@","#","ª","+","¬","\\","^","]","º"};
    for (int i = 0; i < 10; i++) keys.push_back({8.0f + i * (btnW + gap), row1, btnW, 23.0f, r1_b[i], r1_s[i], r1_c[i], 0}); 
    
    std::string r2_b[10] = {"q","w","e","r","t","y","u","i","o","p"}; 
    std::string r2_s[10] = {"Q","W","E","R","T","Y","U","I","O","P"};
    for (int i = 0; i < 10; i++) keys.push_back({8.0f + i * (btnW + gap), row2, btnW, btnH, r2_b[i], r2_s[i], "", 0});
    
    std::string r3_b[10] = {"a","s","d","f","g","h","j","k","l","ñ"}; 
    std::string r3_s[10] = {"A","S","D","F","G","H","J","K","L","Ñ"}; 
    std::string r3_c[10] = {"","","","","","","","","","~"};
    for (int i = 0; i < 10; i++) keys.push_back({8.0f + i * (btnW + gap), row3, btnW, btnH, r3_b[i], r3_s[i], r3_c[i], 0});
    
    std::string r4_b[10] = {"z","x","c","v","b","n","m",",",".","´"}; 
    std::string r4_s[10] = {"Z","X","C","V","B","N","M",";",":","¨"}; 
    std::string r4_c[10] = {"","","ç","","","","","","","`"};
    for (int i = 0; i < 9; i++) keys.push_back({8.0f + i * (btnW + gap), row4, btnW, btnH, r4_b[i], r4_s[i], r4_c[i], 0});
    keys.push_back({8.0f + 9 * (btnW + gap), row4, btnW, btnH, r4_b[9], r4_s[9], r4_c[9], 4});
    
    std::string puncs_b[10] = {"«","¿","¡","(","-","‘",")","!","?","»"}; 
    std::string puncs_s[10] = {"«","¿","¡","[","–","“","]","!","?","»"};
    std::string puncs_c[10] = {"","","","{","","","}","","",""};
    for (int i = 0; i < 10; i++) keys.push_back({8.0f + i * (btnW + gap), row5, btnW, btnH, puncs_b[i], puncs_s[i], puncs_c[i], 0});
    
    keys.push_back({8.0f, row6, 304.0f, 28.0f, "ESPACIO", "ESPACIO", "ESPACIO", 2}); 

    std::vector<StyledChar> documento; size_t cursorPos = 0;
    bool hasSelection = false; size_t selAnchor = 0; bool isSelectModeToggle = false;
    std::vector<StyledChar> portapapeles;
    std::string ultimaHoraGuardado = "--:--"; std::string fileName = "Documento1";
    std::vector<EditorState> undoStack; std::vector<EditorState> redoStack;

    CharStyle activeStyle; activeStyle.fontSize = 24;
    int estadoTeclaMuerta = 0; bool capsLock = false; u64 lastLPressTick = 0;
    bool showTopInfoBar = true; bool showPilcrow = false;
    int menuAbierto = 0; 
    bool flagSalir = false;
    std::vector<std::string> fileList; int fileScroll = 0;
    
    int startMenuSel = -1;
    int formatMenuRow = 0, formatMenuCol = 0; 
    int openMenuSel = -1;
    int confirmSel = -1;
    int manualMenuSel = -1;

    const int INITIAL_DELAY = 15; const int REPEAT_RATE = 4;
    int bsCooldown = 0, touchCooldown = 0, activeTouchKeyIdx = -1, dpadCooldown = 0, undoCooldown = 0;

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
        if (g == "\t") return 20.0f;
        C2D_Text t; C2D_TextParse(&t, dynamicBuf, g.c_str());
        float gw=0, gh=0; float scale = size / 48.0f; C2D_TextGetDimensions(&t, scale, scale, &gw, &gh);
        return (gw < 3.0f) ? (5.0f * scale * 2.0f) : gw;
    };

    while (aptMainLoop() && !flagSalir) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        
        bool isLHeld = (kHeld & KEY_L);
        bool isZLHeld = (kHeld & KEY_ZL);
        bool isZRHeld = (kHeld & KEY_ZR); 

        if (kDown & KEY_START) { menuAbierto = 2; startMenuSel = -1; }
        if (kDown & KEY_X && menuAbierto == 0) showTopInfoBar = !showTopInfoBar;

        if (kDown & KEY_L) {
            u64 cTick = osGetTime();
            if (cTick - lastLPressTick < 280) { capsLock = !capsLock; lastLPressTick = 0; } else lastLPressTick = cTick;
        }

        if (kDown & KEY_SELECT && menuAbierto == 0) {
            isSelectModeToggle = !isSelectModeToggle;
            if (isSelectModeToggle) { hasSelection = true; selAnchor = cursorPos; } else hasSelection = false;
        }

        bool triggerDpad = false; u32 dpadMask = KEY_DLEFT | KEY_DRIGHT | KEY_DUP | KEY_DDOWN;
        if (kDown & dpadMask) { triggerDpad = true; dpadCooldown = INITIAL_DELAY; }
        else if (kHeld & dpadMask) { if (dpadCooldown > 0) dpadCooldown--; else { triggerDpad = true; dpadCooldown = REPEAT_RATE; } } else dpadCooldown = 0;

        if (triggerDpad) {
            if (menuAbierto == 1) { 
                if (kHeld & KEY_DUP) formatMenuRow = std::max(0, formatMenuRow - 1);
                if (kHeld & KEY_DDOWN) formatMenuRow = std::min(3, formatMenuRow + 1);
                if (kHeld & KEY_DLEFT) {
                    if (formatMenuRow == 0) formatMenuCol = std::max(0, formatMenuCol - 1);
                    else if (formatMenuRow == 3) formatMenuCol = 0; 
                    else formatMenuCol = std::max(0, formatMenuCol - 1);
                }
                if (kHeld & KEY_DRIGHT) {
                    if (formatMenuRow == 0) formatMenuCol = std::min(1, formatMenuCol + 1);
                    else if (formatMenuRow == 3) formatMenuCol = 0;
                    else formatMenuCol = std::min(7, formatMenuCol + 1);
                }
            } else if (menuAbierto == 2) { 
                if (startMenuSel == -1) startMenuSel = 0;
                else {
                    if (kHeld & KEY_DUP) startMenuSel = std::max(0, startMenuSel - 1);
                    if (kHeld & KEY_DDOWN) startMenuSel = std::min(4, startMenuSel + 1); // 0 al 4 (Cruz X excluida del teclado)
                }
            } else if (menuAbierto == 3 || menuAbierto == 4) { 
                if (manualMenuSel == -1) manualMenuSel = 0;
                else {
                    if (kHeld & KEY_DLEFT || kHeld & KEY_DUP) manualMenuSel = std::max(0, manualMenuSel - 1);
                    if (kHeld & KEY_DRIGHT || kHeld & KEY_DDOWN) manualMenuSel = std::min(menuAbierto == 3 ? 0 : 1, manualMenuSel + 1); // Excluye cruz X
                }
            } else if (menuAbierto == 5) { 
                if (openMenuSel == -1) openMenuSel = 0;
                else {
                    if (kHeld & KEY_DUP) { if (openMenuSel > 0) openMenuSel--; else if (fileScroll > 0) fileScroll--; }
                    if (kHeld & KEY_DDOWN) { if (openMenuSel < 4 && openMenuSel < (int)fileList.size()-1) openMenuSel++; else if (fileScroll + openMenuSel < (int)fileList.size() - 1) fileScroll++; }
                }
            } else if (menuAbierto == 6 || menuAbierto == 7) { 
                if (confirmSel == -1) confirmSel = 0;
                else {
                    if (kHeld & KEY_DLEFT || kHeld & KEY_DUP) confirmSel = std::max(0, confirmSel - 1);
                    if (kHeld & KEY_DRIGHT || kHeld & KEY_DDOWN) confirmSel = std::min(1, confirmSel + 1); // Excluye cruz X
                }
            } else if (menuAbierto == 0) {
                bool sel = (kHeld & KEY_SELECT) || isSelectModeToggle;
                if (!sel) hasSelection = false; else if (!hasSelection) { hasSelection = true; selAnchor = cursorPos; }
                if (kHeld & KEY_DLEFT) { if (cursorPos > 0) cursorPos--; }
                if (kHeld & KEY_DRIGHT) { if (cursorPos < documento.size()) cursorPos++; }
                if (kHeld & KEY_DUP) { int steps = 40; while(cursorPos > 0 && steps-- > 0) cursorPos--; }
                if (kHeld & KEY_DDOWN) { int steps = 40; while(cursorPos < documento.size() && steps-- > 0) cursorPos++; }
            }
        }

        if (kDown & KEY_A) {
            if (menuAbierto == 1) {
                if (formatMenuRow == 0) {
                    if (formatMenuCol == 0) activeStyle.fontSize = std::max(12, activeStyle.fontSize - 2);
                    else activeStyle.fontSize = std::min(72, activeStyle.fontSize + 2);
                } else if (formatMenuRow == 1) { activeStyle.fgColor = formatMenuCol; } 
                else if (formatMenuRow == 2) { activeStyle.bgColor = formatMenuCol; if (formatMenuCol != 1) activeStyle.fgColor = 1; }
                else if (formatMenuRow == 3) { activeStyle.lineSpacing = (activeStyle.lineSpacing + 1) % 4; }
                
                if (hasSelection) {
                    pushUndo(); size_t s = std::min(selAnchor, cursorPos), e = std::max(selAnchor, cursorPos);
                    for (size_t idx = s; idx < e; idx++) { 
                        documento[idx].style.fontSize = activeStyle.fontSize; documento[idx].style.fgColor = activeStyle.fgColor; 
                        documento[idx].style.bgColor = activeStyle.bgColor; documento[idx].style.lineSpacing = activeStyle.lineSpacing; 
                    }
                }
            } else if (menuAbierto == 2) {
                if (startMenuSel == 0) { guardarEnRTF(documento, fileName, ultimaHoraGuardado); menuAbierto = 0; }
                else if (startMenuSel == 1) {
                    SwkbdState swkbd; char mybuf[256]; swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 1, -1);
                    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0); swkbdSetHintText(&swkbd, "Nombre del archivo"); swkbdSetInitialText(&swkbd, fileName.c_str());
                    if (swkbdInputText(&swkbd, mybuf, sizeof(mybuf)) == SWKBD_BUTTON_CONFIRM) { fileName = mybuf; guardarEnRTF(documento, fileName, ultimaHoraGuardado); }
                    menuAbierto = 0;
                } else if (startMenuSel == 2) {
                    fileList.clear(); DIR *dir = opendir("sdmc:/");
                    if (dir) { struct dirent *ent; while ((ent = readdir(dir)) != NULL) { std::string fn = ent->d_name; if (fn.size() > 4 && fn.substr(fn.size()-4) == ".rtf") fileList.push_back(fn); } closedir(dir); }
                    menuAbierto = 5; fileScroll = 0; openMenuSel = 0;
                } else if (startMenuSel == 3) { menuAbierto = 3; manualMenuSel = -1; }
                else if (startMenuSel == 4) { menuAbierto = 7; confirmSel = -1; } 
            } else if (menuAbierto == 3) {
                if (manualMenuSel == 0) { menuAbierto = 4; manualMenuSel = -1; } 
            } else if (menuAbierto == 4) {
                if (manualMenuSel == 0) { menuAbierto = 3; manualMenuSel = -1; } 
                else if (manualMenuSel == 1) { menuAbierto = 0; } 
            } else if (menuAbierto == 5) {
                if (fileScroll + openMenuSel < (int)fileList.size()) {
                    menuAbierto = 6; confirmSel = -1; 
                }
            } else if (menuAbierto == 6) { 
                if (confirmSel == 0) { 
                    fileName = fileList[fileScroll + openMenuSel]; fileName = fileName.substr(0, fileName.find_last_of('.'));
                    std::string path = "sdmc:/" + fileName + ".rtf"; FILE* f = fopen(path.c_str(), "r");
                    if (f) {
                        char b[512]; std::string raw = ""; while(fgets(b, sizeof(b), f)) raw += b; fclose(f);
                        documento.clear(); size_t textStart = raw.find("\\pard");
                        if (textStart != std::string::npos) {
                            size_t sIdx = raw.find(" ", textStart);
                            if (sIdx != std::string::npos) {
                                std::string pure = raw.substr(sIdx + 1);
                                size_t pos = 0; while((pos = pure.find("\\par\n", pos)) != std::string::npos) { pure.replace(pos, 5, "\n"); pos += 1; }
                                pos = 0; while((pos = pure.find("\\tab ", pos)) != std::string::npos) { pure.replace(pos, 5, "\t"); pos += 1; }
                                std::string finalT = ""; bool inTag = false;
                                for(char c : pure) {
                                    if (c == '\\') inTag = true; else if (inTag && (c == ' ' || c == '\n')) inTag = false; else if (!inTag && c != '}' && c != '{') finalT += c;
                                }
                                documento = stringToStyled(finalT, activeStyle);
                            }
                        }
                        cursorPos = documento.size();
                    }
                    menuAbierto = 0;
                } else if (confirmSel == 1) {
                    menuAbierto = 2; // Vuelve al menú de inicio al pulsar NO
                }
            } else if (menuAbierto == 7) { 
                if (confirmSel == 0) flagSalir = true; 
                else if (confirmSel == 1) menuAbierto = 2; // Vuelve al menú de inicio al pulsar NO
            } else if (menuAbierto == 0 && !portapapeles.empty()) {
                pushUndo(); deleteSelection(); std::vector<StyledChar> aPegar = portapapeles;
                if (isLHeld || isZLHeld) { for (auto& sc : aPegar) sc.style = activeStyle; } 
                documento.insert(documento.begin() + cursorPos, aPegar.begin(), aPegar.end()); cursorPos += aPegar.size(); isSelectModeToggle = false;
            }
        }

        bool triggerB = false;
        if (kDown & KEY_B) { triggerB = true; undoCooldown = INITIAL_DELAY; }
        else if (kHeld & KEY_B) { if (undoCooldown > 0) undoCooldown--; else { triggerB = true; undoCooldown = REPEAT_RATE; } } else undoCooldown = 0;

        if (triggerB) {
            if (menuAbierto != 0) {
                // Al cerrar cualquier submenú o modal, regresa de forma natural al menú de inicio (2) o se cierra
                if (menuAbierto == 1) menuAbierto = 0;
                else if (menuAbierto == 3 || menuAbierto == 4 || menuAbierto == 5 || menuAbierto == 7) menuAbierto = 2;
                else if (menuAbierto == 6) menuAbierto = 5; // De confirmación de abrir regresa a lista abrir
                else menuAbierto = 0; 
            } else {
                hasSelection = false; isSelectModeToggle = false;
                if (isZLHeld) {
                    if (!redoStack.empty()) {
                        undoStack.push_back({documento, cursorPos}); EditorState st = redoStack.back(); redoStack.pop_back();
                        documento = st.doc; cursorPos = std::min(st.cursor, documento.size());
                    }
                } else if (isLHeld) {
                    for (int step = 0; step < 5 && !undoStack.empty(); step++) {
                        redoStack.push_back({documento, cursorPos}); EditorState st = undoStack.back(); redoStack.pop_back();
                        documento = st.doc; cursorPos = std::min(st.cursor, documento.size());
                    }
                } else {
                    if (!undoStack.empty()) {
                        redoStack.push_back({documento, cursorPos}); EditorState st = undoStack.back(); redoStack.pop_back();
                        documento = st.doc; cursorPos = std::min(st.cursor, documento.size());
                    }
                }
            }
        }

        if (menuAbierto == 0) {
            if (kDown & KEY_Y) { 
                if (hasSelection && selAnchor != cursorPos) {
                    size_t start = std::min(selAnchor, cursorPos), end = std::max(selAnchor, cursorPos);
                    portapapeles.assign(documento.begin() + start, documento.begin() + end);
                    if (isLHeld || isZLHeld) { pushUndo(); documento.erase(documento.begin() + start, documento.begin() + end); cursorPos = start; }
                    hasSelection = false; isSelectModeToggle = false;
                }
            }
            if (kDown & KEY_R) { 
                pushUndo(); deleteSelection();
                if (isZLHeld || isLHeld) {
                    auto styledIns = stringToStyled("\n--- [PAGINA NUEVA] ---\n", activeStyle);
                    documento.insert(documento.begin() + cursorPos, styledIns.begin(), styledIns.end()); cursorPos += styledIns.size();
                } else {
                    documento.insert(documento.begin() + cursorPos, {"\n", activeStyle}); cursorPos++;
                }
            }
        }

        bool isBs = false;
        if (kDown & KEY_ZR) { isBs = true; bsCooldown = INITIAL_DELAY; }
        else if (kHeld & KEY_ZR) { if (bsCooldown > 0) bsCooldown--; else { isBs = true; bsCooldown = REPEAT_RATE; } } else bsCooldown = 0;
        
        if (isBs && menuAbierto == 0) {
            if (hasSelection) { pushUndo(); deleteSelection(); } 
            else if (cursorPos > 0) {
                pushUndo();
                if (isZLHeld) { 
                    size_t target = cursorPos; while (target > 0 && documento[target - 1].glyph != "\n") target--;
                    if (target == cursorPos && cursorPos > 0) target--;
                    documento.erase(documento.begin() + target, documento.begin() + cursorPos); cursorPos = target;
                } else if (isLHeld) { 
                    while (cursorPos > 0 && (documento[cursorPos - 1].glyph == " " || documento[cursorPos - 1].glyph == "\n")) { documento.erase(documento.begin() + (cursorPos - 1)); cursorPos--; }
                    while (cursorPos > 0 && documento[cursorPos - 1].glyph != " " && documento[cursorPos - 1].glyph != "\n") { documento.erase(documento.begin() + (cursorPos - 1)); cursorPos--; }
                } else {
                    documento.erase(documento.begin() + (cursorPos - 1)); cursorPos--;
                }
            }
        }

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
                if (menuAbierto == 1) { 
                    if (touch.px >= 275 && touch.px <= 300 && touch.py >= 12 && touch.py <= 37) menuAbierto = 0; 
                    if (touch.px >= 15 && touch.px <= 55 && touch.py >= 60 && touch.py <= 88) { activeStyle.fontSize = std::max(12, activeStyle.fontSize - 2); formatMenuRow = 0; formatMenuCol = 0; }
                    if (touch.px >= 135 && touch.px <= 175 && touch.py >= 60 && touch.py <= 88) { activeStyle.fontSize = std::min(72, activeStyle.fontSize + 2); formatMenuRow = 0; formatMenuCol = 1; }
                    if (touch.px >= 195 && touch.px <= 295 && touch.py >= 60 && touch.py <= 88) { activeStyle.lineSpacing = (activeStyle.lineSpacing + 1) % 4; formatMenuRow = 3; formatMenuCol = 0; }
                    
                    for (int c = 0; c <= 7; c++) {
                        if (touch.px >= 15 + (c * 34) && touch.px <= 15 + (c * 34) + 26 && touch.py >= 130 && touch.py <= 156) { activeStyle.fgColor = c; formatMenuRow = 1; formatMenuCol = c; }
                        if (touch.px >= 15 + (c * 34) && touch.px <= 15 + (c * 34) + 26 && touch.py >= 190 && touch.py <= 216) {
                            activeStyle.bgColor = c; formatMenuRow = 2; formatMenuCol = c; if (c != 1) activeStyle.fgColor = 1; 
                        }
                    }
                    if (hasSelection) {
                        pushUndo(); size_t s = std::min(selAnchor, cursorPos), e = std::max(selAnchor, cursorPos);
                        for (size_t idx = s; idx < e; idx++) { 
                            documento[idx].style.fontSize = activeStyle.fontSize; documento[idx].style.fgColor = activeStyle.fgColor; 
                            documento[idx].style.bgColor = activeStyle.bgColor; documento[idx].style.lineSpacing = activeStyle.lineSpacing; 
                        }
                    }
                } else if (menuAbierto == 2) { 
                    if (touch.px >= 275 && touch.px <= 300 && touch.py >= 12 && touch.py <= 37) { menuAbierto = 0; } 
                    else if (touch.px >= 60 && touch.px <= 260) {
                        if (touch.py >= 50 && touch.py <= 75) { guardarEnRTF(documento, fileName, ultimaHoraGuardado); menuAbierto = 0; } 
                        if (touch.py >= 85 && touch.py <= 110) { 
                            SwkbdState swkbd; char mybuf[256]; swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 1, -1);
                            swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0); swkbdSetHintText(&swkbd, "Nombre del archivo"); swkbdSetInitialText(&swkbd, fileName.c_str());
                            if (swkbdInputText(&swkbd, mybuf, sizeof(mybuf)) == SWKBD_BUTTON_CONFIRM) { fileName = mybuf; guardarEnRTF(documento, fileName, ultimaHoraGuardado); }
                            menuAbierto = 0; 
                        }
                        if (touch.py >= 120 && touch.py <= 145) { 
                            fileList.clear(); DIR *dir = opendir("sdmc:/");
                            if (dir) { struct dirent *ent; while ((ent = readdir(dir)) != NULL) { std::string fn = ent->d_name; if (fn.size() > 4 && fn.substr(fn.size()-4) == ".rtf") fileList.push_back(fn); } closedir(dir); }
                            menuAbierto = 5; fileScroll = 0; openMenuSel = 0;
                        }
                        if (touch.py >= 155 && touch.py <= 180) { menuAbierto = 3; manualMenuSel = -1; } 
                        if (touch.py >= 190 && touch.py <= 215) { menuAbierto = 7; confirmSel = -1; } 
                    }
                } else if (menuAbierto == 3) { 
                    if (touch.px >= 275 && touch.px <= 300 && touch.py >= 12 && touch.py <= 37) { menuAbierto = 2; }
                    else if (touch.px >= 180 && touch.px <= 290 && touch.py >= 190 && touch.py <= 215) { menuAbierto = 4; manualMenuSel = -1; }
                } else if (menuAbierto == 4) {
                    if (touch.px >= 275 && touch.px <= 300 && touch.py >= 12 && touch.py <= 37) { menuAbierto = 2; }
                    else if (touch.px >= 20 && touch.px <= 120 && touch.py >= 190 && touch.py <= 215) { menuAbierto = 3; manualMenuSel = -1; }
                    else if (touch.px >= 140 && touch.px <= 250 && touch.py >= 190 && touch.py <= 215) { menuAbierto = 2; }
                } else if (menuAbierto == 5) { 
                    if (touch.px >= 275 && touch.px <= 300 && touch.py >= 12 && touch.py <= 37) { menuAbierto = 2; }
                    for (int i = 0; i < 5 && (fileScroll + i) < (int)fileList.size(); i++) {
                        if (touch.px >= 20 && touch.px <= 280 && touch.py >= 50 + (i*30) && touch.py <= 75 + (i*30)) {
                            openMenuSel = i;
                            menuAbierto = 6; confirmSel = -1;
                        }
                    }
                } else if (menuAbierto == 6) { 
                    if (touch.px >= 70 && touch.px <= 140 && touch.py >= 135 && touch.py <= 160) {
                        // Acción SÍ (Abrir archivo)
                        fileName = fileList[fileScroll + openMenuSel]; fileName = fileName.substr(0, fileName.find_last_of('.'));
                        std::string path = "sdmc:/" + fileName + ".rtf"; FILE* f = fopen(path.c_str(), "r");
                        if (f) {
                            char b[512]; std::string raw = ""; while(fgets(b, sizeof(b), f)) raw += b; fclose(f);
                            documento.clear(); size_t textStart = raw.find("\\pard");
                            if (textStart != std::string::npos) {
                                size_t sIdx = raw.find(" ", textStart);
                                if (sIdx != std::string::npos) {
                                    std::string pure = raw.substr(sIdx + 1);
                                    size_t pos = 0; while((pos = pure.find("\\par\n", pos)) != std::string::npos) { pure.replace(pos, 5, "\n"); pos += 1; }
                                    pos = 0; while((pos = pure.find("\\tab ", pos)) != std::string::npos) { pure.replace(pos, 5, "\t"); pos += 1; }
                                    std::string finalT = ""; bool inTag = false;
                                    for(char c : pure) {
                                        if (c == '\\') inTag = true; else if (inTag && (c == ' ' || c == '\n')) inTag = false; else if (!inTag && c != '}' && c != '{') finalT += c;
                                    }
                                    documento = stringToStyled(finalT, activeStyle);
                                }
                            }
                            cursorPos = documento.size();
                        }
                        menuAbierto = 0;
                    }
                    if (touch.px >= 180 && touch.px <= 250 && touch.py >= 135 && touch.py <= 160) {
                        menuAbierto = 2; // Regresa al menú de inicio al pulsar NO
                    }
                    if (touch.px >= 255 && touch.px <= 280 && touch.py >= 52 && touch.py <= 77) {
                        menuAbierto = 5; // Cruz X regresa a lista abrir
                    }
                } else if (menuAbierto == 7) { 
                    if (touch.px >= 70 && touch.px <= 140 && touch.py >= 135 && touch.py <= 160) {
                        flagSalir = true; // Acción SÍ (Salir)
                    }
                    if (touch.px >= 180 && touch.px <= 250 && touch.py >= 135 && touch.py <= 160) {
                        menuAbierto = 2; // Regresa al menú de inicio al pulsar NO
                    }
                    if (touch.px >= 255 && touch.px <= 280 && touch.py >= 52 && touch.py <= 77) {
                        menuAbierto = 2; // Cruz X regresa al menú de inicio
                    }
                }
            }
        } else {
            if (touchJustPressed) { activeTouchKeyIdx = touchedIdx; triggerTouchKey = (touchedIdx != -1); touchCooldown = INITIAL_DELAY; }
            else if ((kHeld & KEY_TOUCH) && touchedIdx != -1 && touchedIdx == activeTouchKeyIdx) {
                if (keys[touchedIdx].type == 0 || keys[touchedIdx].type == 2 || keys[touchedIdx].type == 10) { if (touchCooldown > 0) touchCooldown--; else { triggerTouchKey = true; touchCooldown = REPEAT_RATE; } }
            } else { activeTouchKeyIdx = -1; touchCooldown = 0; }
        }

        if (triggerTouchKey && activeTouchKeyIdx != -1 && menuAbierto == 0) {
            const auto& key = keys[activeTouchKeyIdx];
            
            if (key.type == 7) { 
                if (cursorPos > 0 && !hasSelection) activeStyle = documento[cursorPos - 1].style;
                menuAbierto = 1; formatMenuRow = 0; formatMenuCol = 0;
            }
            else if (key.type == 6) { 
                activeStyle.align = (activeStyle.align + 1) % 4; 
                if (hasSelection && selAnchor != cursorPos) {
                    pushUndo(); size_t s = std::min(selAnchor, cursorPos), e = std::max(selAnchor, cursorPos);
                    for (size_t idx = s; idx < e; idx++) documento[idx].style.align = activeStyle.align;
                } else if (!documento.empty() && cursorPos > 0) {
                    pushUndo(); size_t p = cursorPos - 1; while (p > 0 && documento[p].glyph != "\n") p--;
                    size_t endP = cursorPos; while (endP < documento.size() && documento[endP].glyph != "\n") endP++;
                    for (size_t idx = p; idx < endP; idx++) documento[idx].style.align = activeStyle.align;
                }
            } 
            else if (key.type == 9) { showPilcrow = !showPilcrow; } 
            else if (key.type == 10) { 
                if (hasSelection) { pushUndo(); deleteSelection(); } 
                else if (cursorPos > 0) {
                    pushUndo();
                    if (isZLHeld) { 
                        size_t target = cursorPos; while (target > 0 && documento[target - 1].glyph != "\n") target--;
                        if (target == cursorPos && cursorPos > 0) target--;
                        documento.erase(documento.begin() + target, documento.begin() + cursorPos); cursorPos = target;
                    } else if (isLHeld) { 
                        while (cursorPos > 0 && (documento[cursorPos - 1].glyph == " " || documento[cursorPos - 1].glyph == "\n")) { documento.erase(documento.begin() + (cursorPos - 1)); cursorPos--; }
                        while (cursorPos > 0 && documento[cursorPos - 1].glyph != " " && documento[cursorPos - 1].glyph != "\n") { documento.erase(documento.begin() + (cursorPos - 1)); cursorPos--; }
                    } else {
                        documento.erase(documento.begin() + (cursorPos - 1)); cursorPos--;
                    }
                }
            }
            else if (key.type == 5) {
                if (key.base == "N") activeStyle.bold = !activeStyle.bold;
                else if (key.base == "K") { activeStyle.italic = !activeStyle.italic; } 
                else if (key.base == "S") {
                    if (isZLHeld) { activeStyle.underline = true; activeStyle.strike = true; }
                    else if (isLHeld) { activeStyle.strike = !activeStyle.strike; }
                    else { 
                        if (activeStyle.underline || activeStyle.strike) { activeStyle.underline = false; activeStyle.strike = false; }
                        else { activeStyle.underline = true; }
                    }
                }
            } else if (key.type == 4) {
                if (isZLHeld) estadoTeclaMuerta = 3; else if (isLHeld) estadoTeclaMuerta = 2; else estadoTeclaMuerta = 1;
            } else if (key.type == 0 || key.type == 2) {
                pushUndo(); deleteSelection();
                std::string charToAdd = key.base;
                if (key.type == 2) {
                    if (isLHeld) charToAdd = "\t";
                    else if (isZLHeld || isZRHeld) {
                        activeStyle.indent++;
                        size_t p = cursorPos > 0 ? cursorPos - 1 : 0; while (p > 0 && documento[p].glyph != "\n") p--;
                        size_t endP = cursorPos; while (endP < documento.size() && documento[endP].glyph != "\n") endP++;
                        for (size_t idx = p; idx < endP; idx++) documento[idx].style.indent = activeStyle.indent;
                        charToAdd = "";
                    } else { charToAdd = " "; }
                } else {
                    bool isUpper = capsLock; if (key.base.size()==1 && key.base[0]>='a' && key.base[0]<='z') isUpper = (isUpper != isLHeld);
                    
                    if (isZLHeld && !key.ctrlAlt.empty()) {
                        if (key.ctrlAlt == "^") estadoTeclaMuerta = 4;
                        else if (key.ctrlAlt == "~") estadoTeclaMuerta = 5;
                        else charToAdd = key.ctrlAlt; 
                    }
                    else if ((isLHeld || isUpper) && !key.shift.empty()) {
                        if (key.shift == "´") estadoTeclaMuerta = 1;
                        else if (key.shift == "¨") estadoTeclaMuerta = 2;
                        else charToAdd = key.shift;
                    }

                    if (estadoTeclaMuerta > 0 && charToAdd.length() == 1 && ((charToAdd[0]>='a' && charToAdd[0]<='z') || (charToAdd[0]>='A' && charToAdd[0]<='Z'))) {
                        charToAdd = aplicarTeclaMuerta(charToAdd[0], estadoTeclaMuerta); estadoTeclaMuerta = 0;
                    } else if (estadoTeclaMuerta > 0) {
                        std::string dead = (estadoTeclaMuerta == 1 ? "´" : (estadoTeclaMuerta == 2 ? "¨" : (estadoTeclaMuerta == 3 ? "`" : (estadoTeclaMuerta == 4 ? "^" : "~"))));
                        documento.insert(documento.begin() + cursorPos, {dead, activeStyle}); cursorPos++; estadoTeclaMuerta = 0;
                    }
                }
                if (charToAdd != "") { documento.insert(documento.begin() + cursorPos, {charToAdd, activeStyle}); cursorPos++; }
            }
        }

        struct LineSeg { size_t sIdx, eIdx; float w; int align; int spacing; float maxH; int ind; };
        std::vector<LineSeg> renderLines; float curLW = 0; size_t lStart = 0; float maxLH = 16.5f;

        for (size_t i = 0; i < documento.size(); i++) {
            float gw = measureChar(documento[i].glyph, documento[i].style.fontSize);
            float currentLineHeight = 33.0f * (documento[i].style.fontSize / 48.0f);
            if (currentLineHeight > maxLH) maxLH = currentLineHeight;
            bool forceBreak = (documento[i].glyph == "\n"); bool softBreak = false;
            
            float effW = curLW + (curLW == 0 ? (documento[i].style.indent * 20.0f) : 0);

            if (documento[i].glyph == " " || documento[i].glyph == "\t") {
                float nextWordW = 0;
                for (size_t j = i + 1; j < documento.size() && documento[j].glyph != " " && documento[j].glyph != "\n" && documento[j].glyph != "\t"; j++) {
                    nextWordW += measureChar(documento[j].glyph, documento[j].style.fontSize);
                }
                if (effW + gw + nextWordW > 380.0f && curLW > 0) softBreak = true;
            } else if (effW + gw > 380.0f && curLW > 0) softBreak = true;

            if (forceBreak || softBreak) {
                if (forceBreak) curLW += gw; 
                renderLines.push_back({lStart, i, curLW, documento[lStart].style.align, documento[lStart].style.lineSpacing, maxLH, documento[lStart].style.indent});
                if (forceBreak) { lStart = i + 1; curLW = 0; } else { lStart = i; curLW = gw; }
                maxLH = 16.5f;
            } else { curLW += gw; }
        }
        if (lStart <= documento.size()) {
            int al = lStart < documento.size() ? documento[lStart].style.align : activeStyle.align;
            int sp = lStart < documento.size() ? documento[lStart].style.lineSpacing : activeStyle.lineSpacing;
            int in = lStart < documento.size() ? documento[lStart].style.indent : activeStyle.indent;
            renderLines.push_back({lStart, documento.size(), curLW, al, sp, maxLH, in});
        }

        C2D_TextBufClear(dynamicBuf);

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(topScreen, C2D_Color32(250, 250, 245, 255));
        C2D_SceneBegin(topScreen);

        if (menuAbierto == 3 || menuAbierto == 4) {
            C2D_DrawRectSolid(0, 0, 0.4f, 400, 240, C2D_Color32(30, 30, 30, 220));
            C2D_DrawRectSolid(5, 5, 0.5f, 390, 230, C2D_Color32(245, 245, 250, 255));
            C2D_DrawRectSolid(5, 5, 0.6f, 390, 3, COLOR_MUSGO); C2D_DrawRectSolid(5, 232, 0.6f, 390, 3, COLOR_MUSGO);
            C2D_DrawRectSolid(5, 5, 0.6f, 3, 230, COLOR_MUSGO); C2D_DrawRectSolid(392, 5, 0.6f, 3, 230, COLOR_MUSGO);
            
            C2D_Text mTxt; C2D_TextParse(&mTxt, dynamicBuf, menuAbierto == 3 ? "MANUAL DEL USUARIO (1/2)" : "MANUAL DEL USUARIO (2/2)"); C2D_TextOptimize(&mTxt);
            C2D_DrawText(&mTxt, C2D_WithColor, 20, 15, 0.7f, 0.55f, 0.55f, COLOR_NEGRO);
            
            if (menuAbierto == 3) {
                C2D_Text t1; C2D_TextParse(&t1, dynamicBuf, "GUIA TECNICA DE DIGITACION:\n- Salto de linea: **R**. Con L o ZL inserta salto de pagina.\n- Retroceso: **ZR**. Con L borra palabra, con ZL borra linea.\n- Copiar seleccion: **Y** (si hay texto seleccionado).\n- Cortar: **Y + L** o **Y + ZL**.\n- Pegar: **A** (pega con su formato original).\n- Pegar sin formato: **A + L** o **A + ZL** (aplica estilo vigente).\n- Marcar saltos fisicos: **¶** en la barra superior.");
                C2D_TextOptimize(&t1); C2D_DrawText(&t1, C2D_WithColor, 20, 45, 0.7f, 0.35f, 0.35f, COLOR_NEGRO);
            } else {
                C2D_Text t2; C2D_TextParse(&t2, dynamicBuf, "Info:\nFabián Contreras (CL)\nfabian.contreras.abara@gmail.com\nhttps://github.com/fabianquilpue");
                C2D_TextOptimize(&t2); C2D_DrawText(&t2, C2D_WithColor, 20, 50, 0.7f, 0.50f, 0.50f, COLOR_MUSGO);
            }
        } else {
            float textTopMargin = 10.0f;
            if (showTopInfoBar) {
                textTopMargin = 26.0f;
                C2D_DrawRectSolid(0, 0, 0.4f, 400, 20, COLOR_NEGRO);
                u8 batLevel = 5; PTMU_GetBatteryLevel(&batLevel); int batPct = (batLevel * 100) / 5;
                time_t now = time(NULL); struct tm* t = localtime(&now);
                
                char batStr[32]; snprintf(batStr, sizeof(batStr), "Bat: %d%%", batPct);
                char titleStr[128]; snprintf(titleStr, sizeof(titleStr), "%s.rtf [%s]", fileName.c_str(), ultimaHoraGuardado.c_str());
                char timeStr[32]; snprintf(timeStr, sizeof(timeStr), "%02d:%02d", t->tm_hour, t->tm_min);
                
                C2D_Text bT, tT, tmT; 
                C2D_TextParse(&bT, dynamicBuf, batStr); C2D_TextOptimize(&bT); C2D_DrawText(&bT, C2D_WithColor, 8.0f, 3.0f, 0.5f, 0.4f, 0.4f, COLOR_BLANCO);
                C2D_TextParse(&tT, dynamicBuf, titleStr); C2D_TextOptimize(&tT); 
                float tw=0, th=0; C2D_TextGetDimensions(&tT, 0.4f, 0.4f, &tw, &th);
                C2D_DrawText(&tT, C2D_WithColor, (400.0f - tw)/2.0f, 3.0f, 0.5f, 0.4f, 0.4f, COLOR_BLANCO);
                C2D_TextParse(&tmT, dynamicBuf, timeStr); C2D_TextOptimize(&tmT); C2D_DrawText(&tmT, C2D_WithColor, 360.0f, 3.0f, 0.5f, 0.4f, 0.4f, COLOR_BLANCO);
                
                C2D_DrawRectSolid(0, 220, 0.4f, 400, 20, C2D_Color32(230, 230, 235, 255));
                int words = 0; bool inWord=false; for(auto& c: documento) { if(c.glyph==" "||c.glyph=="\n"||c.glyph=="\t") inWord=false; else if(!inWord){ words++; inWord=true;} }
                int pages = (documento.size() / 1500) + 1;
                char statStr[128]; snprintf(statStr, sizeof(statStr), "Palabras: %d  |  Caracteres: %zu  |  Paginas est.: %d", words, documento.size(), pages);
                C2D_Text statTxt; C2D_TextParse(&statTxt, dynamicBuf, statStr); C2D_TextOptimize(&statTxt); C2D_DrawText(&statTxt, C2D_WithColor, 10.0f, 223.0f, 0.5f, 0.4f, 0.4f, COLOR_NEGRO);
            }

            float curY = textTopMargin;
            bool cursorVisible = (osGetTime() / 400) % 2 == 0;
            size_t selStart = hasSelection ? std::min(selAnchor, cursorPos) : 0; size_t selEnd = hasSelection ? std::max(selAnchor, cursorPos) : 0;

            for (const auto& line : renderLines) {
                float startX = 10.0f + (line.ind * 20.0f); 
                if (line.align == 1) startX = std::max(10.0f, (400.0f - line.w) / 2.0f);
                else if (line.align == 2) startX = std::max(10.0f, 390.0f - line.w);
                float curX = startX; float lineMult = (line.spacing == 0) ? 1.0f : ((line.spacing == 1) ? 1.2f : ((line.spacing == 2) ? 1.5f : 2.0f));

                for (size_t idx = line.sIdx; idx <= line.eIdx; idx++) {
                    if (idx == cursorPos && cursorVisible && menuAbierto == 0) { C2D_DrawRectSolid(curX, curY, 0.75f, 1.5f, line.maxH, COLOR_MUSGO); }
                    if (idx == line.eIdx || idx == documento.size()) break;

                    const auto& sc = documento[idx]; float textScale = sc.style.fontSize / 48.0f;
                    if (sc.glyph == "\n") {
                        if (showPilcrow) {
                            C2D_Text pTxt; C2D_TextParse(&pTxt, dynamicBuf, "¶"); C2D_TextOptimize(&pTxt);
                            C2D_DrawText(&pTxt, C2D_WithColor, curX, curY, 0.5f, textScale, textScale, C2D_Color32(150, 150, 150, 255));
                        }
                        break; 
                    }

                    float gWidth = measureChar(sc.glyph, sc.style.fontSize);
                    u32 fg = getPaletteColor(sc.style.fgColor); u32 bg = sc.style.bgColor != 1 ? getPaletteColor(sc.style.bgColor) : 0;
                    
                    if (bg != 0) C2D_DrawRectSolid(curX, curY, 0.25f, gWidth, line.maxH, bg);
                    if (hasSelection && idx >= selStart && idx < selEnd) C2D_DrawRectSolid(curX, curY, 0.3f, gWidth, line.maxH, C2D_Color32(180, 210, 245, 180));

                    if (sc.glyph != "\t" && sc.glyph != " ") {
                        C2D_Text glyphText; C2D_TextParse(&glyphText, dynamicBuf, sc.glyph.c_str()); C2D_TextOptimize(&glyphText);
                        C2D_DrawText(&glyphText, C2D_WithColor, curX, curY, 0.5f, textScale, textScale, fg);
                        if (sc.style.bold) C2D_DrawText(&glyphText, C2D_WithColor, curX + 1.0f, curY, 0.51f, textScale, textScale, fg);
                        if (sc.style.underline) C2D_DrawRectSolid(curX, curY + line.maxH - 2.0f, 0.6f, gWidth, 1.5f, fg);
                        if (sc.style.strike) C2D_DrawRectSolid(curX, curY + (line.maxH * 0.55f), 0.65f, gWidth, 1.5f, fg);
                    }
                    curX += gWidth + (sc.style.bold ? 1.0f : 0.0f);
                }
                curY += line.maxH * lineMult;
            }
        }

        C2D_TargetClear(bottomScreen, C2D_Color32(220, 225, 235, 255));
        C2D_SceneBegin(bottomScreen);

        auto drawCloseX = [&](float bx, float by) {
            C2D_DrawRectSolid(bx, by, 0.6f, 25, 25, COLOR_ROJO);
            C2D_DrawRectSolid(bx, by, 0.65f, 25, 2.5f, COLOR_NEGRO);
            C2D_DrawRectSolid(bx, by + 22.5f, 0.65f, 25, 2.5f, COLOR_NEGRO);
            C2D_DrawRectSolid(bx, by, 0.65f, 2.5f, 25, COLOR_NEGRO);
            C2D_DrawRectSolid(bx + 22.5f, by, 0.65f, 2.5f, 25, COLOR_NEGRO);
            C2D_Text xTxt; C2D_TextParse(&xTxt, dynamicBuf, "X"); C2D_TextOptimize(&xTxt);
            C2D_DrawText(&xTxt, C2D_WithColor, bx + 7, by + 2, 0.7f, 0.45f, 0.45f, COLOR_BLANCO);
        };

        if (menuAbierto == 1) { 
            C2D_DrawRectSolid(10, 10, 0.5f, 300, 220, C2D_Color32(245, 245, 250, 255)); 
            C2D_DrawRectSolid(10, 10, 0.5f, 300, 28, COLOR_MUSGO); 
            C2D_Text titleTxt; C2D_TextParse(&titleTxt, dynamicBuf, "AJUSTES DE FORMATO"); C2D_TextOptimize(&titleTxt);
            C2D_DrawText(&titleTxt, C2D_WithColor, 20, 16, 0.6f, 0.42f, 0.42f, COLOR_BLANCO);
            drawCloseX(275, 12);

            C2D_DrawRectSolid(15, 60, 0.6f, 40, 28, formatMenuRow == 0 && formatMenuCol == 0 ? COLOR_MUSGO : COLOR_BLANCO);
            C2D_DrawRectSolid(15, 60, 0.65f, 40, 2.5f, COLOR_NEGRO); C2D_DrawRectSolid(15, 85.5f, 0.65f, 40, 2.5f, COLOR_NEGRO);
            C2D_DrawRectSolid(15, 60, 0.65f, 2.5f, 28, COLOR_NEGRO); C2D_DrawRectSolid(52.5f, 60, 0.65f, 2.5f, 28, COLOR_NEGRO);
            C2D_Text minusTxt; C2D_TextParse(&minusTxt, dynamicBuf, "-"); C2D_TextOptimize(&minusTxt); C2D_DrawText(&minusTxt, C2D_WithColor, 32, 63, 0.7f, 0.5f, 0.5f, formatMenuRow == 0 && formatMenuCol == 0 ? COLOR_BLANCO : COLOR_NEGRO);
            
            char sizeStr[32]; snprintf(sizeStr, sizeof(sizeStr), "%d pt", activeStyle.fontSize / 2);
            C2D_Text szTxt; C2D_TextParse(&szTxt, dynamicBuf, sizeStr); C2D_TextOptimize(&szTxt);
            C2D_DrawText(&szTxt, C2D_WithColor, 75, 65, 0.6f, 0.45f, 0.45f, COLOR_NEGRO);
            
            C2D_DrawRectSolid(135, 60, 0.6f, 40, 28, formatMenuRow == 0 && formatMenuCol == 1 ? COLOR_MUSGO : COLOR_BLANCO);
            C2D_DrawRectSolid(135, 60, 0.65f, 40, 2.5f, COLOR_NEGRO); C2D_DrawRectSolid(135, 85.5f, 0.65f, 40, 2.5f, COLOR_NEGRO);
            C2D_DrawRectSolid(135, 60, 0.65f, 2.5f, 28, COLOR_NEGRO); C2D_DrawRectSolid(172.5f, 60, 0.65f, 2.5f, 28, COLOR_NEGRO);
            C2D_Text plusTxt; C2D_TextParse(&plusTxt, dynamicBuf, "+"); C2D_TextOptimize(&plusTxt); C2D_DrawText(&plusTxt, C2D_WithColor, 151, 63, 0.7f, 0.5f, 0.5f, formatMenuRow == 0 && formatMenuCol == 1 ? COLOR_BLANCO : COLOR_NEGRO);
            
            C2D_DrawRectSolid(195, 60, 0.6f, 100, 28, formatMenuRow == 3 && formatMenuCol == 0 ? COLOR_MUSGO : COLOR_BLANCO);
            C2D_DrawRectSolid(195, 60, 0.65f, 100, 2.5f, COLOR_NEGRO); C2D_DrawRectSolid(195, 85.5f, 0.65f, 100, 2.5f, COLOR_NEGRO);
            C2D_DrawRectSolid(195, 60, 0.65f, 2.5f, 28, COLOR_NEGRO); C2D_DrawRectSolid(292.5f, 60, 0.65f, 2.5f, 28, COLOR_NEGRO);
            std::string ls = activeStyle.lineSpacing == 0 ? "INT: 1.0 +" : (activeStyle.lineSpacing == 1 ? "INT: 1.2 +" : (activeStyle.lineSpacing == 2 ? "INT: 1.5 +" : "INT: 2.0 +"));
            C2D_Text lsTxt; C2D_TextParse(&lsTxt, dynamicBuf, ls.c_str()); C2D_TextOptimize(&lsTxt); 
            C2D_DrawText(&lsTxt, C2D_WithColor, 205, 66, 0.6f, 0.38f, 0.38f, formatMenuRow == 3 && formatMenuCol == 0 ? COLOR_BLANCO : COLOR_NEGRO);
            
            C2D_Text fgTxt; C2D_TextParse(&fgTxt, dynamicBuf, "COLOR FUENTE:"); C2D_TextOptimize(&fgTxt); C2D_DrawText(&fgTxt, C2D_WithColor, 15, 110, 0.6f, 0.4f, 0.4f, COLOR_NEGRO);
            for (int c = 0; c <= 7; c++) {
                C2D_DrawRectSolid(15 + (c * 34) - 1, 130 - 1, 0.55f, 28, 28, COLOR_NEGRO);
                C2D_DrawRectSolid(15 + (c * 34), 130, 0.6f, 26, 26, getPaletteColor(c));
                if (activeStyle.fgColor == c) C2D_DrawRectSolid(15 + (c * 34), 158, 0.6f, 26, 3, COLOR_NEGRO);
            }

            C2D_Text bgTxt; C2D_TextParse(&bgTxt, dynamicBuf, "COLOR FONDO:"); C2D_TextOptimize(&bgTxt); C2D_DrawText(&bgTxt, C2D_WithColor, 15, 170, 0.6f, 0.4f, 0.4f, COLOR_NEGRO);
            for (int c = 0; c <= 7; c++) {
                C2D_DrawRectSolid(15 + (c * 34) - 1, 190 - 1, 0.55f, 28, 28, COLOR_NEGRO);
                C2D_DrawRectSolid(15 + (c * 34), 190, 0.6f, 26, 26, getPaletteColor(c));
                if (activeStyle.bgColor == c) C2D_DrawRectSolid(15 + (c * 34), 218, 0.6f, 26, 3, COLOR_NEGRO);
            }
        } else if (menuAbierto == 2) { 
            C2D_DrawRectSolid(10, 10, 0.5f, 300, 230, C2D_Color32(245, 245, 250, 255)); 
            C2D_DrawRectSolid(10, 10, 0.5f, 300, 28, COLOR_MUSGO); 
            C2D_Text titleTxt; C2D_TextParse(&titleTxt, dynamicBuf, "MENU PRINCIPAL"); C2D_TextOptimize(&titleTxt);
            C2D_DrawText(&titleTxt, C2D_WithColor, 20, 16, 0.6f, 0.42f, 0.42f, COLOR_BLANCO);
            drawCloseX(275, 12);
            
            auto drawBtn = [&](int idx, float y, const char* label) {
                u32 bCol = (startMenuSel == idx) ? COLOR_MUSGO : C2D_Color32(220, 220, 225, 255);
                u32 tCol = (startMenuSel == idx) ? COLOR_BLANCO : COLOR_NEGRO;
                C2D_DrawRectSolid(60, y, 0.6f, 200, 25, bCol);
                C2D_Text lTxt; C2D_TextParse(&lTxt, dynamicBuf, label); C2D_TextOptimize(&lTxt);
                C2D_DrawText(&lTxt, C2D_WithColor, 70, y+5, 0.7f, 0.4f, 0.4f, tCol);
            };
            drawBtn(0, 50, "GUARDAR"); 
            drawBtn(1, 85, "GUARDAR COMO..."); 
            drawBtn(2, 120, "ABRIR"); 
            drawBtn(3, 155, "MANUAL DEL USUARIO"); 
            drawBtn(4, 190, "SALIR DEL PROGRAMA");
            
            C2D_Text foot; C2D_TextParse(&foot, dynamicBuf, "(c) Laura Dev 2026"); C2D_TextOptimize(&foot);
            C2D_DrawText(&foot, C2D_WithColor, 105, 222, 0.7f, 0.35f, 0.35f, C2D_Color32(150,150,150,255));
        } else if (menuAbierto == 5) { 
            C2D_DrawRectSolid(10, 10, 0.5f, 300, 230, C2D_Color32(245, 245, 250, 255));
            C2D_DrawRectSolid(10, 10, 0.5f, 300, 28, COLOR_MUSGO);
            C2D_Text titleTxt; C2D_TextParse(&titleTxt, dynamicBuf, "ABRIR ARCHIVO (sdmc:/)"); C2D_TextOptimize(&titleTxt);
            C2D_DrawText(&titleTxt, C2D_WithColor, 20, 16, 0.6f, 0.42f, 0.42f, COLOR_BLANCO);
            drawCloseX(275, 12);
            
            for (int i = 0; i < 5 && (fileScroll + i) < (int)fileList.size(); i++) {
                u32 bCol = (openMenuSel == i) ? COLOR_MUSGO : C2D_Color32(220, 220, 225, 255);
                u32 tCol = (openMenuSel == i) ? COLOR_BLANCO : COLOR_NEGRO;
                C2D_DrawRectSolid(20, 50 + (i*30), 0.6f, 260, 25, bCol);
                C2D_Text fTxt; C2D_TextParse(&fTxt, dynamicBuf, fileList[fileScroll + i].c_str()); C2D_TextOptimize(&fTxt);
                C2D_DrawText(&fTxt, C2D_WithColor, 30, 55 + (i*30), 0.7f, 0.4f, 0.4f, tCol);
            }
        } else if (menuAbierto == 6 || menuAbierto == 7) { 
            C2D_DrawRectSolid(30, 50, 0.5f, 260, 140, C2D_Color32(245, 245, 250, 255));
            C2D_DrawRectSolid(30, 50, 0.55f, 260, 26, COLOR_MUSGO);
            C2D_Text mTitle; C2D_TextParse(&mTitle, dynamicBuf, "ATENCION"); C2D_TextOptimize(&mTitle);
            C2D_DrawText(&mTitle, C2D_WithColor, 40, 54, 0.6f, 0.42f, 0.42f, COLOR_BLANCO);
            drawCloseX(255, 52);
            
            C2D_Text mMsg; 
            if (menuAbierto == 6) C2D_TextParse(&mMsg, dynamicBuf, "¿Estas seguro(a) de que quieres abrir\neste archivo? Se perderan todos los\ncambios no guardados.");
            else C2D_TextParse(&mMsg, dynamicBuf, "¿Estas seguro(a) de que quieres salir\ndel programa? Se perderan todos los\ncambios no guardados.");
            C2D_TextOptimize(&mMsg); C2D_DrawText(&mMsg, C2D_WithColor, 40, 85, 0.6f, 0.36f, 0.36f, COLOR_NEGRO);
            
            C2D_DrawRectSolid(70, 135, 0.6f, 70, 25, confirmSel == 0 ? COLOR_MUSGO : C2D_Color32(78, 122, 101, 160));
            C2D_Text tSi; C2D_TextParse(&tSi, dynamicBuf, "SI"); C2D_TextOptimize(&tSi); C2D_DrawText(&tSi, C2D_WithColor, 96, 139, 0.7f, 0.4f, 0.4f, COLOR_BLANCO);

            C2D_DrawRectSolid(180, 135, 0.6f, 70, 25, confirmSel == 1 ? COLOR_ROJO : C2D_Color32(220, 50, 50, 160));
            C2D_Text tNo; C2D_TextParse(&tNo, dynamicBuf, "NO"); C2D_TextOptimize(&tNo); C2D_DrawText(&tNo, C2D_WithColor, 206, 139, 0.7f, 0.4f, 0.4f, COLOR_BLANCO);
        } else if (menuAbierto == 3 || menuAbierto == 4) { 
            C2D_DrawRectSolid(0, 0, 0.4f, 320, 240, C2D_Color32(30, 30, 30, 220));
            C2D_DrawRectSolid(5, 5, 0.5f, 310, 230, C2D_Color32(245, 245, 250, 255));
            C2D_DrawRectSolid(5, 5, 0.6f, 310, 3, COLOR_MUSGO); C2D_DrawRectSolid(5, 232, 0.6f, 310, 3, COLOR_MUSGO);
            C2D_DrawRectSolid(5, 5, 0.6f, 3, 230, COLOR_MUSGO); C2D_DrawRectSolid(312, 5, 0.6f, 3, 230, COLOR_MUSGO);
            drawCloseX(275, 12);
            
            if (menuAbierto == 3) {
                C2D_Text t2; C2D_TextParse(&t2, dynamicBuf, "GUIA TECNICA (PAG 1):\n- Negrita: **N**. Cursiva: **K** (Tachada en rojo).\n- Subrayado y Tachado: **S/T** (Modificadores L/ZL).\n- Alineacion: **ALIN**. Tamano e Interlineado: **INT**.\n- Menu Grafico: **Aa** (con flecha desplegable).");
                C2D_TextOptimize(&t2); C2D_DrawText(&t2, C2D_WithColor, 15, 30, 0.7f, 0.40f, 0.40f, COLOR_NEGRO);
                
                C2D_DrawRectSolid(180, 190, 0.6f, 110, 25, manualMenuSel == 0 ? COLOR_BLANCO : COLOR_MUSGO);
                C2D_Text nTxt; C2D_TextParse(&nTxt, dynamicBuf, "SIGUIENTE >"); C2D_TextOptimize(&nTxt);
                C2D_DrawText(&nTxt, C2D_WithColor, 190, 195, 0.7f, 0.45f, 0.45f, manualMenuSel == 0 ? COLOR_MUSGO : COLOR_BLANCO);
            } else {
                C2D_Text t3; C2D_TextParse(&t3, dynamicBuf, "Info:\nFabián Contreras (CL)\nfabian.contreras.abara@gmail.com\nhttps://github.com/fabianquilpue");
                C2D_TextOptimize(&t3); C2D_DrawText(&t3, C2D_WithColor, 20, 50, 0.7f, 0.50f, 0.50f, COLOR_MUSGO);
                
                C2D_DrawRectSolid(20, 190, 0.6f, 100, 25, manualMenuSel == 0 ? COLOR_BLANCO : COLOR_MUSGO);
                C2D_Text aTxt; C2D_TextParse(&aTxt, dynamicBuf, "< ANTERIOR"); C2D_TextOptimize(&aTxt);
                C2D_DrawText(&aTxt, C2D_WithColor, 30, 195, 0.7f, 0.40f, 0.40f, manualMenuSel == 0 ? COLOR_MUSGO : COLOR_BLANCO);

                C2D_DrawRectSolid(140, 190, 0.6f, 110, 25, manualMenuSel == 1 ? COLOR_BLANCO : COLOR_MUSGO);
                C2D_Text cTxt; C2D_TextParse(&cTxt, dynamicBuf, "CERRAR"); C2D_TextOptimize(&cTxt);
                C2D_DrawText(&cTxt, C2D_WithColor, 172, 195, 0.7f, 0.40f, 0.40f, manualMenuSel == 1 ? COLOR_MUSGO : COLOR_BLANCO);
            }
        } else {
            for (int i = 0; i < (int)keys.size(); i++) {
                const auto& key = keys[i];
                u32 cFondo = C2D_Color32(255, 255, 255, 255); u32 cTexto = COLOR_NEGRO;

                if (i == activeTouchKeyIdx) cFondo = C2D_Color32(210, 225, 245, 255);
                
                if (key.type >= 5 && key.type != 7 && key.type != 10) {
                    bool activo = false;
                    if (key.base == "N") activo = activeStyle.bold;
                    else if (key.base == "K") activo = false; 
                    else if (key.base == "S") {
                        if (isZLHeld) activo = (activeStyle.underline && activeStyle.strike);
                        else if (isLHeld) activo = activeStyle.strike;
                        else activo = activeStyle.underline;
                    }
                    else if (key.type == 9) activo = showPilcrow;
                    
                    if (activo) { cFondo = COLOR_MUSGO; cTexto = COLOR_BLANCO; }
                    else { 
                        if (key.base == "N" || key.base == "K" || key.base == "S" || key.type == 6 || key.type == 7 || key.type == 9) {
                            cFondo = C2D_Color32(180, 185, 195, 255); 
                        } else {
                            cFondo = C2D_Color32(200, 205, 215, 255); 
                        }
                        cTexto = C2D_Color32(60, 65, 75, 255); 
                    }
                }
                
                if (key.type == 10) { cFondo = C2D_Color32(200, 205, 215, 255); cTexto = C2D_Color32(60, 65, 75, 255); }
                
                if ((key.base == "´" || key.ctrlAlt == "^" || key.shift == "¨") && estadoTeclaMuerta > 0) {
                    cFondo = COLOR_MUSGO; cTexto = COLOR_BLANCO;
                }

                C2D_DrawRectSolid(key.x, key.y, 0.3f, key.w, key.h, cFondo);

                if (key.type == 7) {
                    C2D_DrawTriangle(key.x + key.w - 10, key.y + 4, 0.4f,
                                     key.x + key.w - 4,  key.y + 10, 0.4f,
                                     key.x + key.w - 4,  key.y + 4,  0.4f, COLOR_MUSGO);
                    C2D_DrawRectSolid(key.x, key.y, 0.35f, key.w, 1.5f, COLOR_MUSGO);
                    C2D_DrawRectSolid(key.x, key.y + key.h - 1.5f, 0.35f, key.w, 1.5f, COLOR_MUSGO);
                    C2D_DrawRectSolid(key.x, key.y, 0.35f, 1.5f, key.h, COLOR_MUSGO);
                    C2D_DrawRectSolid(key.x + key.w - 1.5f, key.y, 0.35f, 1.5f, key.h, COLOR_MUSGO);
                }

                if (key.type == 6) { 
                    float bx = key.x + (key.w/2.0f) - 8.0f; float by = key.y + 6.0f; u32 ic = cTexto; float lh = 1.5f;
                    if (activeStyle.align == 0) { C2D_DrawRectSolid(bx, by, 0.4f, 16, lh, ic); C2D_DrawRectSolid(bx, by+4.0f, 0.4f, 10, lh, ic); C2D_DrawRectSolid(bx, by+8.0f, 0.4f, 14, lh, ic); }
                    else if (activeStyle.align == 1) { C2D_DrawRectSolid(bx, by, 0.4f, 16, lh, ic); C2D_DrawRectSolid(bx+3.0f, by+4.0f, 0.4f, 10, lh, ic); C2D_DrawRectSolid(bx+1.0f, by+8.0f, 0.4f, 14, lh, ic); }
                    else if (activeStyle.align == 2) { C2D_DrawRectSolid(bx, by, 0.4f, 16, lh, ic); C2D_DrawRectSolid(bx+6.0f, by+4.0f, 0.4f, 10, lh, ic); C2D_DrawRectSolid(bx+2.0f, by+8.0f, 0.4f, 14, lh, ic); }
                    else { C2D_DrawRectSolid(bx, by, 0.4f, 16, lh, ic); C2D_DrawRectSolid(bx, by+4.0f, 0.4f, 16, lh, ic); C2D_DrawRectSolid(bx, by+8.0f, 0.4f, 16, lh, ic); }
                } else {
                    std::string mainChar = key.base; float sX = (key.type >= 5) ? 0.42f : 0.40f;
                    float txX = (key.type == 2) ? (key.w / 2.0f) - 30.0f : (key.w / 2.0f) - 5.0f; 
                    if(key.type>=5) txX = (key.w/2.0f) - 10.0f;
                    float txY = (key.type >= 5) ? 3.5f : 6.0f;
                    if(key.y == 30.0f) txY = 3.5f; 
                    
                    bool isUpper = capsLock; if (key.base.size()==1 && key.base[0]>='a' && key.base[0]<='z') isUpper = (isUpper != isLHeld);
                    
                    if (isZLHeld && !key.ctrlAlt.empty()) { mainChar = key.ctrlAlt; txX = (key.w/2.0f) - 4.0f; }
                    else if ((isLHeld || isUpper) && !key.shift.empty()) { mainChar = key.shift; txX = (key.w/2.0f) - 4.0f; }

                    if (key.type == 5 && key.base == "S") {
                        if (isZLHeld) mainChar = "S/T"; else if (isLHeld) mainChar = "T"; else mainChar = "S";
                        txX = (key.w/2.0f) - 8.0f;
                    }
                    if (key.type == 10) { 
                        mainChar = isZLHeld ? "LINE" : (isLHeld ? "WORD" : "BACK");
                        txX = (key.w / 2.0f) - (isZLHeld ? 16.0f : (isLHeld ? 18.0f : 16.0f));
                        sX = 0.35f;
                    }
                    
                    if (key.type != 10) {
                        C2D_Text keyLabel; C2D_TextParse(&keyLabel, dynamicBuf, mainChar.c_str()); C2D_TextOptimize(&keyLabel);
                        C2D_DrawText(&keyLabel, C2D_WithColor, key.x + txX, key.y + txY, 0.5f, sX, sX, cTexto);
                    } else {
                        C2D_Text keyLabel; C2D_TextParse(&keyLabel, dynamicBuf, mainChar.c_str()); C2D_TextOptimize(&keyLabel);
                        C2D_DrawText(&keyLabel, C2D_WithColor, key.x + txX, key.y + txY, 0.5f, sX, sX, cTexto);
                    }

                    if (key.base == "K" && activeStyle.italic) {
                        C2D_DrawRectSolid(key.x + 4, key.y + 4, 0.45f, key.w - 8, 2.5f, COLOR_ROJO);
                    }
                }
                
                if (!isLHeld && !isZLHeld && !capsLock && (key.type == 0 || key.type == 4)) {
                    if (!key.shift.empty() && key.shift != key.base) {
                        C2D_Text sLbl; C2D_TextParse(&sLbl, dynamicBuf, key.shift.c_str()); C2D_TextOptimize(&sLbl);
                        C2D_DrawText(&sLbl, C2D_WithColor, key.x + 2, key.y + 2, 0.5f, 0.20f, 0.20f, COLOR_ROJO); 
                    }
                    if (!key.ctrlAlt.empty()) {
                        C2D_Text cLbl; C2D_TextParse(&cLbl, dynamicBuf, key.ctrlAlt.c_str()); C2D_TextOptimize(&cLbl);
                        C2D_DrawText(&cLbl, C2D_WithColor, key.x + key.w - 10, key.y + 2, 0.5f, 0.20f, 0.20f, COLOR_MUSGO); 
                    }
                }
            }
        }

        C3D_FrameEnd(0);
    }
    C2D_TextBufDelete(dynamicBuf);
}
