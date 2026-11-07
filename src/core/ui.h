// Prisma — tema e componentes de interface
#pragma once
#include "expr.h"
#include "gfx.h"

namespace px {

class View;

namespace ui {

struct Theme {
  color_t bg, panel, panel2, edge, text, dim, faint, grid, axis;
  color_t accent, accentDim, onAccent, err, ok;
  color_t func[4];
};
extern Theme th;
extern const char* const kAccentNames[];
extern const int kAccentCount;
void setAccent(int idx);

inline int tw(const char* s, int size = 1);
void textC(Gfx& g, int cx, int y, const char* s, color_t c, int size = 1);
void textR(Gfx& g, int rx, int y, const char* s, color_t c, int size = 1);
// Desenha no máximo maxChars caracteres; corta com "..".
void textClip(Gfx& g, int x, int y, const char* s, int maxChars, color_t c, int size = 1);
void statusBar(Gfx& g, const char* title, const char* center, bool degrees, int battery);
void overlayFrame(Gfx& g, int x, int y, int w, int h, const char* title);

// Campo de texto de uma linha com cursor e rolagem horizontal.
class LineEdit {
 public:
  enum { CAP = 96 };
  LineEdit();
  void set(const char* s);
  void clear();
  void insert(const char* s);
  const char* str() const { return buf; }
  bool empty() const { return len == 0; }
  bool key(const Key& k);  // true se consumiu a tecla
  void draw(Gfx& g, int x, int y, int w, int size, color_t fg, bool focused, bool blink);

  char buf[CAP];
  int len, cur, scroll, maxLen;
};

// Menu suspenso (lista). Escolha: Enter ou 1-9; ` cancela.
struct Menu {
  enum { MAXI = 60, ITEMLEN = 40 };
  bool open;
  View* owner;
  int tag;
  char title[32];
  char items[MAXI][ITEMLEN];
  int n, sel, top;
  Menu() : open(false), owner(nullptr), tag(0), n(0), sel(0), top(0) { title[0] = 0; }
  void begin(View* o, int t, const char* ttl);
  void add(const char* s);
  void show(int s = 0);
  int key(const Key& k);  // -2 continua, -1 cancelou, >=0 item escolhido
  void draw(Gfx& g);
};

// Formulário: alguns campos com rótulo. Campos numéricos aceitam expressões ("2pi").
struct Form {
  enum { MAXF = 5 };
  enum Kind : uint8_t { NUM, EXPR };
  bool open;
  View* owner;
  int tag;
  char title[32];
  int n;
  char label[MAXF][14];
  LineEdit ed[MAXF];
  Kind kind[MAXF];
  double val[MAXF];
  int sel, errField;
  char err[40];
  Form() : open(false), owner(nullptr), tag(0), n(0), sel(0), errField(-1) { title[0] = err[0] = 0; }
  void begin(View* o, int t, const char* ttl);
  void add(const char* lbl, const char* init, Kind k = NUM);
  void addNum(const char* lbl, double v);
  void show();
  int key(const Key& k, Context& ctx);  // 1 confirmou, -1 cancelou, 0 continua
  void draw(Gfx& g, bool blink);
};

// Caixa de texto rolável (ajuda, variáveis).
struct TextBox {
  bool open;
  char title[32];
  const char* body;
  int top, lines;
  TextBox() : open(false), body(nullptr), top(0), lines(0) { title[0] = 0; }
  void show(const char* ttl, const char* b);
  void key(const Key& k);
  void draw(Gfx& g);
};

inline int tw(const char* s, int size) {
  int n = 0;
  while (s[n]) n++;
  return n * FONT_W * size;
}

}  // namespace ui
}  // namespace px
