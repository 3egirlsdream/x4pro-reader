#pragma once
#include <ArduinoJson.h>

#include "Game2048.h"
#include "Gomoku.h"
#include "Minesweeper.h"
namespace games::save {
bool decode(JsonVariantConst doc, uint8_t* out, size_t count);
bool valid2048(JsonVariantConst doc);
bool validGomoku(JsonVariantConst doc);
bool validMinesweeper(JsonVariantConst doc);
bool write(const char* id, const uint8_t* bytes, size_t count, char* text);
}  // namespace games::save
