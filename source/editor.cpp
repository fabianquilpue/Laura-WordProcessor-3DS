#pragma once

#include <3ds.h>
#include <citro2d.h>
#include <string>
#include <vector>
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

void runEditor();
