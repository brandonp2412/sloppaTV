#pragma once

#include <android/input.h>

#include "account_screen.hpp"
#include "search_screen.hpp"

#include <cstdint>
#include <string>
#include <vector>

enum class KeyAction {
    Insert,
    Backspace,
    Done,
};

struct VirtualKey {
    std::string label;
    std::string value;
    KeyAction action = KeyAction::Insert;
};

inline const std::vector<std::vector<VirtualKey>>& keyboardRows() {
    static const std::vector<std::vector<VirtualKey>> rows = {
        {{"A", "A"},
         {"B", "B"},
         {"C", "C"},
         {"D", "D"},
         {"E", "E"},
         {"F", "F"},
         {"G", "G"},
         {"H", "H"},
         {"I", "I"},
         {"J", "J"}},
        {{"K", "K"},
         {"L", "L"},
         {"M", "M"},
         {"N", "N"},
         {"O", "O"},
         {"P", "P"},
         {"Q", "Q"},
         {"R", "R"},
         {"S", "S"},
         {"T", "T"}},
        {{"U", "U"},
         {"V", "V"},
         {"W", "W"},
         {"X", "X"},
         {"Y", "Y"},
         {"Z", "Z"},
         {"0", "0"},
         {"1", "1"},
         {"2", "2"},
         {"3", "3"}},
        {{"4", "4"},
         {"5", "5"},
         {"6", "6"},
         {"7", "7"},
         {"8", "8"},
         {"9", "9"},
         {".", "."},
         {"-", "-"},
         {"_", "_"},
         {"/", "/"}},
        {{":", ":"}, {"@", "@"}, {"SPACE", " "}, {"BACK", "", KeyAction::Backspace}, {"DONE", "", KeyAction::Done}},
    };
    return rows;
}

inline char keyCodeToChar(int32_t keyCode, int32_t metaState) {
    const bool shift = (metaState & AMETA_SHIFT_ON) != 0;
    if (keyCode >= AKEYCODE_A && keyCode <= AKEYCODE_Z) {
        const char base = static_cast<char>('a' + (keyCode - AKEYCODE_A));
        return shift ? static_cast<char>(base - 'a' + 'A') : base;
    }
    if (keyCode >= AKEYCODE_0 && keyCode <= AKEYCODE_9) {
        static constexpr char shifted[] = ")!@#$%^&*(";
        const int index = keyCode - AKEYCODE_0;
        return shift ? shifted[index] : static_cast<char>('0' + index);
    }
    switch (keyCode) {
    case AKEYCODE_SPACE:
        return ' ';
    case AKEYCODE_PERIOD:
        return shift ? '>' : '.';
    case AKEYCODE_COMMA:
        return shift ? '<' : ',';
    case AKEYCODE_SLASH:
        return shift ? '?' : '/';
    case AKEYCODE_BACKSLASH:
        return shift ? '|' : '\\';
    case AKEYCODE_MINUS:
        return shift ? '_' : '-';
    case AKEYCODE_EQUALS:
        return shift ? '+' : '=';
    case AKEYCODE_SEMICOLON:
        return shift ? ':' : ';';
    case AKEYCODE_APOSTROPHE:
        return shift ? '"' : '\'';
    case AKEYCODE_AT:
        return '@';
    default:
        return 0;
    }
}

struct VirtualKeyboardEffects {
    bool searchChanged = false;
    bool submitSearch = false;
};

class VirtualKeyboardState {
public:
    void reset() {
        row_ = 0;
        column_ = 0;
    }

    void move(int dx, int dy) {
        const auto& rows = keyboardRows();
        if (dy != 0) {
            row_ = std::clamp(row_ + dy, 0, static_cast<int>(rows.size()) - 1);
            column_ = std::clamp(column_, 0, static_cast<int>(rows[static_cast<size_t>(row_)].size()) - 1);
        }
        if (dx != 0) {
            const int columns = static_cast<int>(rows[static_cast<size_t>(row_)].size());
            column_ = (column_ + dx) % columns;
            if (column_ < 0) column_ += columns;
        }
    }

    [[nodiscard]] VirtualKeyboardEffects activate(bool forSearch, SearchScreenState& search,
                                                  AccountScreenState& account) {
        const auto& key = keyboardRows()[static_cast<size_t>(row_)][static_cast<size_t>(column_)];
        VirtualKeyboardEffects effects;
        if (forSearch) {
            switch (key.action) {
            case KeyAction::Insert:
                for (char value : key.value) search.append(value);
                effects.searchChanged = true;
                break;
            case KeyAction::Backspace:
                effects.searchChanged = search.backspace();
                break;
            case KeyAction::Done:
                search.setKeyboard(false);
                effects.submitSearch = true;
                break;
            }
            return effects;
        }

        if (account.loginFocus() < 0 || account.loginFocus() >= 3) return effects;
        switch (key.action) {
        case KeyAction::Insert:
            for (char value : key.value) account.appendToFocusedField(value);
            break;
        case KeyAction::Backspace:
            account.backspaceFocusedField();
            break;
        case KeyAction::Done:
            account.setKeyboardActive(false);
            break;
        }
        return effects;
    }

    [[nodiscard]] int row() const { return row_; }

    [[nodiscard]] int column() const { return column_; }

private:
    int row_ = 0;
    int column_ = 0;
};
