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
        if (sc.style.bold != currentStyle.bold) { fprintf(archivo, sc.style.bold ? "\\b " : "\\b0 "); currentStyle.bold = sc.style.bold; }
        if (sc.style.italic != currentStyle.italic) { fprintf(archivo, sc.style.italic ? "\\i " : "\\i0 "); currentStyle.italic = sc.style.italic; }
        if (sc.style.underline != currentStyle.underline) { fprintf(archivo, sc.style.underline ? "\\ul " : "\\ulnone "); currentStyle.underline = sc.style.underline; }
        if (sc.style.strike != currentStyle.strike) { fprintf(archivo, sc.style.strike ? "\\strike " : "\\strike0 "); currentStyle.strike = sc.style.strike; }

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

    if (currentStyle.bold) fprintf(archivo,
