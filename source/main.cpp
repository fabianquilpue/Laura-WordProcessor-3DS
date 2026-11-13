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
    if (t) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
        ultimaHoraGuardado = buf;
    }
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

struct EditorState {
    std::string text;
    size_t cursor;
};

int main(int argc, char* argv[]) {
    gfxInitDefault();
    ptmuInit();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget* topScreen = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottomScreen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    C2D_TextBuf staticBuf = C2D_TextBufNew(4096);
    C2D_TextBuf dynamicBuf = C2D_TextBufNew(32768);

    std::vector<KeyButton> keys;
    float btnW = 29.5f; 
    float btnH = 22.0f; 
    float gap = 2.0f;

    // Fila superior
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

    std::string documento = "";
    size_t cursorPos = 0;
    
    // Selección mejorada
    bool hasSelection = false;
    bool isSelectModeToggle = false;
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
            isSelectModeToggle = false;
            return false;
        }
        size_t start = std::min(selAnchor, cursorPos);
        size_t end = std::max(selAnchor, cursorPos);
        documento.erase(start, end - start);
        cursorPos = start;
        hasSelection = false;
        isSelectModeToggle = false;
        return true;
    };

    int estadoTeclaMuerta = 0;
    bool capsLock = false;
    u64 lastLPressTick = 0;

    bool isBold = false;
    bool isItalic = false;
    bool isUnderline = false;
    bool isStrikethrough = false;

    bool showTopInfoBar = true;

    const int INITIAL_DELAY = 15;
    const int REPEAT_RATE = 4;

    int bsCooldown = 0;
    int touchCooldown = 0;
    int activeTouchKeyIdx = -1;
    int dpadCooldown = 0;
    int undoCooldown = 0;

    // Navegación con salto (jump) incorporado
    auto moveCursorLeft = [&](bool selecting, bool jump) {
        if (selecting && !hasSelection) { hasSelection = true; selAnchor = cursorPos; }
        if (!selecting) hasSelection = false;
        
        if (jump) {
            // Saltar espacios
            while (cursorPos > 0 && (documento[cursorPos - 1] == ' ' || documento[cursorPos - 1] == '\n')) {
                cursorPos
