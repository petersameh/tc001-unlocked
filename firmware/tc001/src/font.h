#pragma once
#include <Arduino.h>
// Custom 3x5 font for ASCII 32..95 (lowercase is drawn as uppercase).
// Each glyph = 5 rows top->bottom; row bits: 4 = left, 2 = middle, 1 = right column.
// '`' is drawn as a degree sign.
static const uint8_t FONT3x5[64][5] = {
  {0,0,0,0,0}, {2,2,2,0,2}, {5,5,0,0,0}, {5,7,5,7,5}, // space ! " #
  {3,6,2,3,6}, {5,1,2,4,5}, {2,5,2,5,3}, {2,2,0,0,0}, // $ % & '
  {1,2,2,2,1}, {4,2,2,2,4}, {0,5,2,5,0}, {0,2,7,2,0}, // ( ) * +
  {0,0,0,2,2}, {0,0,7,0,0}, {0,0,0,0,2}, {1,1,2,4,4}, // , - . /
  {7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7}, // 0 1 2 3
  {5,5,7,1,1}, {7,4,7,1,7}, {7,4,7,5,7}, {7,1,1,1,1}, // 4 5 6 7
  {7,5,7,5,7}, {7,5,7,1,7}, {0,2,0,2,0}, {0,2,0,2,4}, // 8 9 : ;
  {1,2,4,2,1}, {0,7,0,7,0}, {4,2,1,2,4}, {7,1,3,0,2}, // < = > ?
  {2,5,7,4,3}, {2,5,7,5,5}, {6,5,6,5,6}, {3,4,4,4,3}, // @ A B C
  {6,5,5,5,6}, {7,4,6,4,7}, {7,4,6,4,4}, {3,4,5,5,3}, // D E F G
  {5,5,7,5,5}, {7,2,2,2,7}, {1,1,1,5,2}, {5,5,6,5,5}, // H I J K
  {4,4,4,4,7}, {5,7,7,5,5}, {6,5,5,5,5}, {2,5,5,5,2}, // L M N O
  {6,5,6,4,4}, {2,5,5,6,3}, {6,5,6,5,5}, {3,4,2,1,6}, // P Q R S
  {7,2,2,2,2}, {5,5,5,5,7}, {5,5,5,5,2}, {5,5,7,7,5}, // T U V W
  {5,5,2,5,5}, {5,5,2,2,2}, {7,1,2,4,7}, {6,4,4,4,6}, // X Y Z [
  {4,4,2,1,1}, {3,1,1,1,3}, {2,5,0,0,0}, {0,0,0,0,7}, // \ ] ^ _
};
static const uint8_t GLYPH_DEGREE[5] = {2,5,2,0,0};
static const uint8_t GLYPH_UP[5] = {0,2,7,0,0};    // '{' = up triangle
static const uint8_t GLYPH_DOWN[5] = {0,7,2,0,0};  // '}' = down triangle

inline char normChar(char c) {
  if (c >= 'a' && c <= 'z') c -= 32;
  return c;
}
inline const uint8_t *glyphFor(char c) {
  c = normChar(c);
  if (c == '`') return GLYPH_DEGREE;
  if (c == '{') return GLYPH_UP;
  if (c == '}') return GLYPH_DOWN;
  if (c < 32 || c > 95) c = '?';
  return FONT3x5[c - 32];
}
// Narrow glyphs use only the middle column.
inline bool glyphNarrow(char c) { return c == ':' || c == '.' || c == '!' || c == '\'' || c == ','; }
inline int glyphWidth(char c) {
  if (c == ' ') return 2;
  return glyphNarrow(c) ? 1 : 3;
}
