#pragma once
#include <Arduino.h>
// Proportional 4-row font for the compact dashboard (digits + a few symbols).
// rows: bit (w-1) = leftmost column.
struct Glyph4 { char c; uint8_t w; uint8_t rows[4]; };
static const Glyph4 FONT4[] = {
  {'0', 3, {7, 5, 5, 7}}, {'1', 1, {1, 1, 1, 1}}, {'2', 3, {7, 3, 4, 7}}, {'3', 3, {7, 3, 1, 7}},
  {'4', 3, {5, 5, 7, 1}}, {'5', 3, {7, 6, 1, 7}}, {'6', 3, {4, 7, 5, 7}}, {'7', 3, {7, 1, 2, 2}},
  {'8', 3, {7, 7, 5, 7}}, {'9', 3, {7, 5, 7, 1}}, {'.', 1, {0, 0, 0, 1}}, {':', 1, {0, 1, 0, 1}},
  {'`', 2, {3, 3, 0, 0}},  // degree
  {'K', 3, {5, 6, 6, 5}}, {'M', 3, {5, 7, 7, 5}}, {'B', 3, {6, 7, 5, 6}},
  {'-', 3, {0, 7, 0, 0}}, {'%', 3, {5, 1, 4, 5}}, {'F', 3, {7, 4, 6, 4}}, {'C', 3, {7, 4, 4, 7}},
  {'A', 3, {2, 5, 7, 5}}, {'D', 3, {6, 5, 5, 6}}, {'E', 3, {7, 6, 4, 7}}, {'G', 3, {7, 4, 5, 7}},
  {'H', 3, {5, 7, 5, 5}}, {'L', 3, {4, 4, 4, 7}}, {'N', 3, {7, 5, 5, 5}}, {'O', 3, {7, 5, 5, 7}},
  {'R', 3, {6, 5, 6, 5}}, {'S', 3, {3, 4, 1, 6}}, {'T', 3, {7, 2, 2, 2}}, {'U', 3, {5, 5, 5, 7}},
  {'W', 3, {5, 5, 7, 7}}, {'P', 3, {7, 5, 7, 4}}, {'I', 1, {1, 1, 1, 1}}, {'Y', 3, {5, 5, 2, 2}},
  {' ', 1, {0, 0, 0, 0}},
};
inline const Glyph4 *glyph4(char c) {
  for (auto &g : FONT4) if (g.c == c) return &g;
  return nullptr;
}
inline int text4Width(const String &s) {
  int w = 0, n = 0;
  for (size_t i = 0; i < s.length(); i++) { const Glyph4 *g = glyph4(s[i]); if (g) { w += g->w; n++; } }
  return n ? w + n - 1 : 0;
}
