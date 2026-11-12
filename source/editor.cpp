#include "editor.hpp"
#include <citro2d.h>
#include <ctru.h>

Editor::Editor() {
    content = "Laura Word Processor - Listo para escribir.";
    cursorPosition = content.length();
}

void Editor::handleInput(uint32_t kDown) {
    if (kDown & KEY_A) {
        content += "a";
    } else if (kDown & KEY_B) {
        content += " ";
    }
}

void Editor::render() {
    // Lógica de renderizado gráfico
}

std::string Editor::getText() const {
    return content;
}
