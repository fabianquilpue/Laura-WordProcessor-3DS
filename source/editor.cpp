#include <3ds.h>
#include <citro2d.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <dirent.h>
#include <sys/stat.h>

// --- PALETA DE COLORES ---
#define COLOR_PAPEL         C2D_Color32(250, 248, 242, 255)
#define COLOR_BLANCO        C2D_Color32(255, 255, 255, 255)
#define COLOR_NEGRO         C2D_Color32(34, 36, 38, 255)
#define COLOR_MUSGO         C2D_Color32(78, 122, 101, 255)
#define COLOR_MUSGO_CLARO   C2D_Color32(112, 162, 138, 255)
#define COLOR_ROJO          C2D_Color32(204, 72, 72, 255)
#define COLOR_GRIS_TECLA    C2D_Color32(238, 236, 230, 255)
#define COLOR_GRIS_BORDE    C2D_Color32(130, 134, 138, 255)
#define COLOR_GRIS_SUAVE    C2D_Color32(165, 168, 172, 255)
#define COLOR_SELECCION     C2D_Color32(186, 214, 198, 255)

struct CharStyle {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strike = false;
    u32 textColor = COLOR_NEGRO;
    u32 bgColor = COLOR_PAPEL;
    int fontSize = 24;

    bool operator==(const CharStyle& o) const {
        return bold == o.bold && italic == o.italic && underline == o.underline &&
               strike == o.strike && textColor == o.textColor && bgColor == o.bgColor &&
               fontSize == o.fontSize;
    }
    bool operator!=(const CharStyle& o) const { return !(*this == o); }
};

struct StyledChar {
    std::string glyph;
    CharStyle style;
};

struct EditorState {
    std::vector<StyledChar> doc;
    size_t cursor;
};

struct KeyButton {
    float x, y, w, h;
    std::string base;
    std::string shift;
    std::string alt;
    int type; // 0=char, 1=back, 2=space, 4=deadkey, 5=false_italic(K), 6=false_pilcrow(¶), 7=format(Aa)
};

struct VisualLine {
    size_t startIdx;
    size_t endIdx; // exclusivo
    float width;
    float height;
};

static std::vector<StyledChar> stringToStyled(const std::string& str, const CharStyle& st) {
    std::vector<StyledChar> out;
    size_t i = 0;
    while (i < str.size()) {
        unsigned char c = (unsigned char)str[i];
        size_t len = 1;
        if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        if (i + len > str.size()) len = 1;
        out.push_back({str.substr(i, len), st});
        i += len;
    }
    return out;
}

static std::string applyDeadKey(int deadState, const std::string& ch) {
    if (deadState == 1) { // Aguda ´
        if (ch == "a") return "á"; if (ch == "e") return "é"; if (ch == "i") return "í";
        if (ch == "o") return "ó"; if (ch == "u") return "ú";
        if (ch == "A") return "Á"; if (ch == "E") return "É"; if (ch == "I") return "Í";
        if (ch == "O") return "Ó"; if (ch == "U") return "Ú";
        return "´" + ch;
    } else if (deadState == 2) { // Diéresis ¨
        if (ch == "u") return "ü"; if (ch == "U") return "Ü";
        if (ch == "a") return "ä"; if (ch == "e") return "ë"; if (ch == "i") return "ï"; if (ch == "o") return "ö";
        return "¨" + ch;
    }
    return ch;
}

static void saveDocumentRTF(const std::string& path, const std::vector<StyledChar>& doc, int alignment, float lineSpacing) {
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return;

    fprintf(f, "{\\rtf1\\ansi\\ansicpg1252\\deff0{\\fonttbl{\\f0\\fmodern\\fcharset0 Courier New;}}\n");
    fprintf(f, "{\\colortbl ;\\red34\\green36\\blue38;\\red108\\green145\\blue126;\\red115\\green142\\blue175;\\red185\\green122\\blue118;\\red150\\green124\\blue165;\\red250\\green248\\blue242;}\n");
    const char* alignTag = "\\ql";
    if (alignment == 1) alignTag = "\\qc";
    else if (alignment == 2) alignTag = "\\qr";
    else if (alignment == 3) alignTag = "\\qj";
    int sl = (int)(240.0f * lineSpacing);
    fprintf(f, "\\pard%s\\sl%d\\slmult1 ", alignTag, sl);

    for (const auto& sc : doc) {
        if (sc.glyph == "\n") {
            fprintf(f, "\\par\n\\pard%s\\sl%d\\slmult1 ", alignTag, sl);
            continue;
        }
        if (sc.glyph == "\t") {
            fprintf(f, "\\tab ");
            continue;
        }
        int fs = sc.style.fontSize;
        fprintf(f, "{\\fs%d", fs);
        if (sc.style.bold) fprintf(f, "\\b");
        if (sc.style.underline) fprintf(f, "\\ul");
        if (sc.style.strike) fprintf(f, "\\strike");
        fprintf(f, " ");

        for (size_t i = 0; i < sc.glyph.size();) {
            unsigned char c = (unsigned char)sc.glyph[i];
            if (c < 128) {
                if (c == '\\' || c == '{' || c == '}') fprintf(f, "\\%c", c);
                else fprintf(f, "%c", c);
                i++;
            } else {
                u32 cp = 0;
                size_t len = 1;
                if ((c & 0xE0) == 0xC0 && i + 1 < sc.glyph.size()) {
                    cp = ((c & 0x1F) << 6) | ((unsigned char)sc.glyph[i + 1] & 0x3F);
                    len = 2;
                } else if ((c & 0xF0) == 0xE0 && i + 2 < sc.glyph.size()) {
                    cp = ((c & 0x0F) << 12) | (((unsigned char)sc.glyph[i + 1] & 0x3F) << 6) | ((unsigned char)sc.glyph[i + 2] & 0x3F);
                    len = 3;
                }
                fprintf(f, "\\u%u?", cp);
                i += len;
            }
        }
        fprintf(f, "}");
    }
    fprintf(f, "\n}");
    fclose(f);
}

static std::vector<std::string> scanRtfFiles() {
    std::vector<std::string> files;
    const char* dirs[2] = {"sdmc:/Textos3DS", "sdmc:/"};
    for (int d = 0; d < 2; d++) {
        DIR* dir = opendir(dirs[d]);
        if (!dir) continue;
        struct dirent* ent;
        while ((ent = readdir(dir)) != NULL) {
            std::string name = ent->d_name;
            if (name.size() > 4 && name.substr(name.size() - 4) == ".rtf") {
                std::string full = (d == 0) ? name : ("../" + name);
                if (std::find(files.begin(), files.end(), full) == files.end()) {
                    files.push_back(full);
                }
            }
        }
        closedir(dir);
    }
    std::sort(files.begin(), files.end());
    return files;
}

// Helper para dibujar texto con soporte de *negrita* y palabra «colores» en verde musgo
static float drawRichWrappedText(C2D_TextBuf buf, const std::string& rawText, float startX, float startY, float maxWidth, float scale, float lineStep, u32 defaultColor) {
    struct Span {
        std::string word;
        bool bold;
        bool moss;
        bool newline;
    };
    std::vector<Span> spans;
    bool inBold = false;
    std::string curWord = "";

    auto flushWord = [&]() {
        if (curWord.empty()) return;
        bool isMoss = (curWord == "colores" || curWord == "colores," || curWord == "colores.");
        spans.push_back({curWord, inBold, isMoss, false});
        curWord.clear();
    };

    for (size_t i = 0; i < rawText.size(); i++) {
        char c = rawText[i];
        if (c == '*') {
            flushWord();
            inBold = !inBold;
        } else if (c == '\n') {
            flushWord();
            spans.push_back({"", false, false, true});
        } else if (c == ' ') {
            curWord += ' ';
            flushWord();
        } else {
            curWord += c;
        }
    }
    flushWord();

    float cx = startX;
    float cy = startY;
    for (const auto& sp : spans) {
        if (sp.newline) {
            cx = startX;
            cy += lineStep;
            continue;
        }
        C2D_Text t;
        C2D_TextParse(&t, buf, sp.word.c_str());
        C2D_TextOptimize(&t);
        float w = 0, h = 0;
        C2D_TextGetDimensions(&t, scale, scale, &w, &h);

        if (cx > startX && (cx + w) > (startX + maxWidth)) {
            cx = startX;
            cy += lineStep;
        }
        u32 col = sp.moss ? COLOR_MUSGO : defaultColor;
        C2D_DrawText(&t, C2D_WithColor, cx, cy, 0.75f, scale, scale, col);
        if (sp.bold) {
            C2D_DrawText(&t, C2D_WithColor, cx + 0.85f, cy, 0.76f, scale, scale, col);
        }
        cx += w;
    }
    return cy + lineStep;
}

void runEditorLoop() {
    mkdir("sdmc:/Textos3DS", 0777);

    C3D_RenderTarget* topScreen = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottomScreen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    C2D_TextBuf dynamicBuf = C2D_TextBufNew(65536);

    // =========================================================================
    // 1. PANTALLA DE BIENVENIDA (FADE FROM BLACK -> ESPERA INPUT -> FADE TO BLACK)
    // =========================================================================
    {
        C2D_SpriteSheet splashSheet = C2D_SpriteSheetLoad("romfs:/splash.t3x");
        C2D_Image splashImg = {0};
        bool hasSplash = (splashSheet != NULL && C2D_SpriteSheetCount(splashSheet) > 0);
        if (hasSplash) splashImg = C2D_SpriteSheetGetImage(splashSheet, 0);

        bool splashActive = true;
        int fadeAlpha = 255;
        bool fadingIn = true;
        bool fadingOut = false;

        while (aptMainLoop() && splashActive) {
            hidScanInput();
            u32 kDown = hidKeysDown();

            // Permanece indefinidamente hasta registrar toque táctil o cualquier botón
            if (!fadingIn && !fadingOut && (kDown != 0)) {
                fadingOut = true;
            }

            if (fadingIn) {
                fadeAlpha -= 10;
                if (fadeAlpha <= 0) {
                    fadeAlpha = 0;
                    fadingIn = false;
                }
            } else if (fadingOut) {
                fadeAlpha += 12;
                if (fadeAlpha >= 255) {
                    fadeAlpha = 255;
                    splashActive = false;
                }
            }

            C2D_TextBufClear(dynamicBuf);
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            // Pantalla Superior
            C2D_TargetClear(topScreen, COLOR_BLANCO);
            C2D_SceneBegin(topScreen);

            if (hasSplash) {
                float imgW = (float)splashImg.subtex->width;
                float imgH = (float)splashImg.subtex->height;
                float scale = (imgW > 0.0f) ? (96.0f / imgW) : 1.0f;
                float drawW = imgW * scale;
                float drawH = imgH * scale;
                C2D_DrawImageAt(splashImg, (400.0f - drawW) / 2.0f, 18.0f + (96.0f - drawH) / 2.0f, 0.5f, NULL, scale, scale);
            } else {
                // Marco minimalista de respaldo si no hay RomFS
                C2D_DrawRectSolid(152, 18, 0.45f, 96, 96, COLOR_PAPEL);
                C2D_DrawRectSolid(152, 18, 0.48f, 96, 2, COLOR_MUSGO);
                C2D_DrawRectSolid(152, 112, 0.48f, 96, 2, COLOR_MUSGO);
                C2D_DrawRectSolid(152, 18, 0.48f, 2, 96, COLOR_MUSGO);
                C2D_DrawRectSolid(246, 18, 0.48f, 2, 96, COLOR_MUSGO);
                C2D_Text tLogo;
                C2D_TextParse(&tLogo, dynamicBuf, "L|");
                C2D_TextOptimize(&tLogo);
                float lw = 0, lh = 0;
                C2D_TextGetDimensions(&tLogo, 1.3f, 1.3f, &lw, &lh);
                C2D_DrawText(&tLogo, C2D_WithColor, (400.0f - lw) / 2.0f, 44.0f, 0.55f, 1.3f, 1.3f, COLOR_NEGRO);
            }

            C2D_Text tTitle, tSub1, tSub2, tCopy;
            C2D_TextParse(&tTitle, dynamicBuf, "Laura"); C2D_TextOptimize(&tTitle);
            C2D_TextParse(&tSub1, dynamicBuf, "Procesador de textos para la Nintendo 3DS"); C2D_TextOptimize(&tSub1);
            C2D_TextParse(&tSub2, dynamicBuf, "Word processor for the Nintendo 3DS"); C2D_TextOptimize(&tSub2);
            C2D_TextParse(&tCopy, dynamicBuf, "© Laura Dev 2026"); C2D_TextOptimize(&tCopy);

            float tw = 0, th = 0;
            C2D_TextGetDimensions(&tTitle, 1.15f, 1.15f, &tw, &th);
            C2D_DrawText(&tTitle, C2D_WithColor, (400.0f - tw) / 2.0f, 122.0f, 0.6f, 1.15f, 1.15f, COLOR_NEGRO);

            C2D_TextGetDimensions(&tSub1, 0.40f, 0.40f, &tw, &th);
            C2D_DrawText(&tSub1, C2D_WithColor, (400.0f - tw) / 2.0f, 164.0f, 0.6f, 0.40f, 0.40f, COLOR_NEGRO);

            C2D_TextGetDimensions(&tSub2, 0.38f, 0.38f, &tw, &th);
            C2D_DrawText(&tSub2, C2D_WithColor, (400.0f - tw) / 2.0f, 184.0f, 0.6f, 0.38f, 0.38f, COLOR_GRIS_BORDE);

            C2D_TextGetDimensions(&tCopy, 0.32f, 0.32f, &tw, &th);
            C2D_DrawText(&tCopy, C2D_WithColor, (400.0f - tw) / 2.0f, 214.0f, 0.6f, 0.32f, 0.32f, COLOR_GRIS_BORDE);

            if (fadeAlpha > 0) {
                C2D_DrawRectSolid(0, 0, 0.95f, 400, 240, C2D_Color32(0, 0, 0, fadeAlpha));
            }

            // Pantalla Inferior
            C2D_TargetClear(bottomScreen, COLOR_BLANCO);
            C2D_SceneBegin(bottomScreen);

            // Texto intermitente en color verde musgo
            if (!fadingOut && ((osGetTime() / 550) % 2 == 0)) {
                C2D_Text tPrompt;
                C2D_TextParse(&tPrompt, dynamicBuf, "Toca la pantalla o presiona cualquier botón");
                C2D_TextOptimize(&tPrompt);
                C2D_TextGetDimensions(&tPrompt, 0.40f, 0.40f, &tw, &th);
                C2D_DrawText(&tPrompt, C2D_WithColor, (320.0f - tw) / 2.0f, 112.0f, 0.6f, 0.40f, 0.40f, COLOR_MUSGO);
            }

            if (fadeAlpha > 0) {
                C2D_DrawRectSolid(0, 0, 0.95f, 320, 240, C2D_Color32(0, 0, 0, fadeAlpha));
            }

            C3D_FrameEnd(0);
        }

        if (splashSheet) C2D_SpriteSheetFree(splashSheet);
        C2D_TextBufClear(dynamicBuf);
    }

    // =========================================================================
    // 2. CONFIGURACIÓN DEL TECLADO TÁCTIL Y ESTADO DEL EDITOR
    // =========================================================================
    std::vector<KeyButton> keys;
    float btnW = 28.6f;
    float gap = 2.0f;

    // Fila superior de herramientas (y = 4, h = 23)
    keys.push_back({8.0f,   4.0f, 32.0f, 23.0f, "N",  "N",  "N",  5}); // Negrita (se reusará con subtipo)
    // Ajustamos botones de barra superior individualmente:
    keys.clear();
    // Barra superior de formato: Aa (Ajustes), N, K (falsa -> ERR rojo), S (Subrayado/Tachado), Alineación (4), Interlineado, ¶ (falso -> ERR rojo), BACK
    // Usaremos type: 10=Aa, 11=N, 12=K(falsa), 13=S, 14=AlignL, 15=AlignC, 16=AlignR, 17=AlignJ, 18=LineSpace, 19=Pilcrow(falso), 1=BACK
    keys.push_back({8.0f,   4.0f, 28.0f, 22.0f, "Aa", "Aa", "Aa", 10});
    keys.push_back({38.0f,  4.0f, 22.0f, 22.0f, "N",  "N",  "N",  11});
    keys.push_back({62.0f,  4.0f, 22.0f, 22.0f, "K",  "K",  "K",  12}); // Falsa: ERR rojo
    keys.push_back({86.0f,  4.0f, 22.0f, 22.0f, "S",  "T",  "S",  13});
    keys.push_back({111.0f, 4.0f, 20.0f, 22.0f, "AL", "AL", "AL", 14});
    keys.push_back({133.0f, 4.0f, 20.0f, 22.0f, "AC", "AC", "AC", 15});
    keys.push_back({155.0f, 4.0f, 20.0f, 22.0f, "AR", "AR", "AR", 16});
    keys.push_back({177.0f, 4.0f, 20.0f, 22.0f, "AJ", "AJ", "AJ", 17});
    keys.push_back({200.0f, 4.0f, 28.0f, 22.0f, "1.0","1.0","1.0",18});
    keys.push_back({230.0f, 4.0f, 22.0f, 22.0f, "¶",  "¶",  "¶",  19}); // Falsa: ERR rojo
    keys.push_back({254.0f, 4.0f, 58.0f, 22.0f, "BACK","L: WORD","ZL: LINE", 1});

    float row1 = 30.0f, row2 = 57.0f, row3 = 86.0f, row4 = 115.0f, row5 = 144.0f, row6 = 173.0f;
    float btnH = 26.0f;

    std::string r1_b[10] = {"1","2","3","4","5","6","7","8","9","0"};
    std::string r1_s[10] = {"*","\"","·","$","%","&","/","<",",","="};
    std::string r1_c[10] = {"|","@","#","ª","+","¬","\\","^","]","º"};
    for (int i = 0; i < 10; i++) keys.push_back({8.0f + i * (btnW + gap), row1, btnW, 24.0f, r1_b[i], r1_s[i], r1_c[i], 0});

    std::string r2_b[10] = {"q","w","e","r","t","y","u","i","o","p"};
    std::string r2_s[10] = {"Q","W","E","R","T","Y","U","I","O","P"};
    std::string r2_c[10] = {"","","€","","","","","","",""};
    for (int i = 0; i < 10; i++) keys.push_back({8.0f + i * (btnW + gap), row2, btnW, btnH, r2_b[i], r2_s[i], r2_c[i], 0});

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

    // Barra espaciadora (row6)
    keys.push_back({8.0f, row6, 304.0f, 30.0f, "ESPACIO", "ESPACIO", "PÁRRAFO", 2});

    // Colores pasteles opacos para Ajustes de Formato
    const u32 textPastels[6] = {
        C2D_Color32(45, 48, 52, 255),     // Carbón suave
        C2D_Color32(102, 140, 121, 255),  // Verde musgo pastel
        C2D_Color32(110, 138, 172, 255),  // Azul pizarra pastel
        C2D_Color32(182, 118, 114, 255),  // Terracota pastel
        C2D_Color32(146, 120, 162, 255),  // Ciruela pastel
        C2D_Color32(250, 248, 242, 255)   // Blanco papel
    };
    const u32 bgPastels[6] = {
        C2D_Color32(250, 248, 242, 255),  // Blanco papel
        C2D_Color32(198, 224, 208, 255),  // Menta pastel opaco
        C2D_Color32(196, 216, 235, 255),  // Celeste niebla pastel
        C2D_Color32(240, 210, 196, 255),  // Durazno pastel opaco
        C2D_Color32(218, 206, 235, 255),  // Lavanda pastel opaco
        C2D_Color32(242, 232, 194, 255)   // Crema vainilla pastel
    };

    std::vector<StyledChar> documento;
    size_t cursorPos = 0;
    bool hasSelection = false;
    size_t selAnchor = 0;
    bool isSelectModeToggle = false;
    std::vector<StyledChar> portapapeles;

    std::string ultimaHoraGuardado = "--:--";
    std::string fileName = "Documento1";
    std::vector<EditorState> undoStack;
    std::vector<EditorState> redoStack;

    CharStyle activeStyle;
    activeStyle.fontSize = 24;
    bool falseItalicErr = false;
    bool falsePilcrowErr = false;
    int alignmentMode = 0; // 0=Izq, 1=Centro, 2=Der, 3=Justificado
    float lineSpacingMult = 1.0f;

    int estadoTeclaMuerta = 0;
    bool capsLock = false;
    u64 lastLPressTick = 0;
    bool showTopInfoBar = true;
    float scrollY = 0.0f;

    // Estados de menús: 0=Editor, 1=Formato(Aa), 2=Inicio(START), 3=Manual Pág 1, 4=Manual Pág 2, 5=Abrir, 6=Confirmar Abrir, 7=Confirmar Salir
    int menuAbierto = 0;
    bool flagSalir = false;
    std::vector<std::string> fileList;
    int fileScroll = 0;

    int startMenuSel = 0;
    int formatMenuRow = 0, formatMenuCol = 0;
    int openMenuSel = 0;
    int confirmSel = 1;
    int manualMenuSel = 0;

    const int INITIAL_DELAY = 15;
    const int REPEAT_RATE = 4;
    int bsCooldown = 0, dpadCooldown = 0;
    bool zrUsedWithDpad = false;

    auto pushUndo = [&]() {
        if (!undoStack.empty() && undoStack.back().doc.size() == documento.size()) {
            bool same = true;
            for (size_t k = 0; k < documento.size(); k++) {
                if (documento[k].glyph != undoStack.back().doc[k].glyph || documento[k].style != undoStack.back().doc[k].style) {
                    same = false;
                    break;
                }
            }
            if (same) return;
        }
        undoStack.push_back({documento, cursorPos});
        if (undoStack.size() > 100) undoStack.erase(undoStack.begin());
        redoStack.clear();
    };

    auto deleteSelection = [&]() {
        if (!hasSelection || selAnchor == cursorPos) return;
        size_t start = std::min(selAnchor, cursorPos);
        size_t end = std::max(selAnchor, cursorPos);
        documento.erase(documento.begin() + start, documento.begin() + end);
        cursorPos = start;
        hasSelection = false;
        isSelectModeToggle = false;
    };

    auto performSave = [&]() {
        std::string fullPath = "sdmc:/Textos3DS/" + fileName + ".rtf";
        saveDocumentRTF(fullPath, documento, alignmentMode, lineSpacingMult);
        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
        ultimaHoraGuardado = buf;
    };

    auto measureChar = [&](const std::string& g, int size) -> float {
        if (g == "\t") return 22.0f;
        if (g == "\n") return 0.0f;
        C2D_Text t;
        C2D_TextParse(&t, dynamicBuf, g.c_str());
        float gw = 0, gh = 0;
        float scale = size / 48.0f;
        C2D_TextGetDimensions(&t, scale, scale, &gw, &gh);
        return (gw < 2.5f) ? (6.0f * scale * 2.0f) : gw;
    };

    // =========================================================================
    // 3. BUCLE PRINCIPAL DEL EDITOR
    // =========================================================================
    while (aptMainLoop() && !flagSalir) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        u32 kUp   = hidKeysUp();

        bool isLHeld  = (kHeld & KEY_L) != 0;
        bool isZLHeld = (kHeld & KEY_ZL) != 0;
        bool isZRHeld = (kHeld & KEY_ZR) != 0;

        if (kDown & KEY_ZR) zrUsedWithDpad = false;

        // Doble toque rápido en L activa Bloq Mayús
        if (kDown & KEY_L) {
            u64 cTick = osGetTime();
            if (cTick - lastLPressTick < 280) {
                capsLock = !capsLock;
                lastLPressTick = 0;
            } else {
                lastLPressTick = cTick;
            }
        }

        // Calcular líneas visuales actuales del documento para renderizado y navegación exacta
        std::vector<VisualLine> vLines;
        {
            const float maxLineW = 376.0f;
            size_t lineStart = 0;
            float curW = 0.0f;
            float curH = (activeStyle.fontSize * 0.68f) * lineSpacingMult;
            for (size_t i = 0; i < documento.size(); i++) {
                const auto& sc = documento[i];
                float chH = (sc.style.fontSize * 0.68f) * lineSpacingMult;
                if (chH > curH) curH = chH;
                if (sc.glyph == "\n") {
                    vLines.push_back({lineStart, i + 1, curW, curH});
                    lineStart = i + 1;
                    curW = 0.0f;
                    curH = (activeStyle.fontSize * 0.68f) * lineSpacingMult;
                } else {
                    float cw = measureChar(sc.glyph, sc.style.fontSize);
                    if (curW > 0.0f && curW + cw > maxLineW) {
                        vLines.push_back({lineStart, i, curW, curH});
                        lineStart = i;
                        curW = cw;
                        curH = chH;
                    } else {
                        curW += cw;
                    }
                }
            }
            vLines.push_back({lineStart, documento.size(), curW, curH});
        }

        // Ubicar en qué línea visual está cursorPos
        size_t curVLineIdx = 0;
        for (size_t idx = 0; idx < vLines.size(); idx++) {
            bool isLast = (idx + 1 == vLines.size());
            size_t effectiveEnd = vLines[idx].endIdx;
            if (!isLast && effectiveEnd > vLines[idx].startIdx && documento[effectiveEnd - 1].glyph == "\n") {
                if (cursorPos >= vLines[idx].startIdx && cursorPos < effectiveEnd) {
                    curVLineIdx = idx;
                    break;
                }
            } else if (isLast) {
                if (cursorPos >= vLines[idx].startIdx && cursorPos <= effectiveEnd) {
                    curVLineIdx = idx;
                    break;
                }
            } else {
                if (cursorPos >= vLines[idx].startIdx && cursorPos < effectiveEnd) {
                    curVLineIdx = idx;
                    break;
                }
            }
        }

        // Botón START: abre/cierra Menú de Inicio
        if (kDown & KEY_START) {
            if (menuAbierto == 2) menuAbierto = 0;
            else { menuAbierto = 2; startMenuSel = 0; }
        }

        // Botón X: alterna barras de info en editor, o cierra modal en menús
        if (kDown & KEY_X) {
            if (menuAbierto == 0) showTopInfoBar = !showTopInfoBar;
            else if (menuAbierto == 6 || menuAbierto == 7) menuAbierto = 2;
            else menuAbierto = 0;
        }

        // Botón SELECT: modo selección
        if ((kDown & KEY_SELECT) && menuAbierto == 0) {
            isSelectModeToggle = !isSelectModeToggle;
            if (isSelectModeToggle) {
                hasSelection = true;
                selAnchor = cursorPos;
            } else {
                hasSelection = false;
            }
        }

        // Salto de línea (R) y Salto de página (ZL + R) en editor
        if ((kDown & KEY_R) && menuAbierto == 0) {
            pushUndo();
            if (hasSelection) deleteSelection();
            if (isZLHeld) {
                // Salto de página visual (doble salto + separador o 3 líneas)
                documento.insert(documento.begin() + cursorPos, {"\n", activeStyle}); cursorPos++;
                documento.insert(documento.begin() + cursorPos, {"\n", activeStyle}); cursorPos++;
            } else {
                documento.insert(documento.begin() + cursorPos, {"\n", activeStyle});
                cursorPos++;
            }
        }

        // Copiar (Y) y Cortar (L + Y)
        if ((kDown & KEY_Y) && menuAbierto == 0 && hasSelection && selAnchor != cursorPos) {
            size_t s = std::min(selAnchor, cursorPos);
            size_t e = std::max(selAnchor, cursorPos);
            portapapeles.assign(documento.begin() + s, documento.begin() + e);
            if (isLHeld || isZLHeld) {
                pushUndo();
                deleteSelection();
            }
        }

        // Botón A: Confirmar en menús o Pegar en editor (A = con formato, L+A / ZL+A = sin formato)
        if (kDown & KEY_A) {
            if (menuAbierto == 0) {
                if (!portapapeles.empty()) {
                    pushUndo();
                    if (hasSelection) deleteSelection();
                    for (auto sc : portapapeles) {
                        if (isLHeld || isZLHeld) sc.style = activeStyle;
                        documento.insert(documento.begin() + cursorPos, sc);
                        cursorPos++;
                    }
                }
            } else if (menuAbierto == 1) {
                // Ajustes de formato mediante D-PAD + A
                if (formatMenuRow == 0) {
                    if (formatMenuCol == 0) activeStyle.fontSize = std::max(14, activeStyle.fontSize - 2);
                    else if (formatMenuCol == 1) activeStyle.fontSize = std::min(42, activeStyle.fontSize + 2);
                    else if (formatMenuCol == 2) lineSpacingMult = std::max(1.0f, lineSpacingMult - 0.2f);
                    else if (formatMenuCol == 3) lineSpacingMult = std::min(2.0f, lineSpacingMult + 0.2f);
                } else if (formatMenuRow == 1) {
                    activeStyle.textColor = textPastels[formatMenuCol];
                    if (hasSelection && selAnchor != cursorPos) {
                        pushUndo();
                        size_t s = std::min(selAnchor, cursorPos), e = std::max(selAnchor, cursorPos);
                        for (size_t i = s; i < e; i++) documento[i].style.textColor = activeStyle.textColor;
                    }
                } else if (formatMenuRow == 2) {
                    activeStyle.bgColor = bgPastels[formatMenuCol];
                    if (activeStyle.bgColor != bgPastels[0] && activeStyle.textColor == textPastels[5]) {
                        activeStyle.textColor = textPastels[0];
                    }
                    if (hasSelection && selAnchor != cursorPos) {
                        pushUndo();
                        size_t s = std::min(selAnchor, cursorPos), e = std::max(selAnchor, cursorPos);
                        for (size_t i = s; i < e; i++) documento[i].style.bgColor = activeStyle.bgColor;
                    }
                }
            } else if (menuAbierto == 2) {
                if (startMenuSel == 0) { performSave(); menuAbierto = 0; }
                else if (startMenuSel == 1) {
                    // Guardar Como usando teclado nativo swkbd de 3DS
                    SwkbdState swkbd;
                    char nameBuf[64];
                    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, 40);
                    swkbdSetInitialText(&swkbd, fileName.c_str());
                    swkbdSetHintText(&swkbd, "Nombre del archivo (.rtf)");
                    if (swkbdInputText(&swkbd, nameBuf, sizeof(nameBuf)) == SWKBD_BUTTON_RIGHT) {
                        std::string newName = nameBuf;
                        if (!newName.empty()) {
                            fileName = newName;
                            performSave();
                        }
                    }
                    menuAbierto = 0;
                } else if (startMenuSel == 2) {
                    fileList = scanRtfFiles();
                    fileScroll = 0;
                    openMenuSel = 0;
                    menuAbierto = 5;
                } else if (startMenuSel == 3) {
                    menuAbierto = 3;
                    manualMenuSel = 0;
                } else if (startMenuSel == 4) {
                    menuAbierto = 7;
                    confirmSel = 1;
                }
            } else if (menuAbierto == 3) {
                menuAbierto = 4;
                manualMenuSel = 0;
            } else if (menuAbierto == 4) {
                if (manualMenuSel == 0) { menuAbierto = 3; manualMenuSel = 0; }
                else { menuAbierto = 2; }
            } else if (menuAbierto == 5) {
                if (!fileList.empty()) {
                    menuAbierto = 6;
                    confirmSel = 1;
                }
            } else if (menuAbierto == 6) {
                if (confirmSel == 0 && !fileList.empty()) {
                    std::string chosen = fileList[fileScroll + openMenuSel];
                    std::string path = (chosen.rfind("../", 0) == 0) ? ("sdmc:/" + chosen.substr(3)) : ("sdmc:/Textos3DS/" + chosen);
                    fileName = chosen;
                    size_t slash = fileName.find_last_of('/');
                    if (slash != std::string::npos) fileName = fileName.substr(slash + 1);
                    size_t dot = fileName.find_last_of('.');
                    if (dot != std::string::npos) fileName = fileName.substr(0, dot);

                    FILE* f = fopen(path.c_str(), "r");
                    if (f) {
                        char b[512];
                        std::string raw = "";
                        while (fgets(b, sizeof(b), f)) raw += b;
                        fclose(f);
                        documento.clear();
                        size_t textStart = raw.find("\\pard");
                        if (textStart != std::string::npos) {
                            size_t sIdx = raw.find(" ", textStart);
                            if (sIdx != std::string::npos) {
                                std::string pure = raw.substr(sIdx + 1);
                                size_t pos = 0;
                                while ((pos = pure.find("\\par\n", pos)) != std::string::npos) { pure.replace(pos, 5, "\n"); pos += 1; }
                                pos = 0;
                                while ((pos = pure.find("\\tab ", pos)) != std::string::npos) { pure.replace(pos, 5, "\t"); pos += 1; }
                                std::string finalT = "";
                                bool inTag = false;
                                for (char c : pure) {
                                    if (c == '\\') inTag = true;
                                    else if (inTag && (c == ' ' || c == '\n')) inTag = false;
                                    else if (!inTag && c != '}' && c != '{') finalT += c;
                                }
                                documento = stringToStyled(finalT, activeStyle);
                            }
                        }
                        cursorPos = documento.size();
                        hasSelection = false;
                        undoStack.clear();
                        redoStack.clear();
                    }
                    menuAbierto = 0;
                } else {
                    menuAbierto = 5;
                }
            } else if (menuAbierto == 7) {
                if (confirmSel == 0) flagSalir = true;
                else menuAbierto = 2;
            }
        }

        // Botón B: Cierra menús (override temporal) o Undo / Redo en editor
        if (kDown & KEY_B) {
            if (menuAbierto == 6 || menuAbierto == 7) {
                menuAbierto = 2;
            } else if (menuAbierto != 0) {
                menuAbierto = 0;
            } else {
                if (isZLHeld) {
                    if (!redoStack.empty()) {
                        undoStack.push_back({documento, cursorPos});
                        documento = redoStack.back().doc;
                        cursorPos = std::min(redoStack.back().cursor, documento.size());
                        redoStack.pop_back();
                        hasSelection = false;
                    }
                } else {
                    int steps = isLHeld ? 5 : 1;
                    while (steps-- > 0 && !undoStack.empty()) {
                        redoStack.push_back({documento, cursorPos});
                        documento = undoStack.back().doc;
                        cursorPos = std::min(undoStack.back().cursor, documento.size());
                        undoStack.pop_back();
                        hasSelection = false;
                    }
                }
            }
        }

        // =====================================================================
        // NAVEGACIÓN POR D-PAD (EDITOR Y MENÚS)
        // =====================================================================
        bool triggerDpad = false;
        u32 dpadMask = KEY_DLEFT | KEY_DRIGHT | KEY_DUP | KEY_DDOWN;
        if (kDown & dpadMask) {
            triggerDpad = true;
            dpadCooldown = INITIAL_DELAY;
        } else if (kHeld & dpadMask) {
            if (dpadCooldown > 0) dpadCooldown--;
            else { triggerDpad = true; dpadCooldown = REPEAT_RATE; }
        } else {
            dpadCooldown = 0;
        }

        if (triggerDpad) {
            if (menuAbierto == 1) {
                int maxCol = (formatMenuRow == 0) ? 3 : 5;
                if (kHeld & KEY_DUP) formatMenuRow = std::max(0, formatMenuRow - 1);
                if (kHeld & KEY_DDOWN) formatMenuRow = std::min(2, formatMenuRow + 1);
                maxCol = (formatMenuRow == 0) ? 3 : 5;
                if (formatMenuCol > maxCol) formatMenuCol = maxCol;
                if (kHeld & KEY_DLEFT) formatMenuCol = std::max(0, formatMenuCol - 1);
                if (kHeld & KEY_DRIGHT) formatMenuCol = std::min(maxCol, formatMenuCol + 1);
            } else if (menuAbierto == 2) {
                if (kHeld & KEY_DUP) startMenuSel = std::max(0, startMenuSel - 1);
                if (kHeld & KEY_DDOWN) startMenuSel = std::min(4, startMenuSel + 1);
            } else if (menuAbierto == 3 || menuAbierto == 4) {
                int maxOpt = (menuAbierto == 3) ? 0 : 1;
                if (kHeld & KEY_DLEFT || kHeld & KEY_DUP) manualMenuSel = std::max(0, manualMenuSel - 1);
                if (kHeld & KEY_DRIGHT || kHeld & KEY_DDOWN) manualMenuSel = std::min(maxOpt, manualMenuSel + 1);
            } else if (menuAbierto == 5) {
                if (kHeld & KEY_DUP) {
                    if (openMenuSel > 0) openMenuSel--;
                    else if (fileScroll > 0) fileScroll--;
                }
                if (kHeld & KEY_DDOWN) {
                    if (openMenuSel < 4 && (fileScroll + openMenuSel + 1) < (int)fileList.size()) openMenuSel++;
                    else if ((fileScroll + openMenuSel + 1) < (int)fileList.size()) fileScroll++;
                }
            } else if (menuAbierto == 6 || menuAbierto == 7) {
                if (kHeld & KEY_DLEFT || kHeld & KEY_DUP) confirmSel = 0;
                if (kHeld & KEY_DRIGHT || kHeld & KEY_DDOWN) confirmSel = 1;
            } else if (menuAbierto == 0) {
                bool sel = (kHeld & KEY_SELECT) || isSelectModeToggle;
                if (!sel) hasSelection = false;
                else if (!hasSelection) { hasSelection = true; selAnchor = cursorPos; }

                if (isZRHeld) {
                    zrUsedWithDpad = true;
                    // ZR + Izquierda/Derecha: inicio / final de la línea actual
                    const auto& vl = vLines[curVLineIdx];
                    size_t lineEnd = vl.endIdx;
                    if (lineEnd > vl.startIdx && documento[lineEnd - 1].glyph == "\n") lineEnd--;
                    if (kHeld & KEY_DLEFT) cursorPos = vl.startIdx;
                    if (kHeld & KEY_DRIGHT) cursorPos = lineEnd;
                } else if (isLHeld) {
                    // L + Izquierda/Derecha: avanza/retrocede una palabra
                    if (kHeld & KEY_DLEFT) {
                        while (cursorPos > 0 && (documento[cursorPos - 1].glyph == " " || documento[cursorPos - 1].glyph == "\n" || documento[cursorPos - 1].glyph == "\t")) cursorPos--;
                        while (cursorPos > 0 && documento[cursorPos - 1].glyph != " " && documento[cursorPos - 1].glyph != "\n" && documento[cursorPos - 1].glyph != "\t") cursorPos--;
                    }
                    if (kHeld & KEY_DRIGHT) {
                        while (cursorPos < documento.size() && documento[cursorPos].glyph != " " && documento[cursorPos].glyph != "\n" && documento[cursorPos].glyph != "\t") cursorPos++;
                        while (cursorPos < documento.size() && (documento[cursorPos].glyph == " " || documento[cursorPos].glyph == "\n" || documento[cursorPos].glyph == "\t")) cursorPos++;
                    }
                    // L + Arriba/Abajo: avanza/retrocede un párrafo
                    if (kHeld & KEY_DUP) {
                        if (cursorPos > 0 && documento[cursorPos - 1].glyph == "\n") cursorPos--;
                        while (cursorPos > 0 && documento[cursorPos - 1].glyph != "\n") cursorPos--;
                    }
                    if (kHeld & KEY_DDOWN) {
                        while (cursorPos < documento.size() && documento[cursorPos].glyph != "\n") cursorPos++;
                        if (cursorPos < documento.size() && documento[cursorPos].glyph == "\n") cursorPos++;
                    }
                } else if (isZLHeld) {
                    // ZL + Arriba/Abajo: avanza/retrocede una página completa (~10 líneas visuales)
                    if (kHeld & KEY_DUP) {
                        size_t targetLine = (curVLineIdx >= 10) ? (curVLineIdx - 10) : 0;
                        cursorPos = vLines[targetLine].startIdx;
                    }
                    if (kHeld & KEY_DDOWN) {
                        size_t targetLine = std::min(vLines.size() - 1, curVLineIdx + 10);
                        cursorPos = vLines[targetLine].startIdx;
                    }
                } else {
                    // D-PAD sin modificadores:
                    // Izquierda / Derecha: carácter a carácter
                    // Arriba: devuelve cursor al inicio de la línea actual
                    // Abajo: lleva cursor al final de la línea actual
                    const auto& vl = vLines[curVLineIdx];
                    size_t lineEnd = vl.endIdx;
                    if (lineEnd > vl.startIdx && documento[lineEnd - 1].glyph == "\n") lineEnd--;

                    if (kHeld & KEY_DLEFT) {
                        if (cursorPos > 0) cursorPos--;
                    }
                    if (kHeld & KEY_DRIGHT) {
                        if (cursorPos < documento.size()) cursorPos++;
                    }
                    if (kHeld & KEY_DUP) {
                        cursorPos = vl.startIdx;
                    }
                    if (kHeld & KEY_DDOWN) {
                        cursorPos = lineEnd;
                    }
                }
            }
        }

        // =====================================================================
        // RETROCESO CON BOTÓN FÍSICO ZR (solo si no se usó como modificador D-PAD)
        // =====================================================================
        auto executeBackspace = [&]() {
            if (documento.empty() && !hasSelection) return;
            pushUndo();
            if (hasSelection) {
                deleteSelection();
                return;
            }
            if (cursorPos == 0) return;

            if (isZLHeld) {
                // Borrar línea completa actual
                const auto& vl = vLines[curVLineIdx];
                documento.erase(documento.begin() + vl.startIdx, documento.begin() + vl.endIdx);
                cursorPos = vl.startIdx;
            } else if (isLHeld) {
                // Borrar palabra anterior
                size_t orig = cursorPos;
                while (cursorPos > 0 && (documento[cursorPos - 1].glyph == " " || documento[cursorPos - 1].glyph == "\t")) cursorPos--;
                while (cursorPos > 0 && documento[cursorPos - 1].glyph != " " && documento[cursorPos - 1].glyph != "\n" && documento[cursorPos - 1].glyph != "\t") cursorPos--;
                if (cursorPos == orig && cursorPos > 0) cursorPos--;
                documento.erase(documento.begin() + cursorPos, documento.begin() + orig);
            } else {
                documento.erase(documento.begin() + cursorPos - 1);
                cursorPos--;
            }
        };

        if (menuAbierto == 0) {
            if ((kUp & KEY_ZR) && !zrUsedWithDpad) {
                executeBackspace();
                bsCooldown = 0;
            } else if (isZRHeld && !zrUsedWithDpad && !(kDown & KEY_ZR)) {
                if (bsCooldown > 0) bsCooldown--;
                else {
                    executeBackspace();
                    bsCooldown = REPEAT_RATE;
                }
            } else if (!isZRHeld) {
                bsCooldown = INITIAL_DELAY;
            }
        }

        // =====================================================================
        // ENTRADA TÁCTIL (STYLUS)
        // =====================================================================
        touchPosition touch;
        hidTouchRead(&touch);

        if (kDown & KEY_TOUCH) {
            if (menuAbierto == 0) {
                bool efectivoShift = isLHeld || capsLock;
                for (const auto& key : keys) {
                    if (touch.px >= key.x && touch.px <= (key.x + key.w) &&
                        touch.py >= key.y && touch.py <= (key.y + key.h)) {

                        if (key.type == 0) { // Tecla normal
                            std::string outCh = key.base;
                            if (isZLHeld && !key.alt.empty()) outCh = key.alt;
                            else if (efectivoShift && !key.shift.empty()) outCh = key.shift;

                            if (estadoTeclaMuerta != 0) {
                                outCh = applyDeadKey(estadoTeclaMuerta, outCh);
                                estadoTeclaMuerta = 0;
                            }
                            pushUndo();
                            if (hasSelection) deleteSelection();
                            documento.insert(documento.begin() + cursorPos, {outCh, activeStyle});
                            cursorPos++;
                        } else if (key.type == 4) { // Tecla muerta ´ / ¨ / `
                            if (isZLHeld) {
                                pushUndo();
                                if (hasSelection) deleteSelection();
                                documento.insert(documento.begin() + cursorPos, {"`", activeStyle});
                                cursorPos++;
                                estadoTeclaMuerta = 0;
                            } else if (isLHeld) {
                                estadoTeclaMuerta = (estadoTeclaMuerta == 2) ? 0 : 2;
                            } else {
                                estadoTeclaMuerta = (estadoTeclaMuerta == 1) ? 0 : 1;
                            }
                        } else if (key.type == 1) { // BACK táctil
                            executeBackspace();
                        } else if (key.type == 2) { // Barra espaciadora
                            pushUndo();
                            if (hasSelection) deleteSelection();
                            if (isZLHeld) {
                                // Aumentar sangría de párrafo: inserta tabulación al inicio del párrafo actual
                                size_t pStart = cursorPos;
                                while (pStart > 0 && documento[pStart - 1].glyph != "\n") pStart--;
                                documento.insert(documento.begin() + pStart, {"\t", activeStyle});
                                cursorPos++;
                            } else if (isLHeld) {
                                // Tabulación en la posición del cursor
                                documento.insert(documento.begin() + cursorPos, {"\t", activeStyle});
                                cursorPos++;
                            } else {
                                documento.insert(documento.begin() + cursorPos, {" ", activeStyle});
                                cursorPos++;
                            }
                        } else if (key.type == 10) { // Botón Aa (Ajustes de Formato)
                            menuAbierto = 1;
                            formatMenuRow = 0;
                            formatMenuCol = 0;
                        } else if (key.type == 11) { // Negrita (N)
                            activeStyle.bold = !activeStyle.bold;
                            if (hasSelection && selAnchor != cursorPos) {
                                pushUndo();
                                size_t s = std::min(selAnchor, cursorPos), e = std::max(selAnchor, cursorPos);
                                for (size_t i = s; i < e; i++) documento[i].style.bold = activeStyle.bold;
                            }
                        } else if (key.type == 12) { // Cursiva falsa (K -> ERR en rojo)
                            falseItalicErr = !falseItalicErr;
                        } else if (key.type == 13) { // Subrayado / Tachado (S / L+S)
                            if (isLHeld) {
                                activeStyle.strike = !activeStyle.strike;
                                if (hasSelection && selAnchor != cursorPos) {
                                    pushUndo();
                                    size_t s = std::min(selAnchor, cursorPos), e = std::max(selAnchor, cursorPos);
                                    for (size_t i = s; i < e; i++) documento[i].style.strike = activeStyle.strike;
                                }
                            } else {
                                activeStyle.underline = !activeStyle.underline;
                                if (hasSelection && selAnchor != cursorPos) {
                                    pushUndo();
                                    size_t s = std::min(selAnchor, cursorPos), e = std::max(selAnchor, cursorPos);
                                    for (size_t i = s; i < e; i++) documento[i].style.underline = activeStyle.underline;
                                }
                            }
                        } else if (key.type >= 14 && key.type <= 17) {
                            alignmentMode = key.type - 14;
                        } else if (key.type == 18) {
                            if (lineSpacingMult < 1.1f) lineSpacingMult = 1.2f;
                            else if (lineSpacingMult < 1.4f) lineSpacingMult = 1.5f;
                            else if (lineSpacingMult < 1.9f) lineSpacingMult = 2.0f;
                            else lineSpacingMult = 1.0f;
                        } else if (key.type == 19) { // Calderón falso (¶ -> ERR en rojo)
                            falsePilcrowErr = !falsePilcrowErr;
                        }
                        break;
                    }
                }
            } else if (menuAbierto == 1) {
                // Cruz roja de cierre táctil (x=280..304, y=14..38)
                if (touch.px >= 280 && touch.px <= 304 && touch.py >= 14 && touch.py <= 38) {
                    menuAbierto = 0;
                }
                // Fila 0: Tamaño (- en x=24..54, + en x=104..134) e Interlineado (< en x=176..206, > en x=256..286), y=62..88
                if (touch.py >= 62 && touch.py <= 88) {
                    if (touch.px >= 24 && touch.px <= 54) { formatMenuRow = 0; formatMenuCol = 0; activeStyle.fontSize = std::max(14, activeStyle.fontSize - 2); }
                    if (touch.px >= 104 && touch.px <= 134) { formatMenuRow = 0; formatMenuCol = 1; activeStyle.fontSize = std::min(42, activeStyle.fontSize + 2); }
                    if (touch.px >= 176 && touch.px <= 206) { formatMenuRow = 0; formatMenuCol = 2; lineSpacingMult = std::max(1.0f, lineSpacingMult - 0.2f); }
                    if (touch.px >= 256 && touch.px <= 286) { formatMenuRow = 0; formatMenuCol = 3; lineSpacingMult = std::min(2.0f, lineSpacingMult + 0.2f); }
                }
                // Fila 1: Colores de Texto (6 cuadros de 38x26, gap 8, x = 24 + i*46, y = 116..142)
                if (touch.py >= 116 && touch.py <= 142) {
                    for (int i = 0; i < 6; i++) {
                        float sx = 24.0f + i * 46.0f;
                        if (touch.px >= sx && touch.px <= sx + 38.0f) {
                            formatMenuRow = 1;
                            formatMenuCol = i;
                            activeStyle.textColor = textPastels[i];
                            if (hasSelection && selAnchor != cursorPos) {
                                pushUndo();
                                size_t s = std::min(selAnchor, cursorPos), e = std::max(selAnchor, cursorPos);
                                for (size_t k = s; k < e; k++) documento[k].style.textColor = activeStyle.textColor;
                            }
                        }
                    }
                }
                // Fila 2: Colores de Fondo (6 cuadros de 38x26, gap 8, x = 24 + i*46, y = 168..194)
                if (touch.py >= 168 && touch.py <= 194) {
                    for (int i = 0; i < 6; i++) {
                        float sx = 24.0f + i * 46.0f;
                        if (touch.px >= sx && touch.px <= sx + 38.0f) {
                            formatMenuRow = 2;
                            formatMenuCol = i;
                            activeStyle.bgColor = bgPastels[i];
                            if (activeStyle.bgColor != bgPastels[0] && activeStyle.textColor == textPastels[5]) {
                                activeStyle.textColor = textPastels[0];
                            }
                            if (hasSelection && selAnchor != cursorPos) {
                                pushUndo();
                                size_t s = std::min(selAnchor, cursorPos), e = std::max(selAnchor, cursorPos);
                                for (size_t k = s; k < e; k++) documento[k].style.bgColor = activeStyle.bgColor;
                            }
                        }
                    }
                }
            } else if (menuAbierto == 2) {
                // Cruz roja táctil (x=280..304, y=14..38)
                if (touch.px >= 280 && touch.px <= 304 && touch.py >= 14 && touch.py <= 38) {
                    menuAbierto = 0;
                }
                const float itemY[5] = {48.0f, 80.0f, 112.0f, 144.0f, 176.0f};
                for (int i = 0; i < 5; i++) {
                    if (touch.px >= 30 && touch.px <= 290 && touch.py >= itemY[i] && touch.py <= itemY[i] + 26.0f) {
                        startMenuSel = i;
                        if (i == 0) { performSave(); menuAbierto = 0; }
                        else if (i == 1) {
                            SwkbdState swkbd;
                            char nameBuf[64];
                            swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, 40);
                            swkbdSetInitialText(&swkbd, fileName.c_str());
                            swkbdSetHintText(&swkbd, "Nombre del archivo (.rtf)");
                            if (swkbdInputText(&swkbd, nameBuf, sizeof(nameBuf)) == SWKBD_BUTTON_RIGHT) {
                                std::string newName = nameBuf;
                                if (!newName.empty()) { fileName = newName; performSave(); }
                            }
                            menuAbierto = 0;
                        } else if (i == 2) {
                            fileList = scanRtfFiles();
                            fileScroll = 0;
                            openMenuSel = 0;
                            menuAbierto = 5;
                        } else if (i == 3) {
                            menuAbierto = 3;
                            manualMenuSel = 0;
                        } else if (i == 4) {
                            menuAbierto = 7;
                            confirmSel = 1;
                        }
                    }
                }
            } else if (menuAbierto == 3) {
                if (touch.px >= 282 && touch.px <= 304 && touch.py >= 12 && touch.py <= 34) {
                    menuAbierto = 2;
                } else if (touch.px >= 190 && touch.px <= 300 && touch.py >= 196 && touch.py <= 222) {
                    menuAbierto = 4;
                    manualMenuSel = 0;
                }
            } else if (menuAbierto == 4) {
                if (touch.px >= 282 && touch.px <= 304 && touch.py >= 12 && touch.py <= 34) {
                    menuAbierto = 2;
                } else if (touch.px >= 22 && touch.px <= 137 && touch.py >= 194 && touch.py <= 220) {
                    menuAbierto = 3;
                    manualMenuSel = 0;
                } else if (touch.px >= 183 && touch.px <= 298 && touch.py >= 194 && touch.py <= 220) {
                    menuAbierto = 2;
                }
            } else if (menuAbierto == 5) {
                if (touch.px >= 280 && touch.px <= 304 && touch.py >= 14 && touch.py <= 38) {
                    menuAbierto = 2;
                }
                for (int i = 0; i < 5 && (fileScroll + i) < (int)fileList.size(); i++) {
                    float fy = 52.0f + i * 32.0f;
                    if (touch.px >= 24 && touch.px <= 296 && touch.py >= fy && touch.py <= fy + 26.0f) {
                        openMenuSel = i;
                        menuAbierto = 6;
                        confirmSel = 1;
                    }
                }
            } else if (menuAbierto == 6 || menuAbierto == 7) {
                if (touch.px >= 266 && touch.px <= 288 && touch.py >= 48 && touch.py <= 70) {
                    menuAbierto = (menuAbierto == 6) ? 5 : 2;
                } else if (touch.px >= 60 && touch.px <= 140 && touch.py >= 142 && touch.py <= 170) {
                    confirmSel = 0;
                    if (menuAbierto == 7) {
                        flagSalir = true;
                    } else if (!fileList.empty()) {
                        std::string chosen = fileList[fileScroll + openMenuSel];
                        std::string path = (chosen.rfind("../", 0) == 0) ? ("sdmc:/" + chosen.substr(3)) : ("sdmc:/Textos3DS/" + chosen);
                        fileName = chosen;
                        size_t slash = fileName.find_last_of('/');
                        if (slash != std::string::npos) fileName = fileName.substr(slash + 1);
                        size_t dot = fileName.find_last_of('.');
                        if (dot != std::string::npos) fileName = fileName.substr(0, dot);
                        FILE* f = fopen(path.c_str(), "r");
                        if (f) {
                            char b[512]; std::string raw = "";
                            while (fgets(b, sizeof(b), f)) raw += b;
                            fclose(f);
                            documento.clear();
                            size_t textStart = raw.find("\\pard");
                            if (textStart != std::string::npos) {
                                size_t sIdx = raw.find(" ", textStart);
                                if (sIdx != std::string::npos) {
                                    std::string pure = raw.substr(sIdx + 1);
                                    size_t pos = 0;
                                    while ((pos = pure.find("\\par\n", pos)) != std::string::npos) { pure.replace(pos, 5, "\n"); pos += 1; }
                                    pos = 0;
                                    while ((pos = pure.find("\\tab ", pos)) != std::string::npos) { pure.replace(pos, 5, "\t"); pos += 1; }
                                    std::string finalT = ""; bool inTag = false;
                                    for (char c : pure) {
                                        if (c == '\\') inTag = true;
                                        else if (inTag && (c == ' ' || c == '\n')) inTag = false;
                                        else if (!inTag && c != '}' && c != '{') finalT += c;
                                    }
                                    documento = stringToStyled(finalT, activeStyle);
                                }
                            }
                            cursorPos = documento.size();
                            hasSelection = false;
                        }
                        menuAbierto = 0;
                    }
                } else if (touch.px >= 180 && touch.px <= 260 && touch.py >= 142 && touch.py <= 170) {
                    confirmSel = 1;
                    menuAbierto = (menuAbierto == 6) ? 5 : 2;
                }
            }
        }

        // =====================================================================
        // RENDERIZADO DE PANTALLAS
        // =====================================================================
        C2D_TextBufClear(dynamicBuf);
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        // ---------------------------------------------------------------------
        // PANTALLA SUPERIOR (400x240)
        // ---------------------------------------------------------------------
        C2D_TargetClear(topScreen, COLOR_PAPEL);
        C2D_SceneBegin(topScreen);

        if (menuAbierto == 3 || menuAbierto == 4) {
            // MANUAL DEL USUARIO — PANTALLA SUPERIOR
            C2D_DrawRectSolid(0, 0, 0.4f, 400, 240, C2D_Color32(30, 30, 30, 210));
            C2D_DrawRectSolid(10, 10, 0.5f, 380, 220, COLOR_BLANCO);
            C2D_DrawRectSolid(10, 10, 0.6f, 380, 3, COLOR_MUSGO);
            C2D_DrawRectSolid(10, 227, 0.6f, 380, 3, COLOR_MUSGO);
            C2D_DrawRectSolid(10, 10, 0.6f, 3, 220, COLOR_MUSGO);
            C2D_DrawRectSolid(387, 10, 0.6f, 3, 220, COLOR_MUSGO);

            C2D_DrawRectSolid(10, 10, 0.65f, 380, 26, COLOR_MUSGO);
            C2D_Text mTxt;
            C2D_TextParse(&mTxt, dynamicBuf, menuAbierto == 3 ? "MANUAL DEL USUARIO (1/2)" : "MANUAL DEL USUARIO (2/2)");
            C2D_TextOptimize(&mTxt);
            C2D_DrawText(&mTxt, C2D_WithColor, 20.0f, 15.0f, 0.7f, 0.42f, 0.42f, COLOR_BLANCO);

            if (menuAbierto == 3) {
                // Página 1 Superior: Texto literal con margen superior limpio y negritas reales
                std::string p1TopText =
                    "GUÍA DE DIGITACIÓN Y NAVEGACIÓN:\n"
                    "Utiliza el *d-pad* para navegar intuitivamente por el documento. Puedes apretar *SELECT* para seleccionar texto mientras te desplazas.\n"
                    "También puedes copiar, cortar y pegar el texto, entre otras funciones, muchas de las cuales serán accesibles mediante la función de shift (*L*) o de alt (*ZL*):";
                drawRichWrappedText(dynamicBuf, p1TopText, 22.0f, 48.0f, 356.0f, 0.43f, 16.5f, COLOR_NEGRO);
            } else {
                // Página 2 Superior: ÚNICAMENTE la reseña institucional aprobada en una sola columna
                std::string p2TopText =
                    "«Laura» es un procesador de textos que reúne las funciones esenciales necesarias para la escritura sin distracciones. Su diseño se inspira en la sencillez de las máquinas de escribir y en la ergonomía. Las puntuaciones están pensadas para ser accesibles de manera fácil e intuitiva.\n\n"
                    "El programa cuenta con casi la totalidad de caracteres de un teclado QWERTY convencional, y tiene la capacidad de exportar sus archivos en formato RTF, permitiendo un mayor control sobre el estilo y la estructura del texto. El proyecto no persigue fines de lucro, pero tampoco puede ser comercializado o monetizado al acogerse a Creative Commons.";
                drawRichWrappedText(dynamicBuf, p2TopText, 22.0f, 44.0f, 356.0f, 0.39f, 14.5f, COLOR_NEGRO);
            }
        } else {
            // HOJA DE DOCUMENTO PRINCIPAL
            float textTopMargin = showTopInfoBar ? 28.0f : 12.0f;
            float textBotMargin = showTopInfoBar ? 214.0f : 228.0f;

            // Calcular desplazamiento vertical automático (scroll) para mantener el cursor visible
            float cursorYRel = 0.0f;
            for (size_t idx = 0; idx < curVLineIdx; idx++) cursorYRel += vLines[idx].height;
            float visibleH = textBotMargin - textTopMargin;
            if (cursorYRel - scrollY > visibleH - 22.0f) {
                scrollY = cursorYRel - (visibleH - 22.0f);
            }
            if (cursorYRel < scrollY) {
                scrollY = cursorYRel;
            }
            if (scrollY < 0.0f) scrollY = 0.0f;

            size_t selS = std::min(selAnchor, cursorPos);
            size_t selE = std::max(selAnchor, cursorPos);

            float drawY = textTopMargin - scrollY;
            for (size_t idx = 0; idx < vLines.size(); idx++) {
                const auto& vl = vLines[idx];
                if (drawY + vl.height >= textTopMargin - 10.0f && drawY <= textBotMargin) {
                    float drawX = 12.0f;
                    float extraSpace = 0.0f;
                    bool hasNewlineEnd = (vl.endIdx > vl.startIdx && documento[vl.endIdx - 1].glyph == "\n");

                    if (alignmentMode == 1) { // Centro
                        drawX = 12.0f + std::max(0.0f, (376.0f - vl.width) / 2.0f);
                    } else if (alignmentMode == 2) { // Derecha
                        drawX = 12.0f + std::max(0.0f, 376.0f - vl.width);
                    } else if (alignmentMode == 3 && !hasNewlineEnd && (idx + 1 < vLines.size())) { // Justificado
                        int spaces = 0;
                        for (size_t k = vl.startIdx; k < vl.endIdx; k++) {
                            if (documento[k].glyph == " ") spaces++;
                        }
                        if (spaces > 0) extraSpace = std::max(0.0f, (376.0f - vl.width) / (float)spaces);
                    }

                    for (size_t k = vl.startIdx; k < vl.endIdx; k++) {
                        if (k == cursorPos && ((osGetTime() / 450) % 2 == 0)) {
                            C2D_DrawRectSolid(drawX, drawY, 0.7f, 1.8f, vl.height * 0.85f, COLOR_MUSGO);
                        }
                        const auto& sc = documento[k];
                        if (sc.glyph == "\n") continue;

                        float cw = measureChar(sc.glyph, sc.style.fontSize);
                        if (sc.glyph == " ") cw += extraSpace;

                        // Fondo de selección o resaltado de carácter
                        if (hasSelection && k >= selS && k < selE) {
                            C2D_DrawRectSolid(drawX, drawY, 0.45f, cw, vl.height * 0.88f, COLOR_SELECCION);
                        } else if (sc.style.bgColor != COLOR_PAPEL) {
                            C2D_DrawRectSolid(drawX, drawY, 0.45f, cw, vl.height * 0.88f, sc.style.bgColor);
                        }

                        if (sc.glyph != "\t" && sc.glyph != " ") {
                            C2D_Text ct;
                            C2D_TextParse(&ct, dynamicBuf, sc.glyph.c_str());
                            C2D_TextOptimize(&ct);
                            float scale = sc.style.fontSize / 48.0f;
                            C2D_DrawText(&ct, C2D_WithColor, drawX, drawY, 0.6f, scale, scale, sc.style.textColor);
                            if (sc.style.bold) {
                                C2D_DrawText(&ct, C2D_WithColor, drawX + 0.9f, drawY, 0.61f, scale, scale, sc.style.textColor);
                            }
                        }
                        if (sc.style.underline) {
                            C2D_DrawRectSolid(drawX, drawY + vl.height * 0.78f, 0.65f, cw, 1.3f, sc.style.textColor);
                        }
                        if (sc.style.strike) {
                            C2D_DrawRectSolid(drawX, drawY + vl.height * 0.44f, 0.65f, cw, 1.3f, sc.style.textColor);
                        }
                        drawX += cw;
                    }
                    if (cursorPos == vl.endIdx && (idx + 1 == vLines.size() || !hasNewlineEnd) && ((osGetTime() / 450) % 2 == 0)) {
                        C2D_DrawRectSolid(drawX, drawY, 0.7f, 1.8f, vl.height * 0.85f, COLOR_MUSGO);
                    }
                }
                drawY += vl.height;
            }

            // Barras de información superior e inferior (Toggle con X)
            if (showTopInfoBar) {
                C2D_DrawRectSolid(0, 0, 0.8f, 400, 20, COLOR_NEGRO);
                u8 batLevel = 5;
                PTMU_GetBatteryLevel(&batLevel);
                int batPct = (batLevel * 100) / 5;
                time_t now = time(NULL);
                struct tm* t = localtime(&now);

                char batStr[32]; snprintf(batStr, sizeof(batStr), "Bat: %d%%", batPct);
                char titleStr[128]; snprintf(titleStr, sizeof(titleStr), "%s.rtf [%s]", fileName.c_str(), ultimaHoraGuardado.c_str());
                char timeStr[32]; snprintf(timeStr, sizeof(timeStr), "%02d:%02d", t->tm_hour, t->tm_min);

                C2D_Text bT, tT, tmT;
                C2D_TextParse(&bT, dynamicBuf, batStr); C2D_TextOptimize(&bT);
                C2D_DrawText(&bT, C2D_WithColor, 8.0f, 3.0f, 0.85f, 0.38f, 0.38f, COLOR_BLANCO);

                C2D_TextParse(&tT, dynamicBuf, titleStr); C2D_TextOptimize(&tT);
                float tw = 0, th = 0;
                C2D_TextGetDimensions(&tT, 0.38f, 0.38f, &tw, &th);
                C2D_DrawText(&tT, C2D_WithColor, (400.0f - tw) / 2.0f, 3.0f, 0.85f, 0.38f, 0.38f, COLOR_BLANCO);

                C2D_TextParse(&tmT, dynamicBuf, timeStr); C2D_TextOptimize(&tmT);
                C2D_DrawText(&tmT, C2D_WithColor, 362.0f, 3.0f, 0.85f, 0.38f, 0.38f, COLOR_BLANCO);

                // Barra inferior de conteo
                C2D_DrawRectSolid(0, 220, 0.8f, 400, 20, C2D_Color32(234, 232, 226, 255));
                int words = 0;
                bool inWord = false;
                for (const auto& c : documento) {
                    if (c.glyph == " " || c.glyph == "\n" || c.glyph == "\t") inWord = false;
                    else if (!inWord) { words++; inWord = true; }
                }
                int pages = ((int)vLines.size() / 10) + 1;
                char statStr[128];
                snprintf(statStr, sizeof(statStr), "Palabras: %d   |   Caracteres: %zu   |   Páginas: %d", words, documento.size(), pages);
                C2D_Text stT;
                C2D_TextParse(&stT, dynamicBuf, statStr); C2D_TextOptimize(&stT);
                C2D_TextGetDimensions(&stT, 0.36f, 0.36f, &tw, &th);
                C2D_DrawText(&stT, C2D_WithColor, (400.0f - tw) / 2.0f, 223.0f, 0.85f, 0.36f, 0.36f, COLOR_NEGRO);
            }
        }

        // ---------------------------------------------------------------------
        // PANTALLA INFERIOR (320x240)
        // ---------------------------------------------------------------------
        C2D_TargetClear(bottomScreen, COLOR_PAPEL);
        C2D_SceneBegin(bottomScreen);

        if (menuAbierto == 0) {
            bool efectivoShift = isLHeld || capsLock;

            for (const auto& key : keys) {
                u32 bg = COLOR_BLANCO;
                u32 fg = COLOR_NEGRO;

                if (key.type == 10) { bg = COLOR_MUSGO; fg = COLOR_BLANCO; }
                else if (key.type == 11 && activeStyle.bold) { bg = COLOR_MUSGO; fg = COLOR_BLANCO; }
                else if (key.type == 12 && falseItalicErr) { bg = COLOR_ROJO; fg = COLOR_BLANCO; }
                else if (key.type == 13 && (activeStyle.underline || activeStyle.strike)) { bg = COLOR_MUSGO; fg = COLOR_BLANCO; }
                else if (key.type >= 14 && key.type <= 17 && alignmentMode == (key.type - 14)) { bg = COLOR_MUSGO; fg = COLOR_BLANCO; }
                else if (key.type == 19 && falsePilcrowErr) { bg = COLOR_ROJO; fg = COLOR_BLANCO; }
                else if (key.type == 1) { bg = COLOR_GRIS_TECLA; }
                else if (key.type == 4 && estadoTeclaMuerta != 0) { bg = COLOR_MUSGO; fg = COLOR_BLANCO; }

                // Fondo de tecla + Marco completo nítido alrededor de cada tecla
                C2D_DrawRectSolid(key.x, key.y, 0.30f, key.w, key.h, bg);
                C2D_DrawRectSolid(key.x, key.y, 0.42f, key.w, 1.2f, COLOR_GRIS_BORDE);
                C2D_DrawRectSolid(key.x, key.y + key.h - 1.5f, 0.42f, key.w, 1.5f, COLOR_NEGRO);
                C2D_DrawRectSolid(key.x, key.y, 0.42f, 1.2f, key.h, COLOR_GRIS_BORDE);
                C2D_DrawRectSolid(key.x + key.w - 1.2f, key.y, 0.42f, 1.2f, key.h, COLOR_NEGRO);

                if (key.type == 2) {
                    // BARRA ESPACIADORA CON ÍCONOS DE TABULACIÓN (IZQ) Y AUMENTAR SANGRÍA (DER)
                    u32 leftIconCol = isLHeld ? COLOR_MUSGO : COLOR_GRIS_SUAVE;
                    u32 rightIconCol = isZLHeld ? COLOR_MUSGO : COLOR_GRIS_SUAVE;

                    // Ícono izquierdo: Tabulación (–>|)
                    float lx = key.x + 14.0f;
                    float ly = key.y + 15.0f;
                    C2D_DrawRectSolid(lx, ly - 1.0f, 0.55f, 14.0f, 2.0f, leftIconCol);
                    C2D_DrawRectSolid(lx + 10.0f, ly - 3.5f, 0.55f, 2.0f, 7.0f, leftIconCol);
                    C2D_DrawRectSolid(lx + 12.0f, ly - 2.0f, 0.55f, 2.0f, 4.0f, leftIconCol);
                    C2D_DrawRectSolid(lx + 16.0f, ly - 6.0f, 0.55f, 2.0f, 12.0f, leftIconCol);

                    // Ícono derecho: Aumentar sangría (tres líneas con flecha a la derecha ≡→)
                    float rx = key.x + key.w - 34.0f;
                    float ry = key.y + 9.0f;
                    // Tres líneas horizontales
                    C2D_DrawRectSolid(rx, ry,        0.55f, 11.0f, 1.8f, rightIconCol);
                    C2D_DrawRectSolid(rx, ry + 5.0f, 0.55f, 11.0f, 1.8f, rightIconCol);
                    C2D_DrawRectSolid(rx, ry + 10.0f,0.55f, 11.0f, 1.8f, rightIconCol);
                    // Flecha hacia la derecha
                    C2D_DrawRectSolid(rx + 13.0f, ry + 5.0f, 0.55f, 8.0f, 1.8f, rightIconCol);
                    C2D_DrawRectSolid(rx + 17.0f, ry + 2.5f, 0.55f, 2.0f, 7.0f, rightIconCol);
                    C2D_DrawRectSolid(rx + 19.0f, ry + 3.8f, 0.55f, 2.0f, 4.4f, rightIconCol);

                    // Texto dinámico de la barra espaciadora
                    const char* spLabel = "ESPACIO";
                    float shiftOffset = 0.0f;
                    u32 spTextCol = COLOR_NEGRO;
                    if (isZLHeld) {
                        spLabel = "PÁRRAFO";
                        shiftOffset = 14.0f;
                        spTextCol = COLOR_MUSGO;
                    } else if (isLHeld) {
                        spLabel = "ESPACIO";
                        shiftOffset = 14.0f;
                        spTextCol = COLOR_MUSGO;
                    }
                    C2D_Text tSp;
                    C2D_TextParse(&tSp, dynamicBuf, spLabel);
                    C2D_TextOptimize(&tSp);
                    float sw = 0, sh = 0;
                    C2D_TextGetDimensions(&tSp, 0.40f, 0.40f, &sw, &sh);
                    C2D_DrawText(&tSp, C2D_WithColor, key.x + (key.w - sw) / 2.0f + shiftOffset, key.y + 7.0f, 0.55f, 0.40f, 0.40f, spTextCol);
                } else if (key.type == 1) {
                    // Tecla BACK
                    const char* bLbl = isZLHeld ? "ZL: LINE" : (isLHeld ? "L: WORD" : "BACK");
                    C2D_Text tBk;
                    C2D_TextParse(&tBk, dynamicBuf, bLbl);
                    C2D_TextOptimize(&tBk);
                    float bw = 0, bh = 0;
                    C2D_TextGetDimensions(&tBk, 0.34f, 0.34f, &bw, &bh);
                    C2D_DrawText(&tBk, C2D_WithColor, key.x + (key.w - bw) / 2.0f, key.y + 5.0f, 0.55f, 0.34f, 0.34f, isZLHeld || isLHeld ? COLOR_MUSGO : COLOR_NEGRO);
                } else if (key.type >= 10) {
                    // Botones de barra superior
                    std::string lbl = key.base;
                    if (key.type == 12 && falseItalicErr) lbl = "ERR";
                    else if (key.type == 19 && falsePilcrowErr) lbl = "ERR";
                    else if (key.type == 13 && isLHeld) lbl = "T";
                    else if (key.type == 18) {
                        char lBuf[16];
                        snprintf(lBuf, sizeof(lBuf), "%.1f", lineSpacingMult);
                        lbl = lBuf;
                    }

                    C2D_Text tTop;
                    C2D_TextParse(&tTop, dynamicBuf, lbl.c_str());
                    C2D_TextOptimize(&tTop);
                    float lw = 0, lh = 0;
                    float sc = (lbl == "ERR") ? 0.30f : 0.35f;
                    C2D_TextGetDimensions(&tTop, sc, sc, &lw, &lh);
                    float tx = key.x + (key.w - lw) / 2.0f;
                    float ty = key.y + 5.0f;
                    C2D_DrawText(&tTop, C2D_WithColor, tx, ty, 0.55f, sc, sc, fg);

                    // Subrayado / tachado visual en botón S/T
                    if (key.type == 13) {
                        if (isLHeld || activeStyle.strike) {
                            C2D_DrawRectSolid(tx - 1.0f, ty + 6.5f, 0.75f, lw + 2.0f, 1.8f, fg);
                        } else {
                            C2D_DrawRectSolid(tx - 1.0f, ty + 12.5f, 0.75f, lw + 2.0f, 1.6f, fg);
                        }
                    }
                } else {
                    // Tecla normal / tecla muerta con variantes
                    std::string mainLbl = key.base;
                    if (isZLHeld && !key.alt.empty()) mainLbl = key.alt;
                    else if (efectivoShift && !key.shift.empty()) mainLbl = key.shift;

                    C2D_Text tMain;
                    C2D_TextParse(&tMain, dynamicBuf, mainLbl.c_str());
                    C2D_TextOptimize(&tMain);
                    float mw = 0, mh = 0;
                    C2D_TextGetDimensions(&tMain, 0.42f, 0.42f, &mw, &mh);
                    C2D_DrawText(&tMain, C2D_WithColor, key.x + (key.w - mw) / 2.0f, key.y + 6.0f, 0.55f, 0.42f, 0.42f, fg);

                    if (!efectivoShift && !isZLHeld) {
                        if (!key.shift.empty() && key.shift != key.base && key.type == 0 && (key.base[0] < 'a' || key.base[0] > 'z') && key.base != "ñ") {
                            C2D_Text tSh;
                            C2D_TextParse(&tSh, dynamicBuf, key.shift.c_str());
                            C2D_TextOptimize(&tSh);
                            C2D_DrawText(&tSh, C2D_WithColor, key.x + 2.5f, key.y + 1.5f, 0.55f, 0.23f, 0.23f, COLOR_GRIS_BORDE);
                        }
                        if (!key.alt.empty()) {
                            C2D_Text tAl;
                            C2D_TextParse(&tAl, dynamicBuf, key.alt.c_str());
                            C2D_TextOptimize(&tAl);
                            float aw = 0, ah = 0;
                            C2D_TextGetDimensions(&tAl, 0.23f, 0.23f, &aw, &ah);
                            C2D_DrawText(&tAl, C2D_WithColor, key.x + key.w - aw - 2.5f, key.y + 1.5f, 0.55f, 0.23f, 0.23f, COLOR_MUSGO);
                        }
                    }
                }
            }

            // Barra inferior de estado de modificadores (L / ZL / SELECT / CAPS)
            C2D_DrawRectSolid(0, 212, 0.5f, 320, 28, C2D_Color32(236, 234, 228, 255));
            C2D_DrawRectSolid(0, 212, 0.55f, 320, 1.5f, COLOR_GRIS_BORDE);

            auto drawModStatus = [&](const char* label, bool active, float x) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%s: %s", label, active ? "ON" : "OFF");
                C2D_Text tM;
                C2D_TextParse(&tM, dynamicBuf, buf);
                C2D_TextOptimize(&tM);
                C2D_DrawText(&tM, C2D_WithColor, x, 218.0f, 0.6f, 0.34f, 0.34f, active ? COLOR_MUSGO : COLOR_NEGRO);
            };
            drawModStatus("L(Shift)", efectivoShift, 10.0f);
            drawModStatus("ZL(Alt)", isZLHeld, 98.0f);
            drawModStatus("SEL", isSelectModeToggle || (kHeld & KEY_SELECT), 182.0f);
            drawModStatus("START:Menú", false, 244.0f);

        } else if (menuAbierto == 1) {
            // =================================================================
            // MENÚ AJUSTES DE FORMATO (COLORES PASTELES OPACOS + HITBOXES EXACTAS)
            // =================================================================
            C2D_DrawRectSolid(10, 10, 0.4f, 300, 220, COLOR_BLANCO);
            C2D_DrawRectSolid(10, 10, 0.5f, 300, 2, COLOR_MUSGO);
            C2D_DrawRectSolid(10, 228, 0.5f, 300, 2, COLOR_MUSGO);
            C2D_DrawRectSolid(10, 10, 0.5f, 2, 220, COLOR_MUSGO);
            C2D_DrawRectSolid(308, 10, 0.5f, 2, 220, COLOR_MUSGO);

            C2D_Text tHead;
            C2D_TextParse(&tHead, dynamicBuf, "AJUSTES DE FORMATO");
            C2D_TextOptimize(&tHead);
            C2D_DrawText(&tHead, C2D_WithColor, 24.0f, 18.0f, 0.6f, 0.42f, 0.42f, COLOR_MUSGO);

            // Cruz roja de cierre (táctil, centrada)
            C2D_DrawRectSolid(280, 14, 0.55f, 24, 24, COLOR_ROJO);
            C2D_Text tClose; C2D_TextParse(&tClose, dynamicBuf, "X"); C2D_TextOptimize(&tClose);
            float cw = 0, ch = 0; C2D_TextGetDimensions(&tClose, 0.42f, 0.42f, &cw, &ch);
            C2D_DrawText(&tClose, C2D_WithColor, 280.0f + (24.0f - cw) / 2.0f, 18.0f, 0.65f, 0.42f, 0.42f, COLOR_BLANCO);

            // Títulos: Tamaño e Interlineado
            C2D_Text tLblSz, tLblSp;
            C2D_TextParse(&tLblSz, dynamicBuf, "Tamaño"); C2D_TextOptimize(&tLblSz);
            C2D_DrawText(&tLblSz, C2D_WithColor, 54.0f, 44.0f, 0.6f, 0.35f, 0.35f, COLOR_NEGRO);

            C2D_TextParse(&tLblSp, dynamicBuf, "Interlineado"); C2D_TextOptimize(&tLblSp);
            C2D_DrawText(&tLblSp, C2D_WithColor, 198.0f, 44.0f, 0.6f, 0.35f, 0.35f, COLOR_NEGRO);

            auto drawBoxControl = [&](float bx, float by, float bw, float bh, const char* txt, bool selected) {
                C2D_DrawRectSolid(bx, by, 0.55f, bw, bh, COLOR_BLANCO);
                u32 border = selected ? COLOR_MUSGO : COLOR_NEGRO;
                float thick = selected ? 2.2f : 1.3f;
                C2D_DrawRectSolid(bx, by, 0.60f, bw, thick, border);
                C2D_DrawRectSolid(bx, by + bh - thick, 0.60f, bw, thick, border);
                C2D_DrawRectSolid(bx, by, 0.60f, thick, bh, border);
                C2D_DrawRectSolid(bx + bw - thick, by, 0.60f, thick, bh, border);

                C2D_Text tB; C2D_TextParse(&tB, dynamicBuf, txt); C2D_TextOptimize(&tB);
                float tw = 0, th = 0; C2D_TextGetDimensions(&tB, 0.40f, 0.40f, &tw, &th);
                C2D_DrawText(&tB, C2D_WithColor, bx + (bw - tw) / 2.0f, by + 5.0f, 0.65f, 0.40f, 0.40f, COLOR_NEGRO);
            };

            char szStr[16], spStr[16];
            snprintf(szStr, sizeof(szStr), "%d", activeStyle.fontSize);
            snprintf(spStr, sizeof(spStr), "%.1f", lineSpacingMult);

            drawBoxControl(24.0f,  62.0f, 30.0f, 26.0f, "-", formatMenuRow == 0 && formatMenuCol == 0);
            drawBoxControl(58.0f,  62.0f, 42.0f, 26.0f, szStr, false);
            drawBoxControl(104.0f, 62.0f, 30.0f, 26.0f, "+", formatMenuRow == 0 && formatMenuCol == 1);

            drawBoxControl(176.0f, 62.0f, 30.0f, 26.0f, "<", formatMenuRow == 0 && formatMenuCol == 2);
            drawBoxControl(210.0f, 62.0f, 42.0f, 26.0f, spStr, false);
            drawBoxControl(256.0f, 62.0f, 30.0f, 26.0f, ">", formatMenuRow == 0 && formatMenuCol == 3);

            // Paleta de Color de Texto (Pasteles opacos)
            C2D_Text tColTxt; C2D_TextParse(&tColTxt, dynamicBuf, "Color de Texto:"); C2D_TextOptimize(&tColTxt);
            C2D_DrawText(&tColTxt, C2D_WithColor, 24.0f, 98.0f, 0.6f, 0.35f, 0.35f, COLOR_NEGRO);

            for (int i = 0; i < 6; i++) {
                float sx = 24.0f + i * 46.0f;
                float sy = 116.0f;
                C2D_DrawRectSolid(sx, sy, 0.55f, 38.0f, 26.0f, textPastels[i]);
                bool isSel = (formatMenuRow == 1 && formatMenuCol == i) || (activeStyle.textColor == textPastels[i]);
                u32 bCol = (formatMenuRow == 1 && formatMenuCol == i) ? COLOR_MUSGO : COLOR_NEGRO;
                float bTh = isSel ? 2.2f : 1.2f;
                C2D_DrawRectSolid(sx, sy, 0.60f, 38.0f, bTh, bCol);
                C2D_DrawRectSolid(sx, sy + 26.0f - bTh, 0.60f, 38.0f, bTh, bCol);
                C2D_DrawRectSolid(sx, sy, 0.60f, bTh, 26.0f, bCol);
                C2D_DrawRectSolid(sx + 38.0f - bTh, sy, 0.60f, bTh, 26.0f, bCol);
            }

            // Paleta de Color de Fondo (Pasteles opacos)
            C2D_Text tColBg; C2D_TextParse(&tColBg, dynamicBuf, "Color de Fondo:"); C2D_TextOptimize(&tColBg);
            C2D_DrawText(&tColBg, C2D_WithColor, 24.0f, 150.0f, 0.6f, 0.35f, 0.35f, COLOR_NEGRO);

            for (int i = 0; i < 6; i++) {
                float sx = 24.0f + i * 46.0f;
                float sy = 168.0f;
                C2D_DrawRectSolid(sx, sy, 0.55f, 38.0f, 26.0f, bgPastels[i]);
                bool isSel = (formatMenuRow == 2 && formatMenuCol == i) || (activeStyle.bgColor == bgPastels[i]);
                u32 bCol = (formatMenuRow == 2 && formatMenuCol == i) ? COLOR_MUSGO : COLOR_NEGRO;
                float bTh = isSel ? 2.2f : 1.2f;
                C2D_DrawRectSolid(sx, sy, 0.60f, 38.0f, bTh, bCol);
                C2D_DrawRectSolid(sx, sy + 26.0f - bTh, 0.60f, 38.0f, bTh, bCol);
                C2D_DrawRectSolid(sx, sy, 0.60f, bTh, 26.0f, bCol);
                C2D_DrawRectSolid(sx + 38.0f - bTh, sy, 0.60f, bTh, 26.0f, bCol);
            }

            C2D_Text tFootF; C2D_TextParse(&tFootF, dynamicBuf, "D-PAD / Stylus para elegir  |  B / X para volver"); C2D_TextOptimize(&tFootF);
            C2D_DrawText(&tFootF, C2D_WithColor, 24.0f, 206.0f, 0.6f, 0.30f, 0.30f, COLOR_GRIS_BORDE);

        } else if (menuAbierto == 2) {
            // =================================================================
            // MENÚ DE INICIO (START)
            // =================================================================
            C2D_DrawRectSolid(10, 10, 0.4f, 300, 222, COLOR_BLANCO);
            C2D_DrawRectSolid(10, 10, 0.5f, 300, 2, COLOR_MUSGO);
            C2D_DrawRectSolid(10, 230, 0.5f, 300, 2, COLOR_MUSGO);
            C2D_DrawRectSolid(10, 10, 0.5f, 2, 222, COLOR_MUSGO);
            C2D_DrawRectSolid(308, 10, 0.5f, 2, 222, COLOR_MUSGO);

            C2D_Text tTitleM; C2D_TextParse(&tTitleM, dynamicBuf, "MENÚ PRINCIPAL"); C2D_TextOptimize(&tTitleM);
            C2D_DrawText(&tTitleM, C2D_WithColor, 28.0f, 18.0f, 0.6f, 0.44f, 0.44f, COLOR_MUSGO);

            // Cruz roja táctil
            C2D_DrawRectSolid(280, 14, 0.55f, 24, 24, COLOR_ROJO);
            C2D_Text tClose; C2D_TextParse(&tClose, dynamicBuf, "X"); C2D_TextOptimize(&tClose);
            float cw = 0, ch = 0; C2D_TextGetDimensions(&tClose, 0.42f, 0.42f, &cw, &ch);
            C2D_DrawText(&tClose, C2D_WithColor, 280.0f + (24.0f - cw) / 2.0f, 18.0f, 0.65f, 0.42f, 0.42f, COLOR_BLANCO);

            const char* opts[5] = {"GUARDAR", "GUARDAR COMO...", "ABRIR", "MANUAL DEL USUARIO", "SALIR DEL PROGRAMA"};
            const float itemY[5] = {48.0f, 80.0f, 112.0f, 144.0f, 176.0f};
            for (int i = 0; i < 5; i++) {
                bool sel = (startMenuSel == i);
                C2D_DrawRectSolid(30.0f, itemY[i], 0.55f, 260.0f, 26.0f, sel ? COLOR_MUSGO : COLOR_PAPEL);
                C2D_Text tOpt; C2D_TextParse(&tOpt, dynamicBuf, opts[i]); C2D_TextOptimize(&tOpt);
                float ow = 0, oh = 0; C2D_TextGetDimensions(&tOpt, 0.40f, 0.40f, &ow, &oh);
                C2D_DrawText(&tOpt, C2D_WithColor, 30.0f + (260.0f - ow) / 2.0f, itemY[i] + 5.0f, 0.65f, 0.40f, 0.40f, sel ? COLOR_BLANCO : COLOR_NEGRO);
            }

            C2D_Text tCopyM; C2D_TextParse(&tCopyM, dynamicBuf, "© Laura Dev 2026"); C2D_TextOptimize(&tCopyM);
            float cpw = 0, cph = 0; C2D_TextGetDimensions(&tCopyM, 0.32f, 0.32f, &cpw, &cph);
            C2D_DrawText(&tCopyM, C2D_WithColor, (320.0f - cpw) / 2.0f, 211.0f, 0.6f, 0.32f, 0.32f, COLOR_GRIS_BORDE);

        } else if (menuAbierto == 3 || menuAbierto == 4) {
            // =================================================================
            // MANUAL DEL USUARIO — PANTALLA INFERIOR
            // =================================================================
            C2D_DrawRectSolid(8, 8, 0.4f, 304, 224, COLOR_BLANCO);
            C2D_DrawRectSolid(8, 8, 0.5f, 304, 2, COLOR_MUSGO);
            C2D_DrawRectSolid(8, 230, 0.5f, 304, 2, COLOR_MUSGO);
            C2D_DrawRectSolid(8, 8, 0.5f, 2, 224, COLOR_MUSGO);
            C2D_DrawRectSolid(310, 8, 0.5f, 2, 224, COLOR_MUSGO);

            // Cruz roja táctil
            C2D_DrawRectSolid(282, 12, 0.55f, 22, 22, COLOR_ROJO);
            C2D_Text tClose; C2D_TextParse(&tClose, dynamicBuf, "X"); C2D_TextOptimize(&tClose);
            float cw = 0, ch = 0; C2D_TextGetDimensions(&tClose, 0.38f, 0.38f, &cw, &ch);
            C2D_DrawText(&tClose, C2D_WithColor, 282.0f + (22.0f - cw) / 2.0f, 15.0f, 0.65f, 0.38f, 0.38f, COLOR_BLANCO);

            bool breath = ((osGetTime() / 320) % 2 == 0);

            if (menuAbierto == 3) {
                // Página 1 Inferior: Dos columnas de atajos + frase final literal con «colores» en verde musgo
                const char* col1[6] = {
                    "• Seleccionar: *SELECT*",
                    "• Copiar selección: *Y*",
                    "• Cortar selección: *L + Y*",
                    "• Pegar selección: *A*",
                    "• Pegar sin formato: *L + A*",
                    "• Salto de línea (Enter): *R*"
                };
                const char* col2[5] = {
                    "• Salto de página: *ZL + R*",
                    "• Retroceso: *ZR*",
                    "• Borrar palabra: *L + ZR*",
                    "• Borrar línea: *ZL + ZR*",
                    "• Menú de inicio: *START*"
                };
                for (int i = 0; i < 6; i++) {
                    drawRichWrappedText(dynamicBuf, col1[i], 16.0f, 16.0f + i * 15.5f, 145.0f, 0.34f, 14.0f, COLOR_NEGRO);
                }
                for (int i = 0; i < 5; i++) {
                    drawRichWrappedText(dynamicBuf, col2[i], 162.0f, 16.0f + i * 15.5f, 142.0f, 0.34f, 14.0f, COLOR_NEGRO);
                }

                std::string p1BotClosing =
                    "*Laura* cuenta funciones como seleccionar tamaño de *fuente*, *interlineado* o *colores* para el texto y su fondo mediante el botón *Ajustes de Formato*, en la parte superior de la pantalla táctil.";
                drawRichWrappedText(dynamicBuf, p1BotClosing, 16.0f, 114.0f, 286.0f, 0.36f, 14.5f, COLOR_NEGRO);

                // Botón Siguiente >
                u32 btnNextCol = (manualMenuSel == 0 && breath) ? COLOR_MUSGO_CLARO : COLOR_MUSGO;
                C2D_DrawRectSolid(190, 196, 0.55f, 110, 26, btnNextCol);
                C2D_Text tNext; C2D_TextParse(&tNext, dynamicBuf, "Siguiente >"); C2D_TextOptimize(&tNext);
                float nw = 0, nh = 0; C2D_TextGetDimensions(&tNext, 0.38f, 0.38f, &nw, &nh);
                C2D_DrawText(&tNext, C2D_WithColor, 190.0f + (110.0f - nw) / 2.0f, 202.0f, 0.65f, 0.38f, 0.38f, COLOR_BLANCO);
            } else {
                // Página 2 Inferior: ÚNICAMENTE Información de contacto en tamaño pequeño y verde musgo
                const char* contactLines[4] = {
                    "Info de Contacto:",
                    "Fabián Contreras (CL)",
                    "fabian.contreras.abara@gmail.com",
                    "https://github.com/fabianquilpue"
                };
                for (int i = 0; i < 4; i++) {
                    C2D_Text tC;
                    C2D_TextParse(&tC, dynamicBuf, contactLines[i]);
                    C2D_TextOptimize(&tC);
                    C2D_DrawText(&tC, C2D_WithColor, 22.0f, 28.0f + i * 20.0f, 0.65f, 0.35f, 0.35f, COLOR_MUSGO);
                }

                // Botones < Anterior y Cerrar
                u32 colPrev = (manualMenuSel == 0) ? (breath ? COLOR_MUSGO_CLARO : COLOR_MUSGO) : COLOR_GRIS_TECLA;
                u32 colClos = (manualMenuSel == 1) ? (breath ? COLOR_MUSGO_CLARO : COLOR_MUSGO) : COLOR_GRIS_TECLA;

                C2D_DrawRectSolid(22, 194, 0.55f, 115, 26, colPrev);
                C2D_Text tPrev; C2D_TextParse(&tPrev, dynamicBuf, "< Anterior"); C2D_TextOptimize(&tPrev);
                float pw = 0, ph = 0; C2D_TextGetDimensions(&tPrev, 0.38f, 0.38f, &pw, &ph);
                C2D_DrawText(&tPrev, C2D_WithColor, 22.0f + (115.0f - pw) / 2.0f, 200.0f, 0.65f, 0.38f, 0.38f, manualMenuSel == 0 ? COLOR_BLANCO : COLOR_NEGRO);

                C2D_DrawRectSolid(183, 194, 0.55f, 115, 26, colClos);
                C2D_Text tCl; C2D_TextParse(&tCl, dynamicBuf, "Cerrar"); C2D_TextOptimize(&tCl);
                float clw = 0, clh = 0; C2D_TextGetDimensions(&tCl, 0.38f, 0.38f, &clw, &clh);
                C2D_DrawText(&tCl, C2D_WithColor, 183.0f + (115.0f - clw) / 2.0f, 200.0f, 0.65f, 0.38f, 0.38f, manualMenuSel == 1 ? COLOR_BLANCO : COLOR_NEGRO);
            }

        } else if (menuAbierto == 5) {
            // =================================================================
            // SUBMENÚ ABRIR ARCHIVO (.RTF)
            // =================================================================
            C2D_DrawRectSolid(10, 10, 0.4f, 300, 220, COLOR_BLANCO);
            C2D_DrawRectSolid(10, 10, 0.5f, 300, 2, COLOR_MUSGO);
            C2D_DrawRectSolid(10, 228, 0.5f, 300, 2, COLOR_MUSGO);
            C2D_DrawRectSolid(10, 10, 0.5f, 2, 220, COLOR_MUSGO);
            C2D_DrawRectSolid(308, 10, 0.5f, 2, 220, COLOR_MUSGO);

            C2D_Text tOpenH; C2D_TextParse(&tOpenH, dynamicBuf, "ABRIR DOCUMENTO (.RTF)"); C2D_TextOptimize(&tOpenH);
            C2D_DrawText(&tOpenH, C2D_WithColor, 24.0f, 18.0f, 0.6f, 0.40f, 0.40f, COLOR_MUSGO);

            C2D_DrawRectSolid(280, 14, 0.55f, 24, 24, COLOR_ROJO);
            C2D_Text tClose; C2D_TextParse(&tClose, dynamicBuf, "X"); C2D_TextOptimize(&tClose);
            float cw = 0, ch = 0; C2D_TextGetDimensions(&tClose, 0.42f, 0.42f, &cw, &ch);
            C2D_DrawText(&tClose, C2D_WithColor, 280.0f + (24.0f - cw) / 2.0f, 18.0f, 0.65f, 0.42f, 0.42f, COLOR_BLANCO);

            if (fileList.empty()) {
                C2D_Text tEmpty; C2D_TextParse(&tEmpty, dynamicBuf, "No se encontraron archivos .rtf\nen sdmc:/Textos3DS/"); C2D_TextOptimize(&tEmpty);
                C2D_DrawText(&tEmpty, C2D_WithColor, 32.0f, 95.0f, 0.6f, 0.38f, 0.38f, COLOR_GRIS_BORDE);
            } else {
                for (int i = 0; i < 5 && (fileScroll + i) < (int)fileList.size(); i++) {
                    float fy = 52.0f + i * 32.0f;
                    bool sel = (openMenuSel == i);
                    C2D_DrawRectSolid(24.0f, fy, 0.55f, 272.0f, 26.0f, sel ? COLOR_MUSGO : COLOR_PAPEL);
                    C2D_Text tFn; C2D_TextParse(&tFn, dynamicBuf, fileList[fileScroll + i].c_str()); C2D_TextOptimize(&tFn);
                    C2D_DrawText(&tFn, C2D_WithColor, 34.0f, fy + 5.0f, 0.65f, 0.38f, 0.38f, sel ? COLOR_BLANCO : COLOR_NEGRO);
                }
            }

        } else if (menuAbierto == 6 || menuAbierto == 7) {
            // =================================================================
            // MODALES DE CONFIRMACIÓN (SÍ / NO CON EFECTO RESPIRACIÓN)
            // =================================================================
            C2D_DrawRectSolid(24, 42, 0.45f, 272, 152, COLOR_BLANCO);
            C2D_DrawRectSolid(24, 42, 0.55f, 272, 28, COLOR_MUSGO);
            C2D_DrawRectSolid(24, 42, 0.60f, 272, 2, COLOR_MUSGO);
            C2D_DrawRectSolid(24, 192, 0.60f, 272, 2, COLOR_MUSGO);
            C2D_DrawRectSolid(24, 42, 0.60f, 2, 152, COLOR_MUSGO);
            C2D_DrawRectSolid(294, 42, 0.60f, 2, 152, COLOR_MUSGO);

            C2D_Text mTitle; C2D_TextParse(&mTitle, dynamicBuf, "ATENCIÓN"); C2D_TextOptimize(&mTitle);
            C2D_DrawText(&mTitle, C2D_WithColor, 36.0f, 48.0f, 0.65f, 0.40f, 0.40f, COLOR_BLANCO);

            // Cruz roja táctil
            C2D_DrawRectSolid(266, 46, 0.65f, 22, 22, COLOR_ROJO);
            C2D_Text tClose; C2D_TextParse(&tClose, dynamicBuf, "X"); C2D_TextOptimize(&tClose);
            float cw = 0, ch = 0; C2D_TextGetDimensions(&tClose, 0.38f, 0.38f, &cw, &ch);
            C2D_DrawText(&tClose, C2D_WithColor, 266.0f + (22.0f - cw) / 2.0f, 49.0f, 0.70f, 0.38f, 0.38f, COLOR_BLANCO);

            C2D_Text mMsg;
            if (menuAbierto == 6) {
                C2D_TextParse(&mMsg, dynamicBuf, "¿Deseas abrir este archivo?\nSe perderán los cambios no guardados.");
            } else {
                C2D_TextParse(&mMsg, dynamicBuf, "¿Deseas salir del programa?\nSe perderán los cambios no guardados.");
            }
            C2D_TextOptimize(&mMsg);
            C2D_DrawText(&mMsg, C2D_WithColor, 36.0f, 84.0f, 0.65f, 0.36f, 0.36f, COLOR_NEGRO);

            bool breath = ((osGetTime() / 300) % 2 == 0);
            u32 cSi = (confirmSel == 0 && breath) ? COLOR_MUSGO_CLARO : COLOR_MUSGO;
            u32 cNo = (confirmSel == 1 && breath) ? C2D_Color32(235, 110, 110, 255) : COLOR_ROJO;

            C2D_DrawRectSolid(60, 142, 0.60f, 80, 28, cSi);
            C2D_Text tSi; C2D_TextParse(&tSi, dynamicBuf, "SÍ"); C2D_TextOptimize(&tSi);
            float sw = 0, sh = 0; C2D_TextGetDimensions(&tSi, 0.42f, 0.42f, &sw, &sh);
            C2D_DrawText(&tSi, C2D_WithColor, 60.0f + (80.0f - sw) / 2.0f, 148.0f, 0.68f, 0.42f, 0.42f, COLOR_BLANCO);

            C2D_DrawRectSolid(180, 142, 0.60f, 80, 28, cNo);
            C2D_Text tNo; C2D_TextParse(&tNo, dynamicBuf, "NO"); C2D_TextOptimize(&tNo);
            float nw = 0, nh = 0; C2D_TextGetDimensions(&tNo, 0.42f, 0.42f, &nw, &nh);
            C2D_DrawText(&tNo, C2D_WithColor, 180.0f + (80.0f - nw) / 2.0f, 148.0f, 0.68f, 0.42f, 0.42f, COLOR_BLANCO);
        }

        C3D_FrameEnd(0);
    }

    if (dynamicBuf) C2D_TextBufDelete(dynamicBuf);
}
