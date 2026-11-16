#include <3ds.h>
#include <citro2d.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>

// ============================================================================
// PALETA DE COLORES OFICIAL (ESTÉTICA PAPEL CÁLIDO Y VERDE MUSGO)
// ============================================================================
#define COLOR_CREMA          C2D_Color32(247, 245, 238, 255) // #F7F5EE
#define COLOR_BLANCO         C2D_Color32(255, 255, 255, 255) // #FFFFFF
#define COLOR_NEGRO          C2D_Color32(30, 30, 30, 255)    // #1E1E1E
#define COLOR_MUSGO          C2D_Color32(78, 122, 101, 255)  // #4E7A65
#define COLOR_MUSGO_SUAVE    C2D_Color32(214, 228, 221, 255)
#define COLOR_GRIS_FONDO     C2D_Color32(236, 234, 228, 255)
#define COLOR_GRIS_TECLA     C2D_Color32(252, 251, 248, 255)
#define COLOR_BORDE_TECLA    C2D_Color32(196, 193, 185, 255)
#define COLOR_GRIS_BTN       C2D_Color32(212, 210, 204, 255)
#define COLOR_GRIS_TEXTO     C2D_Color32(120, 120, 120, 255)
#define COLOR_ROJO_ERR       C2D_Color32(205, 55, 55, 255)
#define COLOR_ROJO_PASTEL    C2D_Color32(235, 145, 145, 255)
#define COLOR_SELECCION      C2D_Color32(185, 215, 200, 255)

// ============================================================================
// TEXTOS OFICIALES E INMUTABLES DEL MANUAL DEL USUARIO
// ============================================================================
static const char* MANUAL_P1_TOP_TITLE = "*GUÍA DE DIGITACIÓN Y NAVEGACIÓN:*";
static const char* MANUAL_P1_TOP_P1 =
    "Utiliza el *d-pad* para navegar intuitivamente por el documento. "
    "Puedes apretar *SELECT* para seleccionar texto mientras te desplazas.";
static const char* MANUAL_P1_TOP_P2 =
    "También puedes copiar, cortar y pegar el texto, entre otras funciones, "
    "muchas de las cuales serán accesibles mediante la función de shift (*L*) "
    "o de alt (*ZL*):";

static const std::vector<std::string> MANUAL_P1_BOT_COL1 = {
    "• Seleccionar: *SELECT*",
    "• Copiar selección: *Y*",
    "• Cortar selección: *L + Y*",
    "• Pegar selección: *A*",
    "• Pegar sin formato: *L + A*",
    "• Salto de línea (Enter): *R*"
};

static const std::vector<std::string> MANUAL_P1_BOT_COL2 = {
    "• Salto de página: *ZL + R*",
    "• Retroceso: *ZR*",
    "• Borrar palabra: *L + ZR*",
    "• Borrar línea: *ZL + ZR*",
    "• Menú de inicio: *START*"
};

static const char* MANUAL_P1_BOT_FOOTER =
    "*Laura* cuenta funciones como seleccionar tamaño de *fuente*, "
    "*interlineado* o *colores* para el texto y su fondo mediante el botón "
    "*Ajustes de Formato*, en la parte superior de la pantalla táctil.";

static const char* MANUAL_P2_TOP_P1 =
    "«Laura» es un procesador de textos que reúne las funciones esenciales "
    "necesarias para la escritura sin distracciones. Su diseño se inspira en "
    "la sencillez de las máquinas de escribir y en la ergonomía. Las puntuaciones "
    "están pensadas para ser accesibles de manera fácil e intuitiva.";

static const char* MANUAL_P2_TOP_P2 =
    "El programa cuenta con casi la totalidad de caracteres de un teclado QWERTY "
    "convencional, y tiene la capacidad de exportar sus archivos en formato RTF, "
    "permitiendo un mayor control sobre el estilo y la estructura del texto. El "
    "proyecto no persigue fines de lucro, pero tampoco puede ser comercializado "
    "o monetizado al acogerse a Creative Commons.";

static const std::vector<std::string> MANUAL_P2_BOT_CONTACT = {
    "Info de Contacto:",
    "Fabián Contreras (CL)",
    "fabian.contreras.abara@gmail.com",
    "https://github.com/fabianquilpue"
};

// ============================================================================
// ESTRUCTURAS DE DATOS DEL DOCUMENTO Y TECLADO
// ============================================================================
enum class Alignment { Left, Center, Right, Justified };

struct StyledChar {
    std::string utf8;
    bool bold = false;
    bool underline = false;
    bool strike = false;
    u32 textColor = COLOR_NEGRO;
    u32 bgColor = COLOR_CREMA;
    int fontSize = 12;
};

struct VirtualKey {
    float x, y, w, h;
    std::string normalLabel;
    std::string shiftLabel;
    std::string altLabel;
    std::string normalOut;
    std::string shiftOut;
    std::string altOut;
    int keyType; // 0=char, 1=backspace, 2=space, 3=dead_acute, 4=dead_tilde
};

enum class ModalState {
    None,
    StartMenu,
    FormatMenu,
    Manual,
    OpenFile,
    ConfirmNew,
    ConfirmExit,
    ConfirmOverwrite
};

// ============================================================================
// FUNCIONES AUXILIARES DE RENDERIZADO Y TEXTO
// ============================================================================
static void drawBorderedBox(float x, float y, float w, float h, float z, u32 fillColor, u32 borderColor, float borderThickness = 1.0f) {
    C2D_DrawRectSolid(x, y, z, w, h, borderColor);
    if (w > borderThickness * 2.0f && h > borderThickness * 2.0f) {
        C2D_DrawRectSolid(x + borderThickness, y + borderThickness, z + 0.01f,
                          w - borderThickness * 2.0f, h - borderThickness * 2.0f, fillColor);
    }
}

static void drawCenteredText(C2D_TextBuf buf, const std::string& str, float x, float y, float w, float h,
                             float z, float scaleX, float scaleY, u32 color, bool bold = false) {
    if (str.empty()) return;
    C2D_Text txt;
    C2D_TextParse(&txt, buf, str.c_str());
    C2D_TextOptimize(&txt);
    float tw = 0.0f, th = 0.0f;
    C2D_TextGetDimensions(&txt, scaleX, scaleY, &tw, &th);
    float dx = x + (w - tw) * 0.5f;
    float dy = y + (h - th) * 0.5f;
    C2D_DrawText(&txt, C2D_WithColor, dx, dy, z, scaleX, scaleY, color);
    if (bold) {
        C2D_DrawText(&txt, C2D_WithColor, dx + 0.65f, dy, z, scaleX, scaleY, color);
    }
}

// Renderizador de párrafos del manual con soporte para *negrita*, palabra «colores» en verde musgo y word-wrap
static float drawRichManualParagraph(C2D_TextBuf buf, const std::string& rawText,
                                     float startX, float startY, float maxWidth,
                                     float scale, float lineHeight, float z,
                                     u32 defaultColor, u32 mossColor) {
    struct RichWord {
        std::string text;
        bool bold;
        bool isMoss;
    };

    std::vector<RichWord> words;
    std::string currentWord;
    bool inBold = false;

    auto flushWord = [&]() {
        if (currentWord.empty()) return;
        std::string clean;
        for (char c : currentWord) {
            if (c != ',' && c != '.' && c != ':' && c != ';' && c != '(' && c != ')' && c != ' ') {
                clean += (char)tolower((unsigned char)c);
            }
        }
        bool moss = (clean == "colores");
        words.push_back({currentWord, inBold, moss});
        currentWord.clear();
    };

    for (size_t i = 0; i < rawText.size(); ++i) {
        char c = rawText[i];
        if (c == '*') {
            flushWord();
            inBold = !inBold;
        } else if (c == ' ') {
            currentWord += ' ';
            flushWord();
        } else {
            currentWord += c;
        }
    }
    flushWord();

    float curX = startX;
    float curY = startY;

    for (const auto& w : words) {
        C2D_Text txt;
        C2D_TextParse(&txt, buf, w.text.c_str());
        C2D_TextOptimize(&txt);

        float tw = 0.0f, th = 0.0f;
        C2D_TextGetDimensions(&txt, scale, scale, &tw, &th);

        // Si el salto de línea ocurre justo con espacio al final, recortamos visualmente el ancho
        std::string trimmed = w.text;
        while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
        C2D_Text trimTxt;
        C2D_TextParse(&trimTxt, buf, trimmed.c_str());
        C2D_TextOptimize(&trimTxt);
        float trimW = 0.0f, trimH = 0.0f;
        C2D_TextGetDimensions(&trimTxt, scale, scale, &trimW, &trimH);

        if (curX > startX && (curX + trimW) > (startX + maxWidth)) {
            curX = startX;
            curY += lineHeight;
        }

        u32 col = w.isMoss ? mossColor : defaultColor;
        C2D_DrawText(&txt, C2D_WithColor, curX, curY, z, scale, scale, col);
        if (w.bold) {
            C2D_DrawText(&txt, C2D_WithColor, curX + 0.65f, curY, z, scale, scale, col);
        }

        curX += tw;
    }

    return curY + lineHeight;
}

// Dibuja el emblema oficial de Laura (hoja crema con esquina doblada, "L" carbón y cursor "|" verde musgo)
static void drawLauraEmblem(C2D_TextBuf buf, float cx, float cy, float size, float z) {
    float x = cx - size * 0.5f;
    float y = cy - size * 0.5f;
    float fold = size * 0.24f;
    u32 borderCol = C2D_Color32(130, 130, 125, 255);
    u32 foldCol = C2D_Color32(224, 221, 212, 255);

    // Base de hoja crema cálido
    drawBorderedBox(x, y, size, size, z, COLOR_CREMA, borderCol, 1.5f);

    // Esquina superior derecha doblada
    C2D_DrawTriangle(x + size - fold, y, COLOR_BLANCO,
                     x + size, y, COLOR_BLANCO,
                     x + size, y + fold, COLOR_BLANCO, z + 0.02f);
    C2D_DrawTriangle(x + size - fold, y, foldCol,
                     x + size - fold, y + fold, foldCol,
                     x + size, y + fold, foldCol, z + 0.03f);
    C2D_DrawLine(x + size - fold, y, borderCol, x + size - fold, y + fold, borderCol, 1.2f, z + 0.04f);
    C2D_DrawLine(x + size - fold, y + fold, borderCol, x + size, y + fold, borderCol, 1.2f, z + 0.04f);
    C2D_DrawLine(x + size - fold, y, borderCol, x + size, y + fold, borderCol, 1.2f, z + 0.04f);

    // Letra "L" en negro tinta carbón
    C2D_Text lTxt;
    C2D_TextParse(&lTxt, buf, "L");
    C2D_TextOptimize(&lTxt);
    float lScale = size / 44.0f;
    float lw = 0.0f, lh = 0.0f;
    C2D_TextGetDimensions(&lTxt, lScale, lScale, &lw, &lh);
    float lx = cx - lw * 0.72f;
    float ly = cy - lh * 0.52f;
    C2D_DrawText(&lTxt, C2D_WithColor, lx, ly, z + 0.05f, lScale, lScale, COLOR_NEGRO);
    C2D_DrawText(&lTxt, C2D_WithColor, lx + 1.0f, ly, z + 0.05f, lScale, lScale, COLOR_NEGRO);

    // Cursor vertical "I" en verde musgo
    float curX = lx + lw + size * 0.08f;
    float curY = cy - size * 0.24f;
    float curH = size * 0.48f;
    C2D_DrawRectSolid(curX, curY, z + 0.05f, 3.0f, curH, COLOR_MUSGO);
}

// ============================================================================
// COMBINACIÓN DE TECLAS MUERTAS EN ESPAÑOL
// ============================================================================
static std::string combineDeadKey(int deadMode, const std::string& base) {
    if (deadMode == 1) { // Agudo (´)
        if (base == "a") return "á";
        if (base == "e") return "é";
        if (base == "i") return "í";
        if (base == "o") return "ó";
        if (base == "u") return "ú";
        if (base == "A") return "Á";
        if (base == "E") return "É";
        if (base == "I") return "Í";
        if (base == "O") return "Ó";
        if (base == "U") return "Ú";
        return "´" + base;
    } else if (deadMode == 2) { // Diéresis (¨)
        if (base == "u") return "ü";
        if (base == "U") return "Ü";
        return "¨" + base;
    } else if (deadMode == 3) { // Virgulilla (~)
        if (base == "n") return "ñ";
        if (base == "N") return "Ñ";
        return "~" + base;
    }
    return base;
}

// ============================================================================
// CLASE PRINCIPAL DEL EDITOR LAURA
// ============================================================================
class LauraEditor {
public:
    LauraEditor() {
        doc.clear();
        cursorPos = 0;
        selectionAnchor = -1;
        scrollOffsetY = 0.0f;

        activeBold = false;
        italicError = false;
        activeUnderline = false;
        activeStrike = false;
        activeTextColor = COLOR_NEGRO;
        activeBgColor = COLOR_CREMA;
        pageBgColor = COLOR_CREMA;
        activeFontSize = 12;
        alignment = Alignment::Left;
        lineSpacingIdx = 0; // 1.0, 1.2, 1.5, 2.0
        showPilcrow = false;
        showHud = true;

        capsLock = false;
        lastLPressTime = 0;
        deadKeyState = 0; // 0=off, 1=acute, 2=diaeresis, 3=tilde

        modal = ModalState::None;
        menuSelection = 0;
        manualPage = 1;
        manualBtnSelection = 0;
        confirmSelection = 1; // "NO" seleccionado por defecto
        fileListSelection = 0;
        currentFilename = "Documento_01.rtf";
        fileCounter = 1;

        buildKeyboard();
    }

    void init() {
        mkdir("sdmc:/Textos3DS", 0777);
        refreshFileList();
    }

    void run() {
        C3D_RenderTarget* topScreen = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
        C3D_RenderTarget* bottomScreen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
        C2D_TextBuf dynamicBuf = C2D_TextBufNew(16384);

        // --------------------------------------------------------------------
        // PANTALLA DE BIENVENIDA (SPLASH SCREEN CON FADE FROM/TO BLACK)
        // Permanece indefinidamente hasta recibir cualquier input físico o táctil
        // --------------------------------------------------------------------
        {
            C2D_SpriteSheet splashSheet = C2D_SpriteSheetLoad("romfs:/splash.t3x");
            C2D_Image splashImg = {0};
            bool hasSplash = (splashSheet != nullptr);
            if (hasSplash) {
                splashImg = C2D_SpriteSheetGetImage(splashSheet, 0);
            }

            bool splashActive = true;
            int fadeAlpha = 255;   // Inicia completamente en negro (Fade from black)
            bool fadingIn = true;
            bool fadingOut = false;

            while (aptMainLoop() && splashActive) {
                // Limpiar buffer de texto en cada fotograma para evitar saturación en espera larga
                C2D_TextBufClear(dynamicBuf);

                hidScanInput();
                u32 kDown = hidKeysDown();

                // Solo inicia fade-to-black cuando se detecta cualquier pulsación o toque
                if (!fadingOut && kDown != 0) {
                    fadingIn = false;
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

                C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

                // --- Pantalla Superior ---
                C2D_TargetClear(topScreen, COLOR_BLANCO);
                C2D_SceneBegin(topScreen);

                if (hasSplash) {
                    C2D_DrawImageAt(splashImg, (400.0f - 84.0f) * 0.5f, 18.0f, 0.5f, nullptr, 1.0f, 1.0f);
                } else {
                    drawLauraEmblem(dynamicBuf, 200.0f, 58.0f, 76.0f, 0.5f);
                }

                drawCenteredText(dynamicBuf, "Laura", 0.0f, 106.0f, 400.0f, 34.0f, 0.6f, 1.15f, 1.15f, COLOR_NEGRO, true);
                drawCenteredText(dynamicBuf, "Procesador de textos para la Nintendo 3DS", 0.0f, 146.0f, 400.0f, 20.0f, 0.6f, 0.44f, 0.44f, COLOR_NEGRO, false);
                drawCenteredText(dynamicBuf, "Word processor for the Nintendo 3DS", 0.0f, 166.0f, 400.0f, 20.0f, 0.6f, 0.42f, 0.42f, COLOR_GRIS_TEXTO, false);
                drawCenteredText(dynamicBuf, "© Laura Dev 2026", 0.0f, 208.0f, 400.0f, 18.0f, 0.6f, 0.35f, 0.35f, COLOR_GRIS_TEXTO, false);

                if (fadeAlpha > 0) {
                    C2D_DrawRectSolid(0.0f, 0.0f, 0.99f, 400.0f, 240.0f, C2D_Color32(0, 0, 0, fadeAlpha));
                }

                // --- Pantalla Inferior ---
                C2D_TargetClear(bottomScreen, COLOR_BLANCO);
                C2D_SceneBegin(bottomScreen);

                if (!fadingOut) {
                    float pulse = 0.55f + 0.45f * sinf((float)osGetTime() * 0.005f);
                    u8 textAlpha = (u8)(pulse * 255.0f);
                    u32 promptCol = C2D_Color32(78, 122, 101, textAlpha);
                    drawCenteredText(dynamicBuf, "Toca la pantalla o presiona un botón para continuar",
                                     10.0f, 104.0f, 300.0f, 28.0f, 0.6f, 0.40f, 0.40f, promptCol, true);
                }

                if (fadeAlpha > 0) {
                    C2D_DrawRectSolid(0.0f, 0.0f, 0.99f, 320.0f, 240.0f, C2D_Color32(0, 0, 0, fadeAlpha));
                }

                C3D_FrameEnd(0);
            }

            if (splashSheet) {
                C2D_SpriteSheetFree(splashSheet);
            }
            C2D_TextBufClear(dynamicBuf);
        }

        // --------------------------------------------------------------------
        // BUCLE PRINCIPAL DEL PROCESADOR DE TEXTOS
        // --------------------------------------------------------------------
        bool running = true;
        while (aptMainLoop() && running) {
            C2D_TextBufClear(dynamicBuf);

            hidScanInput();
            u32 kDown = hidKeysDown();
            u32 kHeld = hidKeysHeld();
            touchPosition touch;
            hidTouchRead(&touch);

            bool shiftActive = (kHeld & KEY_L) || capsLock;
            bool altActive = (kHeld & KEY_ZL) != 0;

            // Doble toque rápido en L activa/desactiva CAPS LOCK
            if (kDown & KEY_L) {
                u64 now = osGetTime();
                if (now - lastLPressTime < 300) {
                    capsLock = !capsLock;
                    lastLPressTime = 0;
                } else {
                    lastLPressTime = now;
                }
            }

            // Gestión de entrada según estado modal activo
            if (modal == ModalState::Manual) {
                handleManualInput(kDown, touch);
            } else if (modal == ModalState::StartMenu) {
                handleStartMenuInput(kDown, touch);
            } else if (modal == ModalState::FormatMenu) {
                handleFormatMenuInput(kDown, touch);
            } else if (modal == ModalState::OpenFile) {
                handleOpenFileInput(kDown, touch);
            } else if (modal == ModalState::ConfirmNew || modal == ModalState::ConfirmExit || modal == ModalState::ConfirmOverwrite) {
                handleConfirmModalInput(kDown, touch, running);
            } else {
                handleEditorInput(kDown, kHeld, shiftActive, altActive, touch);
            }

            // Renderizado de ambas pantallas
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            // PANTALLA SUPERIOR
            C2D_TargetClear(topScreen, pageBgColor);
            C2D_SceneBegin(topScreen);
            if (modal == ModalState::Manual) {
                renderManualTop(dynamicBuf);
            } else {
                renderEditorTop(dynamicBuf);
            }

            // PANTALLA INFERIOR
            C2D_TargetClear(bottomScreen, COLOR_GRIS_FONDO);
            C2D_SceneBegin(bottomScreen);
            if (modal == ModalState::Manual) {
                renderManualBottom(dynamicBuf);
            } else {
                renderKeyboardAndToolbar(dynamicBuf, shiftActive, altActive, kHeld);
                if (modal == ModalState::StartMenu) {
                    renderStartMenuModal(dynamicBuf);
                } else if (modal == ModalState::FormatMenu) {
                    renderFormatMenuModal(dynamicBuf);
                } else if (modal == ModalState::OpenFile) {
                    renderOpenFileModal(dynamicBuf);
                } else if (modal == ModalState::ConfirmNew || modal == ModalState::ConfirmExit || modal == ModalState::ConfirmOverwrite) {
                    renderConfirmModal(dynamicBuf);
                }
            }

            C3D_FrameEnd(0);
        }

        C2D_TextBufDelete(dynamicBuf);
    }

private:
    std::vector<StyledChar> doc;
    std::vector<std::vector<StyledChar>> undoStack;
    std::vector<std::vector<StyledChar>> redoStack;
    std::vector<StyledChar> clipboard;

    size_t cursorPos;
    int selectionAnchor;
    float scrollOffsetY;

    bool activeBold;
    bool italicError;
    bool activeUnderline;
    bool activeStrike;
    u32 activeTextColor;
    u32 activeBgColor;
    u32 pageBgColor;
    int activeFontSize;
    Alignment alignment;
    int lineSpacingIdx;
    bool showPilcrow;
    bool showHud;

    bool capsLock;
    u64 lastLPressTime;
    int deadKeyState;

    ModalState modal;
    int menuSelection;
    int manualPage;
    int manualBtnSelection;
    int confirmSelection;
    int fileListSelection;
    std::string currentFilename;
    int fileCounter;
    std::vector<std::string> sdFiles;
    std::vector<VirtualKey> keys;

    float getLineSpacingMultiplier() const {
        static const float spacings[4] = {1.0f, 1.2f, 1.5f, 2.0f};
        return spacings[lineSpacingIdx % 4];
    }

    void pushUndo() {
        if (undoStack.size() >= 40) undoStack.erase(undoStack.begin());
        undoStack.push_back(doc);
        redoStack.clear();
    }

    void performUndo(int steps = 1) {
        for (int i = 0; i < steps && !undoStack.empty(); ++i) {
            redoStack.push_back(doc);
            doc = undoStack.back();
            undoStack.pop_back();
            if (cursorPos > doc.size()) cursorPos = doc.size();
            selectionAnchor = -1;
        }
    }

    void performRedo() {
        if (!redoStack.empty()) {
            undoStack.push_back(doc);
            doc = redoStack.back();
            redoStack.pop_back();
            if (cursorPos > doc.size()) cursorPos = doc.size();
            selectionAnchor = -1;
        }
    }

    void getSelectionRange(size_t& startIdx, size_t& endIdx) const {
        if (selectionAnchor < 0 || (size_t)selectionAnchor == cursorPos) {
            startIdx = endIdx = cursorPos;
            return;
        }
        size_t anc = std::min((size_t)selectionAnchor, doc.size());
        startIdx = std::min(anc, cursorPos);
        endIdx = std::max(anc, cursorPos);
    }

    void deleteSelectionIfAny() {
        size_t s, e;
        getSelectionRange(s, e);
        if (s < e) {
            doc.erase(doc.begin() + s, doc.begin() + e);
            cursorPos = s;
            selectionAnchor = -1;
        }
    }

    void insertString(const std::string& utf8Str) {
        pushUndo();
        deleteSelectionIfAny();
        StyledChar sc;
        sc.utf8 = utf8Str;
        sc.bold = activeBold;
        sc.underline = activeUnderline;
        sc.strike = activeStrike;
        sc.textColor = activeTextColor;
        sc.bgColor = activeBgColor;
        sc.fontSize = activeFontSize;
        doc.insert(doc.begin() + cursorPos, sc);
        cursorPos++;
    }

    void buildKeyboard() {
        keys.clear();
        float marginX = 4.0f;
        float gap = 2.0f;
        float keyW = (320.0f - marginX * 2.0f - gap * 9.0f) / 10.0f;
        float keyH = 34.0f;
        float y0 = 33.0f;

        // Fila 1: Números 1..0
        const char* numN[10] = {"1","2","3","4","5","6","7","8","9","0"};
        const char* numL[10] = {"*","\"","#","$","%","&","/","<",">","="};
        const char* numZ[10] = {"|","@","·","~","½","¬","{","^","}","\\"};
        for (int i = 0; i < 10; ++i) {
            keys.push_back({marginX + i * (keyW + gap), y0, keyW, keyH,
                            numN[i], numL[i], numZ[i], numN[i], numL[i], numZ[i], 0});
        }

        // Fila 2: QWERTYUIOP
        const char* r2[10] = {"q","w","e","r","t","y","u","i","o","p"};
        const char* r2U[10] = {"Q","W","E","R","T","Y","U","I","O","P"};
        float y1 = y0 + keyH + 3.0f;
        for (int i = 0; i < 10; ++i) {
            keys.push_back({marginX + i * (keyW + gap), y1, keyW, keyH,
                            r2[i], r2U[i], r2U[i], r2[i], r2U[i], r2U[i], 0});
        }

        // Fila 3: ASDFGHJKLÑ
        const char* r3[10] = {"a","s","d","f","g","h","j","k","l","ñ"};
        const char* r3U[10] = {"A","S","D","F","G","H","J","K","L","Ñ"};
        float y2 = y1 + keyH + 3.0f;
        for (int i = 0; i < 10; ++i) {
            int type = (i == 9) ? 4 : 0; // Ñ + ZL = tecla muerta ~
            keys.push_back({marginX + i * (keyW + gap), y2, keyW, keyH,
                            r3[i], r3U[i], (i == 9 ? "~" : r3U[i]),
                            r3[i], r3U[i], (i == 9 ? "~" : r3U[i]), type});
        }

        // Fila 4: Tecla muerta (´/¨), ZXCVBNM, y DEL
        float y3 = y2 + keyH + 3.0f;
        keys.push_back({marginX, y3, keyW, keyH, "´", "¨", "^", "´", "¨", "^", 3});
        const char* r4[7] = {"z","x","c","v","b","n","m"};
        const char* r4U[7] = {"Z","X","C","V","B","N","M"};
        for (int i = 0; i < 7; ++i) {
            keys.push_back({marginX + (i + 1) * (keyW + gap), y3, keyW, keyH,
                            r4[i], r4U[i], r4U[i], r4[i], r4U[i], r4U[i], 0});
        }
        float delX = marginX + 8 * (keyW + gap);
        float delW = 320.0f - marginX - delX;
        keys.push_back({delX, y3, delW, keyH, "BACK", "L:WORD", "ZL:LINE", "", "", "", 1});

        // Fila 5: Puntuación + Barra Espaciadora
        float y4 = y3 + keyH + 3.0f;
        float row5H = 38.0f;
        float pW = 22.0f;
        // Izquierda de barra espaciadora: « ¿ ¡ ( -
        const char* pLeftN[5] = {"«", "¿", "¡", "(", "-"};
        const char* pLeftL[5] = {"»", "?", "!", "[", "_"};
        const char* pLeftZ[5] = {"“", "¿", "¡", "{", "—"};
        for (int i = 0; i < 5; ++i) {
            keys.push_back({marginX + i * (pW + 1.5f), y4, pW, row5H,
                            pLeftN[i], pLeftL[i], pLeftZ[i], pLeftN[i], pLeftL[i], pLeftZ[i], 0});
        }

        // Barra espaciadora central
        float spX = marginX + 5 * (pW + 1.5f) + 1.0f;
        float spW = 84.0f;
        keys.push_back({spX, y4, spW, row5H, "ESPACIO", "->| ESPACIO", "=-> PÁRRAFO", " ", "\t", "    ", 2});

        // Derecha de barra espaciadora: ‘ ) ! ? »
        const char* pRightN[5] = {"‘", ")", "!", "?", "»"};
        const char* pRightL[5] = {"’", "]", "¡", "¿", "«"};
        const char* pRightZ[5] = {"'", "}", "|", "·", "”"};
        float rStart = spX + spW + 2.5f;
        for (int i = 0; i < 5; ++i) {
            keys.push_back({rStart + i * (pW + 1.5f), y4, pW, row5H,
                            pRightN[i], pRightL[i], pRightZ[i], pRightN[i], pRightL[i], pRightZ[i], 0});
        }
    }

    // ------------------------------------------------------------------------
    // MANEJO DE ENTRADA PRINCIPAL DEL EDITOR
    // ------------------------------------------------------------------------
    void handleEditorInput(u32 kDown, u32 kHeld, bool shiftActive, bool altActive, touchPosition touch) {
        // Abrir Menú de Inicio con START
        if (kDown & KEY_START) {
            modal = ModalState::StartMenu;
            menuSelection = 0; // "GUARDAR" seleccionado por defecto
            return;
        }

        // Toggle HUD con X
        if (kDown & KEY_X) {
            showHud = !showHud;
        }

        // Toggle Selección con SELECT
        if (kDown & KEY_SELECT) {
            if (selectionAnchor < 0) selectionAnchor = (int)cursorPos;
            else selectionAnchor = -1;
        }

        // Navegación D-Pad
        if (kDown & KEY_LEFT) {
            if (kHeld & KEY_L) {
                while (cursorPos > 0 && doc[cursorPos - 1].utf8 == " ") cursorPos--;
                while (cursorPos > 0 && doc[cursorPos - 1].utf8 != " " && doc[cursorPos - 1].utf8 != "\n") cursorPos--;
            } else if (cursorPos > 0) {
                cursorPos--;
            }
        }
        if (kDown & KEY_RIGHT) {
            if (kHeld & KEY_L) {
                while (cursorPos < doc.size() && doc[cursorPos].utf8 != " " && doc[cursorPos].utf8 != "\n") cursorPos++;
                while (cursorPos < doc.size() && doc[cursorPos].utf8 == " ") cursorPos++;
            } else if (cursorPos < doc.size()) {
                cursorPos++;
            }
        }
        if (kDown & KEY_UP) {
            if (kHeld & KEY_L) {
                if (cursorPos > 0) cursorPos--;
                while (cursorPos > 0 && doc[cursorPos - 1].utf8 != "\n") cursorPos--;
            } else {
                cursorPos = (cursorPos > 32) ? cursorPos - 32 : 0;
            }
        }
        if (kDown & KEY_DOWN) {
            if (kHeld & KEY_L) {
                while (cursorPos < doc.size() && doc[cursorPos].utf8 != "\n") cursorPos++;
                if (cursorPos < doc.size()) cursorPos++;
            } else {
                cursorPos = std::min(doc.size(), cursorPos + 32);
            }
        }

        // Enter (R) y Salto de página (ZL + R)
        if (kDown & KEY_R) {
            if (altActive) {
                insertString("\f");
            } else {
                insertString("\n");
            }
        }

        // Retroceso (ZR), Borrar palabra (L + ZR), Borrar línea (ZL + ZR)
        if (kDown & KEY_ZR) {
            if (altActive) {
                pushUndo();
                while (cursorPos > 0 && doc[cursorPos - 1].utf8 != "\n") {
                    doc.erase(doc.begin() + cursorPos - 1);
                    cursorPos--;
                }
            } else if (kHeld & KEY_L) {
                pushUndo();
                while (cursorPos > 0 && doc[cursorPos - 1].utf8 == " ") {
                    doc.erase(doc.begin() + cursorPos - 1);
                    cursorPos--;
                }
                while (cursorPos > 0 && doc[cursorPos - 1].utf8 != " " && doc[cursorPos - 1].utf8 != "\n") {
                    doc.erase(doc.begin() + cursorPos - 1);
                    cursorPos--;
                }
            } else {
                performBackspace();
            }
        }

        // Copiar (Y) / Cortar (L + Y)
        if (kDown & KEY_Y) {
            size_t s, e;
            getSelectionRange(s, e);
            if (s < e) {
                clipboard.assign(doc.begin() + s, doc.begin() + e);
                if (kHeld & KEY_L) {
                    pushUndo();
                    deleteSelectionIfAny();
                }
            }
        }

        // Pegar (A) / Pegar sin formato (L + A o ZL + A)
        if (kDown & KEY_A) {
            if (!clipboard.empty()) {
                pushUndo();
                deleteSelectionIfAny();
                bool plain = (kHeld & KEY_L) || altActive;
                for (const auto& item : clipboard) {
                    StyledChar sc = item;
                    if (plain) {
                        sc.bold = activeBold;
                        sc.underline = activeUnderline;
                        sc.strike = activeStrike;
                        sc.textColor = activeTextColor;
                        sc.bgColor = activeBgColor;
                        sc.fontSize = activeFontSize;
                    }
                    doc.insert(doc.begin() + cursorPos, sc);
                    cursorPos++;
                }
            }
        }

        // Deshacer (B / L + B) y Rehacer (ZL + B)
        if (kDown & KEY_B) {
            if (altActive) performRedo();
            else if (kHeld & KEY_L) performUndo(5);
            else performUndo(1);
        }

        // Toques en barra de formato superior o teclado
        if (kDown & KEY_TOUCH) {
            // Barra de formato superior (y: 3..28)
            if (touch.py >= 3 && touch.py <= 28) {
                if (touch.px >= 4 && touch.px <= 36) { // N (Negrita)
                    activeBold = !activeBold;
                    applyStyleToSelection();
                } else if (touch.px >= 40 && touch.px <= 76) { // K -> ERR (Cursiva)
                    italicError = !italicError;
                } else if (touch.px >= 80 && touch.px <= 116) { // S / T / S/T
                    if (altActive) {
                        bool both = !(activeUnderline && activeStrike);
                        activeUnderline = both;
                        activeStrike = both;
                    } else if (kHeld & KEY_L) {
                        activeStrike = !activeStrike;
                    } else {
                        activeUnderline = !activeUnderline;
                    }
                    applyStyleToSelection();
                } else if (touch.px >= 120 && touch.px <= 164) { // Alineación
                    alignment = (Alignment)(((int)alignment + 1) % 4);
                } else if (touch.px >= 168 && touch.px <= 212) { // Interlineado
                    lineSpacingIdx = (lineSpacingIdx + 1) % 4;
                } else if (touch.px >= 216 && touch.px <= 248) { // Calderón ¶
                    showPilcrow = !showPilcrow;
                } else if (touch.px >= 254 && touch.px <= 316) { // Aa (Ajustes de Formato)
                    modal = ModalState::FormatMenu;
                }
                return;
            }

            // Teclado virtual
            for (const auto& k : keys) {
                if (touch.px >= k.x && touch.px <= k.x + k.w &&
                    touch.py >= k.y && touch.py <= k.y + k.h) {
                    if (k.keyType == 1) {
                        performBackspace();
                    } else if (k.keyType == 3) {
                        if (altActive) deadKeyState = 0;
                        else if (shiftActive) deadKeyState = (deadKeyState == 2) ? 0 : 2;
                        else deadKeyState = (deadKeyState == 1) ? 0 : 1;
                    } else if (k.keyType == 4 && altActive) {
                        deadKeyState = (deadKeyState == 3) ? 0 : 3;
                    } else {
                        std::string out = altActive ? k.altOut : (shiftActive ? k.shiftOut : k.normalOut);
                        if (deadKeyState != 0) {
                            if (out == " ") {
                                const char* solo[4] = {"", "´", "¨", "~"};
                                insertString(solo[deadKeyState]);
                            } else {
                                insertString(combineDeadKey(deadKeyState, out));
                            }
                            deadKeyState = 0;
                        } else {
                            insertString(out);
                        }
                    }
                    break;
                }
            }
        }
    }

    void performBackspace() {
        size_t s, e;
        getSelectionRange(s, e);
        if (s < e) {
            pushUndo();
            deleteSelectionIfAny();
        } else if (cursorPos > 0) {
            pushUndo();
            doc.erase(doc.begin() + cursorPos - 1);
            cursorPos--;
        }
    }

    void applyStyleToSelection() {
        size_t s, e;
        getSelectionRange(s, e);
        if (s < e) {
            pushUndo();
            for (size_t i = s; i < e; ++i) {
                doc[i].bold = activeBold;
                doc[i].underline = activeUnderline;
                doc[i].strike = activeStrike;
                doc[i].textColor = activeTextColor;
                doc[i].bgColor = activeBgColor;
                doc[i].fontSize = activeFontSize;
            }
        }
    }

    // ------------------------------------------------------------------------
    // RENDERIZADO DE PANTALLA SUPERIOR DEL EDITOR
    // ------------------------------------------------------------------------
    void renderEditorTop(C2D_TextBuf buf) {
        float topMargin = showHud ? 24.0f : 10.0f;
        float bottomLimit = showHud ? 216.0f : 230.0f;

        if (showHud) {
            C2D_DrawRectSolid(0, 0, 0.7f, 400, 18, COLOR_MUSGO);
            u8 bat = 5;
            PTMU_GetBatteryLevel(&bat);
            int pct = std::min(100, (int)bat * 20);
            char leftHud[32];
            snprintf(leftHud, sizeof(leftHud), "Bat: %d%%", pct);

            time_t now = time(NULL);
            struct tm* tmInfo = localtime(&now);
            char rightHud[32];
            snprintf(rightHud, sizeof(rightHud), "%02d/%02d/%02d %02d:%02d",
                     tmInfo->tm_mday, tmInfo->tm_mon + 1, tmInfo->tm_year % 100,
                     tmInfo->tm_hour, tmInfo->tm_min);

            C2D_Text tL, tC, tR;
            C2D_TextParse(&tL, buf, leftHud); C2D_TextOptimize(&tL);
            C2D_TextParse(&tC, buf, currentFilename.c_str()); C2D_TextOptimize(&tC);
            C2D_TextParse(&tR, buf, rightHud); C2D_TextOptimize(&tR);

            C2D_DrawText(&tL, C2D_WithColor, 6.0f, 3.0f, 0.75f, 0.36f, 0.36f, COLOR_BLANCO);
            drawCenteredText(buf, currentFilename, 80.0f, 1.0f, 240.0f, 16.0f, 0.75f, 0.36f, 0.36f, COLOR_BLANCO, true);
            float rw = 0, rh = 0;
            C2D_TextGetDimensions(&tR, 0.36f, 0.36f, &rw, &rh);
            C2D_DrawText(&tR, C2D_WithColor, 394.0f - rw, 3.0f, 0.75f, 0.36f, 0.36f, COLOR_BLANCO);
        }

        // Renderizado fluido del documento con word-wrap y estilos
        float marginX = 14.0f;
        float maxW = 400.0f - marginX * 2.0f;
        float curX = marginX;
        float curY = topMargin - scrollOffsetY;

        size_t selS, selE;
        getSelectionRange(selS, selE);

        for (size_t i = 0; i <= doc.size(); ++i) {
            // Dibujar cursor en su posición exacta
            if (i == cursorPos && curY >= topMargin - 10.0f && curY <= bottomLimit) {
                C2D_DrawRectSolid(curX, curY, 0.65f, 2.0f, 15.0f, COLOR_MUSGO);
            }
            if (i == doc.size()) break;

            const StyledChar& sc = doc[i];
            float scale = (float)sc.fontSize / 24.0f;
            float lineStep = (16.0f * (scale / 0.5f)) * getLineSpacingMultiplier();

            if (sc.utf8 == "\n") {
                if (showPilcrow && curY >= topMargin - 10.0f && curY <= bottomLimit) {
                    C2D_Text pTxt;
                    C2D_TextParse(&pTxt, buf, "¶"); C2D_TextOptimize(&pTxt);
                    C2D_DrawText(&pTxt, C2D_WithColor, curX + 2.0f, curY, 0.5f, scale, scale, COLOR_MUSGO);
                }
                curX = marginX;
                curY += lineStep;
                continue;
            }
            if (sc.utf8 == "\f") {
                curX = marginX;
                curY += lineStep;
                if (curY >= topMargin - 10.0f && curY <= bottomLimit) {
                    drawCenteredText(buf, "--- [PÁGINA NUEVA] ---", marginX, curY, maxW, 14.0f, 0.5f, 0.38f, 0.38f, COLOR_MUSGO, true);
                }
                curY += lineStep;
                continue;
            }

            std::string disp = (sc.utf8 == "\t") ? "    " : sc.utf8;
            C2D_Text chTxt;
            C2D_TextParse(&chTxt, buf, disp.c_str());
            C2D_TextOptimize(&chTxt);
            float cw = 0.0f, ch = 0.0f;
            C2D_TextGetDimensions(&chTxt, scale, scale, &cw, &ch);

            if (curX > marginX && curX + cw > marginX + maxW) {
                curX = marginX;
                curY += lineStep;
            }

            if (curY >= topMargin - 10.0f && curY <= bottomLimit) {
                if (i >= selS && i < selE) {
                    C2D_DrawRectSolid(curX, curY, 0.4f, cw, ch, COLOR_SELECCION);
                } else if (sc.bgColor != pageBgColor) {
                    C2D_DrawRectSolid(curX, curY, 0.4f, cw, ch, sc.bgColor);
                }

                C2D_DrawText(&chTxt, C2D_WithColor, curX, curY, 0.5f, scale, scale, sc.textColor);
                if (sc.bold) {
                    C2D_DrawText(&chTxt, C2D_WithColor, curX + 0.7f, curY, 0.5f, scale, scale, sc.textColor);
                }
                if (sc.underline) {
                    C2D_DrawLine(curX, curY + ch - 1.5f, sc.textColor, curX + cw, curY + ch - 1.5f, sc.textColor, 1.2f, 0.55f);
                }
                if (sc.strike) {
                    C2D_DrawLine(curX, curY + ch * 0.52f, sc.textColor, curX + cw, curY + ch * 0.52f, sc.textColor, 1.3f, 0.55f);
                }
            }
            curX += cw;
        }

        // Auto-scroll suave si el cursor baja del límite visible
        if (curY > bottomLimit - 14.0f) {
            scrollOffsetY += (curY - (bottomLimit - 14.0f));
        } else if (curY < topMargin && scrollOffsetY > 0.0f) {
            scrollOffsetY = std::max(0.0f, scrollOffsetY - (topMargin - curY));
        }

        if (showHud) {
            C2D_DrawRectSolid(0, 222, 0.7f, 400, 18, COLOR_MUSGO);
            int words = 0;
            bool inWord = false;
            for (const auto& c : doc) {
                if (c.utf8 == " " || c.utf8 == "\n" || c.utf8 == "\t" || c.utf8 == "\f") {
                    inWord = false;
                } else if (!inWord) {
                    inWord = true;
                    words++;
                }
            }
            int pages = std::max(1, (words / 250) + 1);
            char botHud[64];
            snprintf(botHud, sizeof(botHud), "Palabras: %d  |  Caracteres: %d  |  Pág. est.: %d",
                     words, (int)doc.size(), pages);
            drawCenteredText(buf, botHud, 0.0f, 223.0f, 400.0f, 16.0f, 0.75f, 0.36f, 0.36f, COLOR_BLANCO, false);
        }
    }

    // ------------------------------------------------------------------------
    // RENDERIZADO DE PANTALLA INFERIOR (BARRA DE FORMATO Y TECLADO)
    // ------------------------------------------------------------------------
    void renderKeyboardAndToolbar(C2D_TextBuf buf, bool shiftActive, bool altActive, u32 kHeld) {
        // Barra superior de formato (y: 3..28)
        float ty = 3.0f, th = 25.0f;

        // N (Negrita)
        drawBorderedBox(4, ty, 32, th, 0.3f, activeBold ? COLOR_MUSGO : COLOR_GRIS_BTN, COLOR_BORDE_TECLA, 1.0f);
        drawCenteredText(buf, "N", 4, ty, 32, th, 0.4f, 0.48f, 0.48f, activeBold ? COLOR_BLANCO : COLOR_NEGRO, activeBold);

        // K -> ERR (Cursiva deshabilitada visualmente como tecla mala)
        drawBorderedBox(40, ty, 36, th, 0.3f, italicError ? COLOR_ROJO_ERR : COLOR_GRIS_BTN, COLOR_BORDE_TECLA, 1.0f);
        drawCenteredText(buf, italicError ? "ERR" : "K", 40, ty, 36, th, 0.4f, 0.44f, 0.44f, italicError ? COLOR_BLANCO : COLOR_NEGRO, italicError);

        // S / T / S/T (Subrayado / Tachado)
        bool stActive = activeUnderline || activeStrike;
        const char* stLbl = altActive ? "S/T" : ((kHeld & KEY_L) ? "T" : "S");
        drawBorderedBox(80, ty, 36, th, 0.3f, stActive ? COLOR_MUSGO : COLOR_GRIS_BTN, COLOR_BORDE_TECLA, 1.0f);
        drawCenteredText(buf, stLbl, 80, ty, 36, th, 0.4f, 0.46f, 0.46f, stActive ? COLOR_BLANCO : COLOR_NEGRO, stActive);

        // Alineación (Siempre activa en verde musgo)
        const char* alLbl = (alignment == Alignment::Left) ? "IZQ" :
                            (alignment == Alignment::Center) ? "CEN" :
                            (alignment == Alignment::Right) ? "DER" : "JUS";
        drawBorderedBox(120, ty, 44, th, 0.3f, COLOR_MUSGO, COLOR_BORDE_TECLA, 1.0f);
        drawCenteredText(buf, alLbl, 120, ty, 44, th, 0.4f, 0.40f, 0.40f, COLOR_BLANCO, true);

        // Interlineado (Siempre en verde musgo bajo cualquier circunstancia)
        const char* intLbl[4] = {"1.0", "1.2", "1.5", "2.0"};
        drawBorderedBox(168, ty, 44, th, 0.3f, COLOR_MUSGO, COLOR_BORDE_TECLA, 1.0f);
        drawCenteredText(buf, intLbl[lineSpacingIdx % 4], 168, ty, 44, th, 0.4f, 0.42f, 0.42f, COLOR_BLANCO, true);

        // Calderón ¶
        drawBorderedBox(216, ty, 32, th, 0.3f, showPilcrow ? COLOR_MUSGO : COLOR_GRIS_BTN, COLOR_BORDE_TECLA, 1.0f);
        drawCenteredText(buf, "¶", 216, ty, 32, th, 0.4f, 0.48f, 0.48f, showPilcrow ? COLOR_BLANCO : COLOR_NEGRO, showPilcrow);

        // Botón Ajustes de Formato (Aa) con borde verde musgo y triángulo superior derecho
        drawBorderedBox(254, ty, 62, th, 0.3f, COLOR_GRIS_TECLA, COLOR_MUSGO, 1.3f);
        drawCenteredText(buf, "Aa", 254, ty, 56, th, 0.4f, 0.48f, 0.48f, COLOR_NEGRO, true);
        C2D_DrawTriangle(306.0f, ty + 3.0f, COLOR_MUSGO,
                         313.0f, ty + 3.0f, COLOR_MUSGO,
                         313.0f, ty + 10.0f, COLOR_MUSGO, 0.45f);

        // Dibujo de todas las teclas del teclado
        for (const auto& k : keys) {
            u32 fill = COLOR_GRIS_TECLA;
            u32 txtCol = COLOR_NEGRO;

            if (k.keyType == 3 && (deadKeyState == 1 || deadKeyState == 2)) {
                fill = COLOR_MUSGO;
                txtCol = COLOR_BLANCO;
            } else if (k.keyType == 4 && deadKeyState == 3) {
                fill = COLOR_MUSGO;
                txtCol = COLOR_BLANCO;
            }

            drawBorderedBox(k.x, k.y, k.w, k.h, 0.3f, fill, COLOR_BORDE_TECLA, 1.0f);

            std::string lbl = altActive ? k.altLabel : (shiftActive ? k.shiftLabel : k.normalLabel);
            float scale = (k.keyType == 1 || k.keyType == 2) ? 0.38f : 0.54f;

            if (k.keyType == 2) {
                // Barra espaciadora con íconos laterales
                drawCenteredText(buf, lbl, k.x + 8.0f, k.y, k.w - 16.0f, k.h, 0.4f, 0.38f, 0.38f, txtCol, activeBold);
                if (!shiftActive && !altActive) {
                    drawCenteredText(buf, "->|", k.x + 2.0f, k.y, 18.0f, k.h, 0.4f, 0.30f, 0.30f, COLOR_MUSGO, false);
                    drawCenteredText(buf, "=->", k.x + k.w - 20.0f, k.y, 18.0f, k.h, 0.4f, 0.30f, 0.30f, COLOR_MUSGO, false);
                }
            } else {
                drawCenteredText(buf, lbl, k.x, k.y, k.w, k.h, 0.4f, scale, scale, txtCol, activeBold);
            }
        }
    }

    // ------------------------------------------------------------------------
    // MANUAL DEL USUARIO (2 PÁGINAS CON TEXTO OFICIAL INMUTABLE)
    // ------------------------------------------------------------------------
    void handleManualInput(u32 kDown, touchPosition touch) {
        if (kDown & KEY_B) {
            modal = ModalState::StartMenu;
            return;
        }
        if (kDown & KEY_LEFT) manualBtnSelection = 0;
        if (kDown & KEY_RIGHT) manualBtnSelection = 1;

        auto activateBtn = [&](int idx) {
            if (idx == 0) {
                manualPage = (manualPage == 1) ? 2 : 1;
                manualBtnSelection = 0;
            } else {
                modal = ModalState::StartMenu;
            }
        };

        if (kDown & KEY_A) {
            activateBtn(manualBtnSelection);
            return;
        }

        if (kDown & KEY_TOUCH) {
            if (touch.py >= 200 && touch.py <= 230) {
                if (touch.px >= 24 && touch.px <= 148) {
                    if (manualBtnSelection == 0) activateBtn(0);
                    else manualBtnSelection = 0;
                } else if (touch.px >= 172 && touch.px <= 296) {
                    if (manualBtnSelection == 1) activateBtn(1);
                    else manualBtnSelection = 1;
                }
            }
        }
    }

    void renderManualTop(C2D_TextBuf buf) {
        C2D_TargetClear(C2D_GetScreenTop(), COLOR_CREMA);

        // Franja superior verde musgo de borde a borde
        C2D_DrawRectSolid(0.0f, 0.0f, 0.5f, 400.0f, 24.0f, COLOR_MUSGO);
        const char* header = (manualPage == 1) ? "MANUAL DEL USUARIO 1/2" : "MANUAL DEL USUARIO 2/2";
        drawCenteredText(buf, header, 0.0f, 0.0f, 400.0f, 24.0f, 0.6f, 0.48f, 0.48f, COLOR_BLANCO, true);

        if (manualPage == 1) {
            float y = 34.0f; // Margen superior limpio
            y = drawRichManualParagraph(buf, MANUAL_P1_TOP_TITLE, 14.0f, y, 372.0f, 0.50f, 18.0f, 0.6f, COLOR_NEGRO, COLOR_MUSGO);
            y += 4.0f;
            y = drawRichManualParagraph(buf, MANUAL_P1_TOP_P1, 14.0f, y, 372.0f, 0.49f, 17.5f, 0.6f, COLOR_NEGRO, COLOR_MUSGO);
            y += 6.0f;
            drawRichManualParagraph(buf, MANUAL_P1_TOP_P2, 14.0f, y, 372.0f, 0.49f, 17.5f, 0.6f, COLOR_NEGRO, COLOR_MUSGO);
        } else {
            // Página 2 Superior: Únicamente la reseña institucional en una sola columna
            float y = 34.0f;
            y = drawRichManualParagraph(buf, MANUAL_P2_TOP_P1, 14.0f, y, 372.0f, 0.48f, 17.0f, 0.6f, COLOR_NEGRO, COLOR_MUSGO);
            y += 8.0f;
            drawRichManualParagraph(buf, MANUAL_P2_TOP_P2, 14.0f, y, 372.0f, 0.48f, 17.0f, 0.6f, COLOR_NEGRO, COLOR_MUSGO);
        }
    }

    void renderManualBottom(C2D_TextBuf buf) {
        C2D_TargetClear(C2D_GetScreenBottom(), COLOR_CREMA);

        if (manualPage == 1) {
            // Dos columnas para la lista de comandos en Página 1 Inferior
            float yLeft = 10.0f;
            for (const auto& item : MANUAL_P1_BOT_COL1) {
                yLeft = drawRichManualParagraph(buf, item, 8.0f, yLeft, 152.0f, 0.42f, 15.5f, 0.6f, COLOR_NEGRO, COLOR_MUSGO);
            }

            float yRight = 10.0f;
            for (const auto& item : MANUAL_P1_BOT_COL2) {
                yRight = drawRichManualParagraph(buf, item, 164.0f, yRight, 150.0f, 0.42f, 15.5f, 0.6f, COLOR_NEGRO, COLOR_MUSGO);
            }

            // Párrafo final con *colores* en verde musgo
            float yFoot = std::max(yLeft, yRight) + 6.0f;
            drawRichManualParagraph(buf, MANUAL_P1_BOT_FOOTER, 10.0f, yFoot, 300.0f, 0.43f, 15.5f, 0.6f, COLOR_NEGRO, COLOR_MUSGO);
        } else {
            // Página 2 Inferior: Únicamente información de contacto en fuente pequeña verde musgo
            float y = 32.0f;
            for (const auto& line : MANUAL_P2_BOT_CONTACT) {
                C2D_Text cTxt;
                C2D_TextParse(&cTxt, buf, line.c_str());
                C2D_TextOptimize(&cTxt);
                C2D_DrawText(&cTxt, C2D_WithColor, 18.0f, y, 0.6f, 0.39f, 0.39f, COLOR_MUSGO);
                y += 18.0f;
            }
        }

        // Botones de navegación inferiores (grises por defecto, titilan en verde musgo al seleccionarse)
        float pulse = 0.65f + 0.35f * sinf((float)osGetTime() * 0.008f);
        u32 activeMoss = C2D_Color32((u8)(78 * pulse), (u8)(122 * pulse), (u8)(101 * pulse), 255);

        const char* btn0Label = (manualPage == 1) ? "Siguiente" : "Anterior";
        bool sel0 = (manualBtnSelection == 0);
        bool sel1 = (manualBtnSelection == 1);

        drawBorderedBox(24.0f, 202.0f, 124.0f, 28.0f, 0.6f,
                        sel0 ? activeMoss : COLOR_GRIS_BTN,
                        sel0 ? COLOR_NEGRO : COLOR_BORDE_TECLA,
                        sel0 ? 1.5f : 1.0f);
        drawCenteredText(buf, btn0Label, 24.0f, 202.0f, 124.0f, 28.0f, 0.7f, 0.44f, 0.44f,
                         sel0 ? COLOR_BLANCO : COLOR_NEGRO, sel0);

        drawBorderedBox(172.0f, 202.0f, 124.0f, 28.0f, 0.6f,
                        sel1 ? activeMoss : COLOR_GRIS_BTN,
                        sel1 ? COLOR_NEGRO : COLOR_BORDE_TECLA,
                        sel1 ? 1.5f : 1.0f);
        drawCenteredText(buf, "Cerrar", 172.0f, 202.0f, 124.0f, 28.0f, 0.7f, 0.44f, 0.44f,
                         sel1 ? COLOR_BLANCO : COLOR_NEGRO, sel1);
    }

    // ------------------------------------------------------------------------
    // MENÚ DE INICIO (START)
    // ------------------------------------------------------------------------
    void handleStartMenuInput(u32 kDown, touchPosition touch) {
        if (kDown & KEY_B || kDown & KEY_START) {
            modal = ModalState::None;
            return;
        }
        if (kDown & KEY_UP) menuSelection = (menuSelection + 4) % 5;
        if (kDown & KEY_DOWN) menuSelection = (menuSelection + 1) % 5;

        auto triggerItem = [&](int idx) {
            if (idx == 0) { // GUARDAR
                saveRTF("sdmc:/Textos3DS/" + currentFilename);
                modal = ModalState::None;
            } else if (idx == 1) { // GUARDAR COMO (Nuevo nombre incremental)
                fileCounter++;
                char bufName[64];
                snprintf(bufName, sizeof(bufName), "Documento_%02d.rtf", fileCounter);
                currentFilename = bufName;
                saveRTF("sdmc:/Textos3DS/" + currentFilename);
                modal = ModalState::None;
            } else if (idx == 2) { // ABRIR ARCHIVO
                refreshFileList();
                fileListSelection = 0;
                modal = ModalState::OpenFile;
            } else if (idx == 3) { // MANUAL DE USUARIO
                manualPage = 1;
                manualBtnSelection = 0;
                modal = ModalState::Manual;
            } else if (idx == 4) { // SALIR DEL PROGRAMA
                confirmSelection = 1; // NO por defecto
                modal = ModalState::ConfirmExit;
            }
        };

        if (kDown & KEY_A) {
            triggerItem(menuSelection);
            return;
        }

        if (kDown & KEY_TOUCH) {
            // Cruz roja de cierre (estrictamente táctil, centrada en esquina superior derecha)
            if (touch.px >= 262 && touch.px <= 286 && touch.py >= 14 && touch.py <= 36) {
                modal = ModalState::None;
                return;
            }
            for (int i = 0; i < 5; ++i) {
                float by = 44.0f + i * 31.0f;
                if (touch.px >= 52 && touch.px <= 268 && touch.py >= by && touch.py <= by + 26.0f) {
                    if (menuSelection == i) triggerItem(i);
                    else menuSelection = i;
                    break;
                }
            }
        }
    }

    void renderStartMenuModal(C2D_TextBuf buf) {
        C2D_DrawRectSolid(0, 0, 0.75f, 320, 240, C2D_Color32(0, 0, 0, 110));
        drawBorderedBox(34, 12, 252, 218, 0.78f, COLOR_CREMA, COLOR_MUSGO, 2.0f);

        // Cabecera verde musgo
        C2D_DrawRectSolid(36, 14, 0.80f, 248, 22, COLOR_MUSGO);
        drawCenteredText(buf, "MENÚ DE INICIO", 36, 14, 226, 22, 0.82f, 0.42f, 0.42f, COLOR_BLANCO, true);

        // Cruz roja centrada
        C2D_DrawRectSolid(262, 14, 0.81f, 22, 22, COLOR_ROJO_ERR);
        drawCenteredText(buf, "X", 262, 14, 22, 22, 0.83f, 0.42f, 0.42f, COLOR_BLANCO, true);

        const char* items[5] = {
            "GUARDAR",
            "GUARDAR COMO",
            "ABRIR ARCHIVO",
            "MANUAL DE USUARIO",
            "SALIR DEL PROGRAMA"
        };

        float pulse = 0.65f + 0.35f * sinf((float)osGetTime() * 0.008f);
        u32 activeMoss = C2D_Color32((u8)(78 * pulse), (u8)(122 * pulse), (u8)(101 * pulse), 255);

        for (int i = 0; i < 5; ++i) {
            float by = 44.0f + i * 31.0f;
            bool sel = (menuSelection == i);
            drawBorderedBox(52, by, 216, 26, 0.80f,
                            sel ? activeMoss : COLOR_GRIS_TECLA,
                            sel ? COLOR_NEGRO : COLOR_BORDE_TECLA,
                            sel ? 1.5f : 1.0f);
            drawCenteredText(buf, items[i], 52, by, 216, 26, 0.82f, 0.42f, 0.42f,
                             sel ? COLOR_BLANCO : COLOR_NEGRO, sel);
        }

        drawCenteredText(buf, "© Laura Dev 2026", 36, 206, 248, 18, 0.82f, 0.34f, 0.34f, COLOR_GRIS_TEXTO, false);
    }

    // ------------------------------------------------------------------------
    // MENÚ AJUSTES DE FORMATO (Aa) — D-PAD DESACTIVADO, TAMAÑO E INTERLINEADO LADO A LADO
    // ------------------------------------------------------------------------
    void handleFormatMenuInput(u32 kDown, touchPosition touch) {
        if (kDown & KEY_B) {
            modal = ModalState::None;
            return;
        }
        if (kDown & KEY_TOUCH) {
            // Cruz roja de cierre
            if (touch.px >= 274 && touch.px <= 298 && touch.py >= 16 && touch.py <= 38) {
                modal = ModalState::None;
                return;
            }
            // Selector de Tamaño (Izquierda: x 28..150, y 60..88)
            if (touch.py >= 60 && touch.py <= 88) {
                if (touch.px >= 28 && touch.px <= 62 && activeFontSize > 8) {
                    activeFontSize -= 2;
                    applyStyleToSelection();
                } else if (touch.px >= 116 && touch.px <= 150 && activeFontSize < 24) {
                    activeFontSize += 2;
                    applyStyleToSelection();
                }
                // Selector de Interlineado (Derecha: x 170..292, y 60..88)
                if (touch.px >= 170 && touch.px <= 204 && lineSpacingIdx > 0) {
                    lineSpacingIdx--;
                } else if (touch.px >= 258 && touch.px <= 292 && lineSpacingIdx < 3) {
                    lineSpacingIdx++;
                }
            }

            // Paleta de colores pasteles para texto (y: 112..136) y fondo (y: 162..186)
            static const u32 palette[6] = {
                COLOR_NEGRO, COLOR_MUSGO,
                C2D_Color32(140, 175, 220, 255),
                C2D_Color32(225, 150, 170, 255),
                C2D_Color32(235, 215, 140, 255),
                COLOR_CREMA
            };
            for (int i = 0; i < 6; ++i) {
                float sx = 32.0f + i * 44.0f;
                if (touch.px >= sx && touch.px <= sx + 36.0f) {
                    if (touch.py >= 112 && touch.py <= 136) {
                        activeTextColor = palette[i];
                        applyStyleToSelection();
                    } else if (touch.py >= 162 && touch.py <= 186) {
                        activeBgColor = palette[i];
                        pageBgColor = palette[i];
                        if (palette[i] == COLOR_NEGRO) activeTextColor = COLOR_BLANCO;
                        applyStyleToSelection();
                    }
                }
            }
        }
    }

    void renderFormatMenuModal(C2D_TextBuf buf) {
        C2D_DrawRectSolid(0, 0, 0.75f, 320, 240, C2D_Color32(0, 0, 0, 110));
        drawBorderedBox(20, 14, 280, 206, 0.78f, COLOR_CREMA, COLOR_MUSGO, 2.0f);

        C2D_DrawRectSolid(22, 16, 0.80f, 276, 22, COLOR_MUSGO);
        drawCenteredText(buf, "AJUSTES DE FORMATO", 22, 16, 252, 22, 0.82f, 0.42f, 0.42f, COLOR_BLANCO, true);

        C2D_DrawRectSolid(274, 16, 0.81f, 24, 22, COLOR_ROJO_ERR);
        drawCenteredText(buf, "X", 274, 16, 24, 22, 0.83f, 0.42f, 0.42f, COLOR_BLANCO, true);

        // Títulos lado a lado: "Tamaño" e "interlineado"
        drawCenteredText(buf, "Tamaño", 28, 42, 122, 16, 0.82f, 0.38f, 0.38f, COLOR_NEGRO, true);
        drawCenteredText(buf, "interlineado", 170, 42, 122, 16, 0.82f, 0.38f, 0.38f, COLOR_NEGRO, true);

        // Selector Tamaño (A pequeña negrita | valor | A grande negrita, todos del mismo alto y ancho)
        drawBorderedBox(28, 60, 34, 28, 0.80f, COLOR_GRIS_TECLA, COLOR_NEGRO, 1.2f);
        drawCenteredText(buf, "A", 28, 60, 34, 28, 0.82f, 0.34f, 0.34f, COLOR_NEGRO, true);

        char szBuf[16];
        snprintf(szBuf, sizeof(szBuf), "%d", activeFontSize);
        drawBorderedBox(66, 60, 46, 28, 0.80f, COLOR_BLANCO, COLOR_MUSGO, 1.5f);
        drawCenteredText(buf, szBuf, 66, 60, 46, 28, 0.82f, 0.46f, 0.46f, COLOR_NEGRO, true);

        drawBorderedBox(116, 60, 34, 28, 0.80f, COLOR_GRIS_TECLA, COLOR_NEGRO, 1.2f);
        drawCenteredText(buf, "A", 116, 60, 34, 28, 0.82f, 0.56f, 0.56f, COLOR_NEGRO, true);

        // Selector Interlineado
        drawBorderedBox(170, 60, 34, 28, 0.80f, COLOR_GRIS_TECLA, COLOR_NEGRO, 1.2f);
        drawCenteredText(buf, "A", 170, 60, 34, 28, 0.82f, 0.34f, 0.34f, COLOR_NEGRO, true);

        const char* intLbl[4] = {"1.0", "1.2", "1.5", "2.0"};
        drawBorderedBox(208, 60, 46, 28, 0.80f, COLOR_BLANCO, COLOR_MUSGO, 1.5f);
        drawCenteredText(buf, intLbl[lineSpacingIdx % 4], 208, 60, 46, 28, 0.82f, 0.46f, 0.46f, COLOR_NEGRO, true);

        drawBorderedBox(258, 60, 34, 28, 0.80f, COLOR_GRIS_TECLA, COLOR_NEGRO, 1.2f);
        drawCenteredText(buf, "A", 258, 60, 34, 28, 0.82f, 0.56f, 0.56f, COLOR_NEGRO, true);

        // Colores pasteles
        drawCenteredText(buf, "Color de Texto", 28, 94, 264, 16, 0.82f, 0.38f, 0.38f, COLOR_NEGRO, true);
        drawCenteredText(buf, "Color de Fondo", 28, 144, 264, 16, 0.82f, 0.38f, 0.38f, COLOR_NEGRO, true);

        static const u32 palette[6] = {
            COLOR_NEGRO, COLOR_MUSGO,
            C2D_Color32(140, 175, 220, 255),
            C2D_Color32(225, 150, 170, 255),
            C2D_Color32(235, 215, 140, 255),
            COLOR_CREMA
        };
        for (int i = 0; i < 6; ++i) {
            float sx = 32.0f + i * 44.0f;
            drawBorderedBox(sx, 112, 36, 24, 0.80f, palette[i], COLOR_NEGRO, (activeTextColor == palette[i]) ? 2.2f : 1.0f);
            drawBorderedBox(sx, 162, 36, 24, 0.80f, palette[i], COLOR_NEGRO, (activeBgColor == palette[i]) ? 2.2f : 1.0f);
        }
    }

    // ------------------------------------------------------------------------
    // EXPLORADOR DE ARCHIVOS .RTF (ABRIR)
    // ------------------------------------------------------------------------
    void refreshFileList() {
        sdFiles.clear();
        DIR* dir = opendir("sdmc:/Textos3DS");
        if (!dir) return;
        struct dirent* ent;
        while ((ent = readdir(dir)) != NULL) {
            std::string name = ent->d_name;
            if (name == "." || name == "..") continue;
            sdFiles.push_back(name);
        }
        closedir(dir);
        std::sort(sdFiles.begin(), sdFiles.end());
    }

    void handleOpenFileInput(u32 kDown, touchPosition touch) {
        if (kDown & KEY_B) {
            modal = ModalState::StartMenu;
            return;
        }
        if (!sdFiles.empty()) {
            if (kDown & KEY_UP) fileListSelection = (fileListSelection + (int)sdFiles.size() - 1) % (int)sdFiles.size();
            if (kDown & KEY_DOWN) fileListSelection = (fileListSelection + 1) % (int)sdFiles.size();
            if (kDown & KEY_A) {
                loadRTF("sdmc:/Textos3DS/" + sdFiles[fileListSelection], sdFiles[fileListSelection]);
                modal = ModalState::None;
                return;
            }
        }
        if (kDown & KEY_TOUCH) {
            if (touch.px >= 262 && touch.px <= 286 && touch.py >= 16 && touch.py <= 38) {
                modal = ModalState::StartMenu;
                return;
            }
            for (size_t i = 0; i < sdFiles.size() && i < 5; ++i) {
                float fy = 46.0f + i * 30.0f;
                if (touch.px >= 46 && touch.px <= 274 && touch.py >= fy && touch.py <= fy + 25.0f) {
                    if (fileListSelection == (int)i) {
                        loadRTF("sdmc:/Textos3DS/" + sdFiles[i], sdFiles[i]);
                        modal = ModalState::None;
                    } else {
                        fileListSelection = (int)i;
                    }
                    break;
                }
            }
        }
    }

    void renderOpenFileModal(C2D_TextBuf buf) {
        C2D_DrawRectSolid(0, 0, 0.75f, 320, 240, C2D_Color32(0, 0, 0, 110));
        drawBorderedBox(30, 14, 260, 206, 0.78f, COLOR_CREMA, COLOR_MUSGO, 2.0f);

        C2D_DrawRectSolid(32, 16, 0.80f, 256, 22, COLOR_MUSGO);
        drawCenteredText(buf, "ABRIR ARCHIVO (/Textos3DS)", 32, 16, 232, 22, 0.82f, 0.38f, 0.38f, COLOR_BLANCO, true);

        C2D_DrawRectSolid(264, 16, 0.81f, 24, 22, COLOR_ROJO_ERR);
        drawCenteredText(buf, "X", 264, 16, 24, 22, 0.83f, 0.42f, 0.42f, COLOR_BLANCO, true);

        if (sdFiles.empty()) {
            drawCenteredText(buf, "No hay archivos guardados aún.", 32, 95, 256, 24, 0.82f, 0.40f, 0.40f, COLOR_GRIS_TEXTO, false);
        } else {
            for (size_t i = 0; i < sdFiles.size() && i < 5; ++i) {
                float fy = 46.0f + i * 30.0f;
                bool sel = (fileListSelection == (int)i);
                drawBorderedBox(46, fy, 228, 25, 0.80f,
                                sel ? COLOR_MUSGO : COLOR_GRIS_TECLA,
                                sel ? COLOR_NEGRO : COLOR_BORDE_TECLA,
                                sel ? 1.5f : 1.0f);
                drawCenteredText(buf, sdFiles[i], 46, fy, 228, 25, 0.82f, 0.40f, 0.40f,
                                 sel ? COLOR_BLANCO : COLOR_NEGRO, sel);
            }
        }
    }

    // ------------------------------------------------------------------------
    // VENTANAS DE CONFIRMACIÓN (SÍ / NO SIN CRUZ ROJA REDUNDANTE, NO POR DEFECTO)
    // ------------------------------------------------------------------------
    void handleConfirmModalInput(u32 kDown, touchPosition touch, bool& running) {
        if (kDown & KEY_B) {
            modal = ModalState::StartMenu;
            return;
        }
        if (kDown & KEY_LEFT) confirmSelection = 0;
        if (kDown & KEY_RIGHT) confirmSelection = 1;

        auto executeConfirm = [&](int sel) {
            if (sel == 1) { // NO
                modal = ModalState::StartMenu;
            } else { // SÍ
                if (modal == ModalState::ConfirmExit) {
                    running = false;
                } else if (modal == ModalState::ConfirmNew) {
                    doc.clear();
                    cursorPos = 0;
                    selectionAnchor = -1;
                    modal = ModalState::None;
                }
            }
        };

        if (kDown & KEY_A) {
            executeConfirm(confirmSelection);
            return;
        }

        if (kDown & KEY_TOUCH) {
            if (touch.py >= 140 && touch.py <= 172) {
                if (touch.px >= 56 && touch.px <= 146) {
                    if (confirmSelection == 0) executeConfirm(0);
                    else confirmSelection = 0;
                } else if (touch.px >= 174 && touch.px <= 264) {
                    if (confirmSelection == 1) executeConfirm(1);
                    else confirmSelection = 1;
                }
            }
        }
    }

    void renderConfirmModal(C2D_TextBuf buf) {
        C2D_DrawRectSolid(0, 0, 0.75f, 320, 240, C2D_Color32(0, 0, 0, 120));
        drawBorderedBox(36, 48, 248, 144, 0.78f, COLOR_CREMA, COLOR_MUSGO, 2.0f);

        C2D_DrawRectSolid(38, 50, 0.80f, 244, 22, COLOR_MUSGO);
        drawCenteredText(buf, "ATENCIÓN", 38, 50, 244, 22, 0.82f, 0.42f, 0.42f, COLOR_BLANCO, true);

        const char* msg = (modal == ModalState::ConfirmExit)
            ? "¿Deseas salir del programa?"
            : "¿Crear documento nuevo y descartar cambios?";
        drawCenteredText(buf, msg, 44, 86, 232, 36, 0.82f, 0.40f, 0.40f, COLOR_NEGRO, false);

        float pulse = 0.65f + 0.35f * sinf((float)osGetTime() * 0.008f);
        u32 activeMoss = C2D_Color32((u8)(78 * pulse), (u8)(122 * pulse), (u8)(101 * pulse), 255);

        bool selYes = (confirmSelection == 0);
        bool selNo = (confirmSelection == 1);

        drawBorderedBox(56, 140, 90, 32, 0.80f,
                        selYes ? activeMoss : COLOR_GRIS_BTN,
                        selYes ? COLOR_NEGRO : COLOR_BORDE_TECLA,
                        selYes ? 1.5f : 1.0f);
        drawCenteredText(buf, "SÍ", 56, 140, 90, 32, 0.82f, 0.44f, 0.44f,
                         selYes ? COLOR_BLANCO : COLOR_NEGRO, selYes);

        drawBorderedBox(174, 140, 90, 32, 0.80f,
                        selNo ? activeMoss : COLOR_GRIS_BTN,
                        selNo ? COLOR_NEGRO : COLOR_BORDE_TECLA,
                        selNo ? 1.5f : 1.0f);
        drawCenteredText(buf, "NO", 174, 140, 90, 32, 0.82f, 0.44f, 0.44f,
                         selNo ? COLOR_BLANCO : COLOR_NEGRO, selNo);
    }

    // ------------------------------------------------------------------------
    // EXPORTACIÓN E IMPORTACIÓN RTF CON SOPORTE DE CARACTERES EN ESPAÑOL
    // ------------------------------------------------------------------------
    void saveRTF(const std::string& path) {
        FILE* f = fopen(path.c_str(), "w");
        if (!f) return;

        fprintf(f, "{\\rtf1\\ansi\\ansicpg1252\\deff0{\\fonttbl{\\f0 Courier New;}}\n\\f0\\fs24 ");
        for (const auto& sc : doc) {
            if (sc.utf8 == "\n") {
                fprintf(f, "\\par\n");
            } else if (sc.utf8 == "\f") {
                fprintf(f, "\\page\n");
            } else if (sc.utf8 == "\t") {
                fprintf(f, "\\tab ");
            } else if (sc.utf8 == "\\" || sc.utf8 == "{" || sc.utf8 == "}") {
                fprintf(f, "\\%s", sc.utf8.c_str());
            } else {
                if (sc.bold) fprintf(f, "\\b ");
                if (sc.underline) fprintf(f, "\\ul ");
                if (sc.strike) fprintf(f, "\\strike ");

                const unsigned char* bytes = (const unsigned char*)sc.utf8.c_str();
                if (bytes[0] < 0x80) {
                    fputc(bytes[0], f);
                } else {
                    // Decodificar UTF-8 a punto de código Unicode para escape RTF \uN?
                    uint32_t cp = 0;
                    if ((bytes[0] & 0xE0) == 0xC0 && bytes[1]) {
                        cp = ((bytes[0] & 0x1F) << 6) | (bytes[1] & 0x3F);
                    } else if ((bytes[0] & 0xF0) == 0xE0 && bytes[1] && bytes[2]) {
                        cp = ((bytes[0] & 0x0F) << 12) | ((bytes[1] & 0x3F) << 6) | (bytes[2] & 0x3F);
                    }
                    fprintf(f, "\\u%u?", cp);
                }

                if (sc.strike) fprintf(f, "\\strike0 ");
                if (sc.underline) fprintf(f, "\\ulnone ");
                if (sc.bold) fprintf(f, "\\b0 ");
            }
        }
        fprintf(f, "\n}\n");
        fclose(f);
    }

    void loadRTF(const std::string& path, const std::string& fname) {
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) return;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz <= 0) {
            fclose(f);
            return;
        }
        std::string raw(sz, '\0');
        fread(&raw[0], 1, sz, f);
        fclose(f);

        doc.clear();
        cursorPos = 0;
        selectionAnchor = -1;
        currentFilename = fname;

        bool b = false, u = false, s = false;
        size_t i = 0;
        while (i < raw.size()) {
            char c = raw[i];
            if (c == '{' || c == '}' || c == '\r' || c == '\n') {
                i++;
                continue;
            }
            if (c == '\\') {
                i++;
                if (i >= raw.size()) break;
                if (raw[i] == '\\' || raw[i] == '{' || raw[i] == '}') {
                    StyledChar sc{std::string(1, raw[i]), b, u, s, COLOR_NEGRO, pageBgColor, activeFontSize};
                    doc.push_back(sc);
                    i++;
                    continue;
                }
                if (raw[i] == 'u' && i + 1 < raw.size() && isdigit((unsigned char)raw[i + 1])) {
                    i++;
                    uint32_t cp = 0;
                    while (i < raw.size() && isdigit((unsigned char)raw[i])) {
                        cp = cp * 10 + (raw[i] - '0');
                        i++;
                    }
                    if (i < raw.size() && raw[i] == '?') i++;
                    std::string utf8Out;
                    if (cp < 0x80) {
                        utf8Out += (char)cp;
                    } else if (cp < 0x800) {
                        utf8Out += (char)(0xC0 | (cp >> 6));
                        utf8Out += (char)(0x80 | (cp & 0x3F));
                    } else {
                        utf8Out += (char)(0xE0 | (cp >> 12));
                        utf8Out += (char)(0x80 | ((cp >> 6) & 0x3F));
                        utf8Out += (char)(0x80 | (cp & 0x3F));
                    }
                    doc.push_back({utf8Out, b, u, s, COLOR_NEGRO, pageBgColor, activeFontSize});
                    continue;
                }
                std::string ctrl;
                while (i < raw.size() && isalpha((unsigned char)raw[i])) {
                    ctrl += raw[i++];
                }
                std::string param;
                while (i < raw.size() && (isdigit((unsigned char)raw[i]) || raw[i] == '-')) {
                    param += raw[i++];
                }
                if (i < raw.size() && raw[i] == ' ') i++;

                if (ctrl == "par") doc.push_back({"\n", b, u, s, COLOR_NEGRO, pageBgColor, activeFontSize});
                else if (ctrl == "page") doc.push_back({"\f", b, u, s, COLOR_NEGRO, pageBgColor, activeFontSize});
                else if (ctrl == "tab") doc.push_back({"\t", b, u, s, COLOR_NEGRO, pageBgColor, activeFontSize});
                else if (ctrl == "b") b = (param != "0");
                else if (ctrl == "ul") u = true;
                else if (ctrl == "ulnone") u = false;
                else if (ctrl == "strike") s = (param != "0");
            } else {
                // Saltar cabecera fonttbl si aparece como texto plano
                if (raw.compare(i, 12, "Courier New;") == 0) {
                    i += 12;
                    continue;
                }
                doc.push_back({std::string(1, c), b, u, s, COLOR_NEGRO, pageBgColor, activeFontSize});
                i++;
            }
        }
        cursorPos = doc.size();
    }
};

// ============================================================================
// PUNTO DE ENTRADA INVOCADO DESDE source/main.cpp
// ============================================================================
void runEditorLoop() {
    LauraEditor editor;
    editor.init();
    editor.run();
}
