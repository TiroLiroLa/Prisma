// Prisma — interfaces de plataforma: desenho, teclas, persistência.
// O mesmo código de interface roda no Cardputer (M5Canvas) e no simulador.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace px {

typedef uint16_t color_t;  // RGB565

inline color_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (color_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
// mistura a com b (t = 0..256 = quanto de b)
inline color_t mix(color_t a, color_t b, int t) {
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
  int br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  return (color_t)((((ar * (256 - t) + br * t) >> 8) << 11) | (((ag * (256 - t) + bg * t) >> 8) << 5) |
                   ((ab * (256 - t) + bb * t) >> 8));
}

enum { SCREEN_W = 240, SCREEN_H = 135, FONT_W = 6, FONT_H = 8 };

class Gfx {
 public:
  virtual ~Gfx() {}
  virtual void fillScreen(color_t c) = 0;
  virtual void fillRect(int x, int y, int w, int h, color_t c) = 0;
  virtual void drawRect(int x, int y, int w, int h, color_t c) = 0;
  virtual void hline(int x, int y, int w, color_t c) = 0;
  virtual void vline(int x, int y, int h, color_t c) = 0;
  virtual void line(int x0, int y0, int x1, int y1, color_t c) = 0;
  virtual void pixel(int x, int y, color_t c) = 0;
  virtual void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, color_t c) = 0;
  virtual void fillRoundRect(int x, int y, int w, int h, int r, color_t c) = 0;
  virtual void drawRoundRect(int x, int y, int w, int h, int r, color_t c) = 0;
  virtual void fillCircle(int x, int y, int r, color_t c) = 0;
  virtual void drawCircle(int x, int y, int r, color_t c) = 0;
  // Texto monoespaçado 6x8 (Font0), canto superior esquerdo, fundo transparente.
  virtual void text(int x, int y, const char* s, color_t c, int size = 1) = 0;
  virtual void setClip(int x, int y, int w, int h) = 0;
  virtual void clearClip() = 0;
};

enum KeyCode : uint8_t { K_CHAR, K_ENTER, K_DEL, K_TAB, K_BACK };

struct Key {
  KeyCode code;
  char ch;  // já com shift aplicado
  bool fn, ctrl, alt, opt;
};

enum class Nav : uint8_t { None, Up, Down, Left, Right };

// Setas do Cardputer: ; . , /  (em campos de texto, só com fn)
inline Nav arrowOf(const Key& k, bool needFn) {
  if (k.code != K_CHAR) return Nav::None;
  if (needFn && !k.fn) return Nav::None;
  switch (k.ch) {
    case ';': return Nav::Up;
    case '.': return Nav::Down;
    case ',': return Nav::Left;
    case '/': return Nav::Right;
  }
  return Nav::None;
}

class Platform {
 public:
  virtual ~Platform() {}
  virtual uint32_t millis() = 0;
  virtual int battery() { return -1; }  // 0..100 ou -1
  virtual bool load(void* buf, size_t len) { (void)buf; (void)len; return false; }
  virtual void save(const void* buf, size_t len) { (void)buf; (void)len; }
  virtual void setBrightness(uint8_t level) { (void)level; }
  virtual bool hasBrightness() { return true; }  // false: o brilho é do Órbita
  // embedded(): o Prisma roda como app nativo dentro do Órbita; sair = voltar ao menu.
  virtual bool embedded() { return false; }
  virtual bool hasOrbita() { return false; }
  virtual void exitToOrbita() {}
};

}  // namespace px
