// Prisma — calculadora gráfica científica para o M5Stack Cardputer ADV
#pragma once
#include "expr.h"
#include "gfx.h"
#include "ui.h"

namespace px {

#define PRISMA_VERSION "0.1.0"

enum { NFUNC = 4, FN_LEN = 80 };

struct Settings {
  uint8_t degrees;
  uint8_t accent;
  uint8_t res3d;       // quadrados por lado: 16..40
  uint8_t brightness;  // 10..100 (%)
  uint8_t style3d;     // 0 sólido+malha, 1 sólido, 2 malha
  uint8_t box3d;       // caixa e eixos no 3D
  int8_t fix;          // -1 automático, 0..9 casas fixas
  uint8_t pad;
};

// Tudo que sobrevive a desligar (gravado inteiro na NVS).
struct State {
  uint32_t magic;
  uint16_t version, size;
  Settings set;
  char f[NFUNC][FN_LEN];
  uint8_t fen[NFUNC];
  char z[FN_LEN];
  double win[4];  // janela 2D: xmin xmax ymin ymax
  double r3[4];   // domínio 3D: xmin xmax ymin ymax
  double tbl[2];  // tabela: início, passo
  double vars[NVARS];
};

class App;

class View {
 public:
  App* app;
  View() : app(nullptr) {}
  virtual ~View() {}
  virtual const char* title() = 0;
  virtual void enter() {}
  virtual void leave() {}
  virtual void key(const Key& k) = 0;
  virtual void render(Gfx& g) = 0;
  virtual bool animating() { return false; }
  virtual bool hasCursor() { return false; }
  virtual void menuPick(int tag, int idx) { (void)tag; (void)idx; }
  virtual void formDone(int tag) { (void)tag; }
};

View* makeHome();
View* makeCalc();
View* makeFuncs();
View* makeGraph();
View* makeTable();
View* makeGraph3D();
View* makeSettings();

class App {
 public:
  App();
  ~App();
  void begin(Platform* p);
  void key(const Key& k);
  bool frameDue(uint32_t now);  // precisa redesenhar?
  void render(Gfx& g);

  void go(View* v);
  void statusBar(Gfx& g, const char* title, const char* center = nullptr);
  bool blink() const { return ((now_ / 450) & 1) == 0; }
  uint32_t now() const { return now_; }

  // sobreposições
  ui::Menu menu;
  ui::Form form;
  ui::TextBox box;
  void openMenu() { menu.show(); }
  void toast(const char* msg, bool error = false);

  // estado e avaliação
  State st;
  Context ctx;  // variáveis da calculadora (a..z, ans)
  Program fprog[NFUNC];
  Program zprog;
  void compileFunctions();
  void compileZ();
  double evalF(int i, double x);
  double evalZ(double x, double y);
  bool fActive(int i) const { return st.fen[i] && fprog[i].ok(); }
  void defaults();
  void applySettings();
  void requestSave();
  void flushSave();
  Platform* plat;

  View *home, *calc, *funcs, *graph, *table, *g3d, *settings;
  View* cur;

 private:
  uint32_t now_;
  bool dirty_;
  bool lastBlink_;
  uint32_t saveAt_;
  char toastMsg_[48];
  bool toastErr_;
  uint32_t toastUntil_;
};

}  // namespace px
