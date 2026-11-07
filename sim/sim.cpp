// Simulador de desktop do Prisma: roda o mesmo núcleo e grava a tela em PPM.
//   ./prisma_sim "<roteiro>" pasta_saida
// Roteiro: caracteres digitados + {enter} {del} {tab} {esc} {fn+X} {ctrl+X}
//          {fn+del} {wait:ms} {shot:nome}.  A crase ` também é "voltar".
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>

#include "core/app.h"
#include "simfont.h"

using namespace px;

class FbGfx : public Gfx {
 public:
  uint16_t fb[SCREEN_W * SCREEN_H];
  int cx0 = 0, cy0 = 0, cx1 = SCREEN_W, cy1 = SCREEN_H;

  void pixel(int x, int y, color_t c) override {
    if (x < cx0 || y < cy0 || x >= cx1 || y >= cy1) return;
    fb[y * SCREEN_W + x] = c;
  }
  void fillScreen(color_t c) override {
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) fb[i] = c;
  }
  void fillRect(int x, int y, int w, int h, color_t c) override {
    for (int j = y; j < y + h; j++)
      for (int i = x; i < x + w; i++) pixel(i, j, c);
  }
  void drawRect(int x, int y, int w, int h, color_t c) override {
    hline(x, y, w, c);
    hline(x, y + h - 1, w, c);
    vline(x, y, h, c);
    vline(x + w - 1, y, h, c);
  }
  void hline(int x, int y, int w, color_t c) override { fillRect(x, y, w, 1, c); }
  void vline(int x, int y, int h, color_t c) override { fillRect(x, y, 1, h, c); }
  void line(int x0, int y0, int x1, int y1, color_t c) override {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (int guard = 0; guard < 20000; guard++) {
      pixel(x0, y0, c);
      if (x0 == x1 && y0 == y1) break;
      int e2 = 2 * e;
      if (e2 >= dy) {
        e += dy;
        x0 += sx;
      }
      if (e2 <= dx) {
        e += dx;
        y0 += sy;
      }
    }
  }
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, color_t c) override {
    if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }
    if (y1 > y2) { std::swap(y1, y2); std::swap(x1, x2); }
    if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }
    if (y0 == y2) {
      int a = std::min(x0, std::min(x1, x2)), b = std::max(x0, std::max(x1, x2));
      hline(a, y0, b - a + 1, c);
      return;
    }
    for (int y = y0; y <= y2; y++) {
      if (y < cy0 || y >= cy1) continue;
      double xa = x0 + (double)(x2 - x0) * (y - y0) / (y2 - y0);
      double xb;
      if (y < y1 || y1 == y2) xb = (y1 == y0) ? x1 : x0 + (double)(x1 - x0) * (y - y0) / (y1 - y0);
      else xb = x1 + (double)(x2 - x1) * (y - y1) / (y2 - y1);
      if (xa > xb) std::swap(xa, xb);
      int a = (int)lround(xa), b = (int)lround(xb);
      hline(a, y, b - a + 1, c);
    }
  }
  void circleQ(int x0, int y0, int r, int q, color_t c) {
    int f = 1 - r, ddx = 1, ddy = -2 * r, x = 0, y = r;
    while (x < y) {
      if (f >= 0) { y--; ddy += 2; f += ddy; }
      x++; ddx += 2; f += ddx;
      if (q & 4) { pixel(x0 + x, y0 + y, c); pixel(x0 + y, y0 + x, c); }
      if (q & 2) { pixel(x0 + x, y0 - y, c); pixel(x0 + y, y0 - x, c); }
      if (q & 8) { pixel(x0 - y, y0 + x, c); pixel(x0 - x, y0 + y, c); }
      if (q & 1) { pixel(x0 - y, y0 - x, c); pixel(x0 - x, y0 - y, c); }
    }
  }
  void fillCircleH(int x0, int y0, int r, int corners, int delta, color_t c) {
    int f = 1 - r, ddx = 1, ddy = -2 * r, x = 0, y = r, px = x, py = y;
    delta++;
    while (x < y) {
      if (f >= 0) { y--; ddy += 2; f += ddy; }
      x++; ddx += 2; f += ddx;
      if (x < y + 1) {
        if (corners & 1) vline(x0 + x, y0 - y, 2 * y + delta, c);
        if (corners & 2) vline(x0 - x, y0 - y, 2 * y + delta, c);
      }
      if (y != py) {
        if (corners & 1) vline(x0 + py, y0 - px, 2 * px + delta, c);
        if (corners & 2) vline(x0 - py, y0 - px, 2 * px + delta, c);
        py = y;
      }
      px = x;
    }
  }
  void fillRoundRect(int x, int y, int w, int h, int r, color_t c) override {
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    fillRect(x + r, y, w - 2 * r, h, c);
    fillCircleH(x + w - r - 1, y + r, r, 1, h - 2 * r - 1, c);
    fillCircleH(x + r, y + r, r, 2, h - 2 * r - 1, c);
  }
  void drawRoundRect(int x, int y, int w, int h, int r, color_t c) override {
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    hline(x + r, y, w - 2 * r, c);
    hline(x + r, y + h - 1, w - 2 * r, c);
    vline(x, y + r, h - 2 * r, c);
    vline(x + w - 1, y + r, h - 2 * r, c);
    circleQ(x + r, y + r, r, 1, c);
    circleQ(x + w - r - 1, y + r, r, 2, c);
    circleQ(x + w - r - 1, y + h - r - 1, r, 4, c);
    circleQ(x + r, y + h - r - 1, r, 8, c);
  }
  void fillCircle(int x, int y, int r, color_t c) override {
    vline(x, y - r, 2 * r + 1, c);
    fillCircleH(x, y, r, 3, 0, c);
  }
  void drawCircle(int x, int y, int r, color_t c) override {
    pixel(x, y + r, c); pixel(x, y - r, c); pixel(x + r, y, c); pixel(x - r, y, c);
    circleQ(x, y, r, 15, c);
  }
  void text(int x, int y, const char* s, color_t c, int size) override {
    for (; *s; s++, x += FONT_W * size) {
      unsigned ch = (unsigned char)*s;
      if (ch < 32 || ch > 126) ch = '?';
      const uint8_t* gl = kSimFont[ch - 32];
      for (int r = 1; r < 9; r++)
        for (int b = 0; b < 6; b++)
          if (gl[r] & (0x80 >> b)) fillRect(x + b * size, y + (r - 1) * size, size, size, c);
    }
  }
  void setClip(int x, int y, int w, int h) override {
    cx0 = std::max(0, x);
    cy0 = std::max(0, y);
    cx1 = std::min((int)SCREEN_W, x + w);
    cy1 = std::min((int)SCREEN_H, y + h);
  }
  void clearClip() override {
    cx0 = cy0 = 0;
    cx1 = SCREEN_W;
    cy1 = SCREEN_H;
  }

  void savePPM(const char* path, int scale) {
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", SCREEN_W * scale, SCREEN_H * scale);
    for (int y = 0; y < SCREEN_H * scale; y++)
      for (int x = 0; x < SCREEN_W * scale; x++) {
        uint16_t c = fb[(y / scale) * SCREEN_W + x / scale];
        unsigned char px[3] = {(unsigned char)(((c >> 11) & 31) * 255 / 31),
                               (unsigned char)(((c >> 5) & 63) * 255 / 63), (unsigned char)((c & 31) * 255 / 31)};
        fwrite(px, 1, 3, f);
      }
    fclose(f);
  }
};

class SimPlatform : public Platform {
 public:
  uint32_t t = 1000;
  uint32_t millis() override { return t; }
  int battery() override { return 76; }
  bool hasOrbita() override { return false; }  // simula o firmware sozinho
};

int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "uso: %s \"roteiro\" pasta\n", argv[0]);
    return 1;
  }
  static FbGfx g;
  static SimPlatform plat;
  static App app;
  app.begin(&plat);
  std::string sc = argv[1], out = argv[2];
  auto press = [&](Key k) {
    app.key(k);
    plat.t += 150;
    app.frameDue(plat.t);
  };
  for (size_t i = 0; i < sc.size(); i++) {
    Key k = {K_CHAR, 0, false, false, false, false};
    char c = sc[i];
    if (c == '{') {
      size_t e = sc.find('}', i);
      std::string tok = sc.substr(i + 1, e - i - 1);
      i = e;
      if (tok.compare(0, 5, "shot:") == 0) {
        plat.t = (plat.t / 900) * 900 + 10;  // cursor visível
        app.frameDue(plat.t);
        app.render(g);
        g.savePPM((out + "/" + tok.substr(5) + ".ppm").c_str(), 3);
        continue;
      }
      if (tok.compare(0, 5, "wait:") == 0) {
        uint32_t ms = (uint32_t)atoi(tok.c_str() + 5);
        for (uint32_t w = 0; w < ms; w += 33) {
          plat.t += 33;
          if (app.frameDue(plat.t)) app.render(g);
        }
        continue;
      }
      if (tok.compare(0, 3, "fn+") == 0) {
        k.fn = true;
        tok = tok.substr(3);
      } else if (tok.compare(0, 5, "ctrl+") == 0) {
        k.ctrl = true;
        tok = tok.substr(5);
      }
      if (tok == "enter") k.code = K_ENTER;
      else if (tok == "del") k.code = K_DEL;
      else if (tok == "tab") k.code = K_TAB;
      else if (tok == "esc") k.code = K_BACK;
      else k.ch = tok[0];
      press(k);
      continue;
    }
    if (c == '`') k.code = K_BACK;
    else k.ch = c;
    press(k);
  }
  return 0;
}
