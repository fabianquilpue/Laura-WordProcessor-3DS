#ifndef EDITOR_HPP
#define EDITOR_HPP

#include <string>
#include <vector>
#include <cstdint>

class Editor {
public:
    Editor();
    void handleInput(uint32_t kDown);
    void render();
    std::string getText() const;

private:
    std::string content;
    size_t cursorPosition;
};

#endif
