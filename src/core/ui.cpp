#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace px {
namespace ui {

Theme th = {
    rgb(10, 13, 20),    // bg
    rgb(21, 27, 39),    // panel
    rgb(30, 38, 54),    // panel2
    rgb(48, 58, 78),    // edge
    rgb(230, 234, 242), // text
    rgb(140, 150, 170), // dim
    rgb(80, 90, 110),   // faint
    rgb(24, 31, 45),    // grid
    rgb(95, 106, 128),  // axis
    rgb(45, 212, 191),  // accent
    rgb(20, 90, 84),    // accentDim
    rgb(8, 20, 24),     // onAccent
    rgb(255, 92, 108),  // err
    rgb(74, 222, 128),  // ok
    {rgb(79, 195, 247), rgb(255, 107, 107), rgb(124, 219, 106), rgb(255, 181, 71)},
};

const char* const kAccentNames[] = {"Turquesa", "Ambar", "Violeta", "Verde", "Rosa", "Azul"};
const int kAccentCount = 6;
static const uint8_t kAccentRGB[][3] = {{45, 212, 191}, {255, 176, 32}, {167, 139, 250},
                                        {74, 222, 128}, {244, 114, 182}, {96, 165, 250}};

void setAccent(int idx) {
  if (idx < 0 || idx >= kAccentCount) idx = 0;
  const uint8_t* c = kAccentRGB[idx];
  th.accent = rgb(c[0], c[1], c[2]);
  th.accentDim = mix(th.bg, th.accent, 90);
  th.onAccent = rgb(c[0] / 9, c[1] / 9, c[2] / 9);
}

void textC(Gfx& g, int cx, int y, const char* s, color_t c, int size) {
  g.text(cx - tw(s, size) / 2, y, s, c, size);
}

void textR(Gfx& g, int rx, int y, const char* s, color_t c, int size) {
  g.text(rx - tw(s, size), y, s, c, size);
}

void textClip(Gfx& g, int x, int y, const char* s, int maxChars, color_t c, int size) {
  int n = (int)strlen(s);
  if (n <= maxChars) {
    g.text(x, y, s, c, size);
    return;
  }
  char buf[128];
  if (maxChars < 3) maxChars = 3;
  if (maxChars > (int)sizeof(buf) - 1) maxChars = sizeof(buf) - 1;
  memcpy(buf, s, maxChars - 2);
  buf[maxChars - 2] = '.';
  buf[maxChars - 1] = '.';
  buf[maxChars] = 0;
  g.text(x, y, buf, c, size);
}

void statusBar(Gfx& g, const char* title, const char* center, bool degrees, int battery) {
  g.fillRect(0, 0, SCREEN_W, 12, th.panel);
  int w = tw(title) + 8;
  g.fillRect(0, 0, w, 12, th.accent);
  g.text(4, 2, title, th.onAccent);
  int right = SCREEN_W - 3;
  if (battery >= 0) {
    // bateria: contorno 13x7 + ponta
    int bx = right - 15, by = 3;
    g.drawRect(bx, by, 13, 7, th.dim);
    g.fillRect(bx + 13, by + 2, 2, 3, th.dim);
    int fill = (battery * 11 + 50) / 100;
    if (fill > 0) g.fillRect(bx + 1, by + 1, fill, 5, battery <= 15 ? th.err : th.dim);
    right = bx - 4;
  }
  const char* ang = degrees ? "DEG" : "RAD";
  textR(g, right, 2, ang, th.dim);
  right -= tw(ang) + 6;
  if (center && *center) {
    int maxc = (right - (w + 6)) / FONT_W;
    textClip(g, w + 6, 2, center, maxc, th.text);
  }
}

void overlayFrame(Gfx& g, int x, int y, int w, int h, const char* title) {
  g.fillRoundRect(x + 2, y + 3, w, h, 4, rgb(0, 0, 0));  // sombra
  g.fillRoundRect(x, y, w, h, 4, th.panel);
  g.drawRoundRect(x, y, w, h, 4, th.edge);
  if (title && *title) {
    g.fillRect(x + 1, y + 13, w - 2, 1, th.edge);
    g.text(x + 6, y + 3, title, th.accent);
  }
}

// ------------------------------------------------------------ LineEdit
LineEdit::LineEdit() : len(0), cur(0), scroll(0), maxLen(CAP - 1) { buf[0] = 0; }

void LineEdit::set(const char* s) {
  len = 0;
  while (s[len] && len < maxLen) {
    buf[len] = s[len];
    len++;
  }
  buf[len] = 0;
  cur = len;
  scroll = 0;
}

void LineEdit::clear() {
  len = cur = scroll = 0;
  buf[0] = 0;
}

void LineEdit::insert(const char* s) {
  int n = (int)strlen(s);
  if (len + n > maxLen) n = maxLen - len;
  if (n <= 0) return;
  memmove(buf + cur + n, buf + cur, len - cur + 1);
  memcpy(buf + cur, s, n);
  len += n;
  cur += n;
}

bool LineEdit::key(const Key& k) {
  if (k.code == K_DEL) {
    if (k.ctrl) {
      clear();
    } else if (k.fn) {  // apaga para a frente
      if (cur < len) {
        memmove(buf + cur, buf + cur + 1, len - cur);
        len--;
      }
    } else if (cur > 0) {
      memmove(buf + cur - 1, buf + cur, len - cur + 1);
      cur--;
      len--;
    }
    return true;
  }
  if (k.code != K_CHAR) return false;
  if (k.fn || k.ctrl) {
    bool jump = k.ctrl;  // ctrl+, / ctrl+/ = início / fim
    if (k.ch == ',') {
      cur = jump ? 0 : (cur > 0 ? cur - 1 : 0);
      return true;
    }
    if (k.ch == '/') {
      cur = jump ? len : (cur < len ? cur + 1 : len);
      return true;
    }
    return false;
  }
  if (k.ch >= 32 && k.ch < 127 && k.ch != '`') {
    char s[2] = {k.ch, 0};
    insert(s);
    return true;
  }
  return false;
}

void LineEdit::draw(Gfx& g, int x, int y, int w, int size, color_t fg, bool focused, bool blink) {
  int cw = FONT_W * size;
  int vis = w / cw;
  if (vis < 2) vis = 2;
  if (cur < scroll) scroll = cur;
  if (cur > scroll + vis - 1) scroll = cur - vis + 1;
  if (scroll > 0 && len - scroll < vis - 1) scroll = len - vis + 1 > 0 ? len - vis + 1 : 0;
  char tmp[CAP];
  int n = len - scroll;
  if (n > vis) n = vis;
  if (n < 0) n = 0;
  memcpy(tmp, buf + scroll, n);
  tmp[n] = 0;
  g.text(x, y, tmp, fg, size);
  int ah = 4 * size;
  if (scroll > 0) g.fillTriangle(x - 2, y + ah - 3, x - 2, y + ah + 3, x - 5, y + ah, th.dim);
  if (len - scroll > vis) {
    int rx = x + vis * cw + 1;
    g.fillTriangle(rx, y + ah - 3, rx, y + ah + 3, rx + 3, y + ah, th.dim);
  }
  if (focused && blink) g.fillRect(x + (cur - scroll) * cw - 1, y - 1, size > 1 ? 2 : 1, FONT_H * size + 1, th.accent);
}

// ------------------------------------------------------------ Menu
void Menu::begin(View* o, int t, const char* ttl) {
  owner = o;
  tag = t;
  snprintf(title, sizeof title, "%s", ttl);
  n = 0;
}

void Menu::add(const char* s) {
  if (n >= MAXI) return;
  snprintf(items[n++], ITEMLEN, "%s", s);
}

void Menu::show(int s) {
  sel = (s >= 0 && s < n) ? s : 0;
  top = 0;
  open = n > 0;
}

int Menu::key(const Key& k) {
  enum { ROWS = 8 };
  if (k.code == K_BACK || k.code == K_TAB) {
    open = false;
    return -1;
  }
  if (k.code == K_ENTER) {
    open = false;
    return sel;
  }
  if (k.code == K_CHAR && !k.fn && k.ch >= '1' && k.ch <= '9' && k.ch - '1' < n) {
    open = false;
    return k.ch - '1';
  }
  Nav nv = arrowOf(k, false);
  if (nv == Nav::Up) sel = sel > 0 ? sel - 1 : n - 1;
  if (nv == Nav::Down) sel = sel < n - 1 ? sel + 1 : 0;
  if (nv == Nav::Left) sel = sel - ROWS > 0 ? sel - ROWS : 0;
  if (nv == Nav::Right) sel = sel + ROWS < n - 1 ? sel + ROWS : n - 1;
  if (sel < top) top = sel;
  if (sel >= top + ROWS) top = sel - ROWS + 1;
  return -2;
}

void Menu::draw(Gfx& g) {
  enum { ROWS = 8, RH = 11 };
  int maxw = tw(title) + 12;
  for (int i = 0; i < n; i++) {
    int w = tw(items[i]) + 24;
    if (w > maxw) maxw = w;
  }
  if (maxw < 120) maxw = 120;
  if (maxw > 232) maxw = 232;
  int rows = n < ROWS ? n : ROWS;
  int h = 16 + rows * RH + 3;
  int x = (SCREEN_W - maxw) / 2, y = (SCREEN_H - h) / 2 + 4;
  overlayFrame(g, x, y, maxw, h, title);
  int maxc = (maxw - 22) / FONT_W;
  for (int r = 0; r < rows; r++) {
    int i = top + r;
    int ry = y + 16 + r * RH;
    bool s = i == sel;
    if (s) g.fillRoundRect(x + 3, ry - 1, maxw - 6, RH, 2, th.accent);
    if (i < 9) {
      char num[2] = {(char)('1' + i), 0};
      g.text(x + 6, ry + 1, num, s ? th.onAccent : th.faint);
    }
    textClip(g, x + 16, ry + 1, items[i], maxc, s ? th.onAccent : th.text);
  }
  if (n > ROWS) {  // barra de rolagem
    int bh = rows * RH;
    int th_ = bh * ROWS / n;
    if (th_ < 6) th_ = 6;
    int ty = y + 16 + (bh - th_) * top / (n - ROWS);
    g.fillRect(x + maxw - 3, ty, 2, th_, th.dim);
  }
}

// ------------------------------------------------------------ Form
void Form::begin(View* o, int t, const char* ttl) {
  owner = o;
  tag = t;
  snprintf(title, sizeof title, "%s", ttl);
  n = 0;
  err[0] = 0;
  errField = -1;
}

void Form::add(const char* lbl, const char* init, Kind k) {
  if (n >= MAXF) return;
  snprintf(label[n], sizeof label[n], "%s", lbl);
  ed[n].maxLen = 79;
  ed[n].set(init);
  kind[n] = k;
  val[n] = 0;
  n++;
}

void Form::addNum(const char* lbl, double v) {
  char b[32];
  if (fabs(v) < 1e-12) v = 0;
  snprintf(b, sizeof b, "%.10g", v);
  add(lbl, b, NUM);
}

void Form::show() {
  sel = 0;
  open = n > 0;
  err[0] = 0;
  errField = -1;
}

int Form::key(const Key& k, Context& ctx) {
  if (k.code == K_BACK) {
    open = false;
    return -1;
  }
  Nav nv = arrowOf(k, true);
  if (k.code == K_TAB || nv == Nav::Down) {
    sel = (sel + 1) % n;
    return 0;
  }
  if (nv == Nav::Up) {
    sel = (sel + n - 1) % n;
    return 0;
  }
  if (k.code == K_ENTER) {
    if (sel < n - 1 && !k.fn) {
      sel++;
      return 0;
    }
    // valida tudo
    for (int i = 0; i < n; i++) {
      Program p;
      Err e = p.compile(ed[i].str());
      if (e == Err::None && kind[i] == NUM) {
        val[i] = p.eval(ctx);
        if (!isfinite(val[i])) {
          errField = sel = i;
          snprintf(err, sizeof err, "%s: valor invalido", label[i]);
          return 0;
        }
      }
      if (e != Err::None) {
        errField = sel = i;
        ed[i].cur = p.errorPos();
        snprintf(err, sizeof err, "%s", errText(e));
        return 0;
      }
    }
    open = false;
    return 1;
  }
  if (ed[sel].key(k)) {
    errField = -1;
    err[0] = 0;
  }
  return 0;
}

void Form::draw(Gfx& g, bool blink) {
  enum { RH = 17 };
  int lw = 0;
  for (int i = 0; i < n; i++) {
    int w = tw(label[i]);
    if (w > lw) lw = w;
  }
  int w = 228, h = 18 + n * RH + 12;
  int x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2 + 4;
  if (y < 13) y = 13;
  overlayFrame(g, x, y, w, h, title);
  for (int i = 0; i < n; i++) {
    int ry = y + 18 + i * RH;
    bool s = i == sel;
    g.text(x + 6, ry + 4, label[i], s ? th.text : th.dim);
    int ex = x + 12 + lw, ew = w - (ex - x) - 6;
    g.fillRoundRect(ex, ry, ew, RH - 3, 2, th.panel2);
    color_t border = i == errField ? th.err : (s ? th.accent : th.edge);
    g.drawRoundRect(ex, ry, ew, RH - 3, 2, border);
    ed[i].draw(g, ex + 5, ry + 3, ew - 10, 1, th.text, s, blink);
  }
  int fy = y + h - 11;
  if (err[0]) g.text(x + 6, fy, err, th.err);
  else textR(g, x + w - 6, fy, n > 1 ? "Tab: campo  Enter: ok" : "Enter: ok  `: cancelar", th.faint);
}

// ------------------------------------------------------------ TextBox
void TextBox::show(const char* ttl, const char* b) {
  snprintf(title, sizeof title, "%s", ttl);
  body = b;
  top = 0;
  lines = 1;
  for (const char* p = b; *p; p++)
    if (*p == '\n') lines++;
  open = true;
}

void TextBox::key(const Key& k) {
  enum { ROWS = 10 };
  Nav nv = arrowOf(k, false);
  if (nv == Nav::Up) {
    if (top > 0) top--;
    return;
  }
  if (nv == Nav::Down) {
    if (top < lines - ROWS) top++;
    return;
  }
  open = false;
}

void TextBox::draw(Gfx& g) {
  enum { ROWS = 10, LH = 10 };
  int x = 4, y = 14, w = SCREEN_W - 8, h = SCREEN_H - 16;
  overlayFrame(g, x, y, w, h, title);
  const char* p = body;
  int line = 0;
  char buf[64];
  while (*p && line < top + ROWS) {
    const char* e = strchr(p, '\n');
    int n = e ? (int)(e - p) : (int)strlen(p);
    if (line >= top) {
      int m = n < (int)sizeof(buf) - 1 ? n : (int)sizeof(buf) - 1;
      memcpy(buf, p, m);
      buf[m] = 0;
      // linhas que começam com '#' são subtítulos
      if (buf[0] == '#') g.text(x + 6, y + 17 + (line - top) * LH, buf + 1, th.accent);
      else textClip(g, x + 6, y + 17 + (line - top) * LH, buf, (w - 12) / FONT_W, th.text);
    }
    line++;
    if (!e) break;
    p = e + 1;
  }
  if (lines > ROWS) {
    int bh = ROWS * LH;
    int t = bh * ROWS / lines;
    int ty = y + 16 + (bh - t) * top / (lines - ROWS);
    g.fillRect(x + w - 4, ty, 2, t, th.dim);
  }
}

}  // namespace ui
}  // namespace px
