#include "app.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "numeric.h"

namespace px {

using namespace ui;

static const uint32_t kMagic = 0x50525A31;  // "PRZ1"
static const uint16_t kVersion = 1;

static double userFnThunk(void* u, int idx, double x) { return ((App*)u)->evalF(idx, x); }

App::App()
    : plat(nullptr), cur(nullptr), now_(0), dirty_(true), lastBlink_(false), saveAt_(0), toastErr_(false),
      toastUntil_(0) {
  toastMsg_[0] = 0;
  home = makeHome();
  calc = makeCalc();
  funcs = makeFuncs();
  graph = makeGraph();
  table = makeTable();
  g3d = makeGraph3D();
  settings = makeSettings();
  View* all[] = {home, calc, funcs, graph, table, g3d, settings};
  for (View* v : all) v->app = this;
}

App::~App() {
  View* all[] = {home, calc, funcs, graph, table, g3d, settings};
  for (View* v : all) delete v;
}

void App::defaults() {
  memset(&st, 0, sizeof st);
  st.magic = kMagic;
  st.version = kVersion;
  st.size = sizeof(State);
  st.set.degrees = 0;
  st.set.accent = 0;
  st.set.res3d = 32;
  st.set.brightness = 80;
  st.set.style3d = 0;
  st.set.box3d = 1;
  st.set.fix = -1;
  strcpy(st.f[0], "sin(x)");
  strcpy(st.f[1], "x^2/4-2");
  st.fen[0] = st.fen[1] = 1;
  strcpy(st.z, "sin(x)cos(y)");
  st.win[0] = -10;
  st.win[1] = 10;
  st.win[2] = -5.125;
  st.win[3] = 5.125;
  st.r3[0] = st.r3[2] = -kPi;
  st.r3[1] = st.r3[3] = kPi;
  st.tbl[0] = 0;
  st.tbl[1] = 1;
}

void App::begin(Platform* p) {
  plat = p;
  now_ = plat->millis();
  defaults();
  State tmp;
  if (plat->load(&tmp, sizeof tmp) && tmp.magic == kMagic && tmp.version == kVersion && tmp.size == sizeof(State)) {
    st = tmp;
    for (int i = 0; i < NFUNC; i++) st.f[i][FN_LEN - 1] = 0;
    st.z[FN_LEN - 1] = 0;
  }
  for (int i = 0; i < NVARS; i++) ctx.vars[i] = isfinite(st.vars[i]) ? st.vars[i] : 0;
  ctx.userFn = userFnThunk;
  ctx.user = this;
  applySettings();
  compileFunctions();
  compileZ();
  go(home);
}

void App::applySettings() {
  setAccent(st.set.accent);
  ctx.degrees = st.set.degrees != 0;
  if (st.set.res3d < 8 || st.set.res3d > 40) st.set.res3d = 32;
  if (st.set.brightness < 10 || st.set.brightness > 100) st.set.brightness = 80;
  if (plat->hasBrightness()) plat->setBrightness(st.set.brightness);
}

void App::compileFunctions() {
  for (int i = 0; i < NFUNC; i++) fprog[i].compile(st.f[i]);
}

void App::compileZ() { zprog.compile(st.z); }

double App::evalF(int i, double x) {
  if (i < 0 || i >= NFUNC || !fprog[i].ok()) return NAN;
  double save = ctx.vars[VAR_X];
  ctx.vars[VAR_X] = x;
  double r = fprog[i].eval(ctx);
  ctx.vars[VAR_X] = save;
  return r;
}

double App::evalZ(double x, double y) {
  if (!zprog.ok()) return NAN;
  double sx = ctx.vars[VAR_X], sy = ctx.vars[VAR_Y];
  ctx.vars[VAR_X] = x;
  ctx.vars[VAR_Y] = y;
  double r = zprog.eval(ctx);
  ctx.vars[VAR_X] = sx;
  ctx.vars[VAR_Y] = sy;
  return r;
}

void App::requestSave() { saveAt_ = now_ + 1500; if (!saveAt_) saveAt_ = 1; }

void App::flushSave() {
  for (int i = 0; i < NVARS; i++) st.vars[i] = ctx.vars[i];
  plat->save(&st, sizeof st);
  saveAt_ = 0;
}

void App::go(View* v) {
  if (cur) cur->leave();
  cur = v;
  menu.open = form.open = box.open = false;
  cur->enter();
  dirty_ = true;
}

void App::toast(const char* msg, bool error) {
  snprintf(toastMsg_, sizeof toastMsg_, "%s", msg);
  toastErr_ = error;
  toastUntil_ = now_ + 1800;
  dirty_ = true;
}

void App::key(const Key& k) {
  dirty_ = true;
  toastUntil_ = toastUntil_ ? now_ + 300 : 0;  // tecla apressa o aviso
  if (box.open) {
    box.key(k);
  } else if (form.open) {
    if (form.key(k, ctx) == 1 && form.owner) form.owner->formDone(form.tag);
  } else if (menu.open) {
    int r = menu.key(k);
    if (r >= 0 && menu.owner) menu.owner->menuPick(menu.tag, r);
  } else {
    cur->key(k);
  }
}

bool App::frameDue(uint32_t now) {
  now_ = now;
  if (saveAt_ && now >= saveAt_) flushSave();
  if (toastUntil_ && now >= toastUntil_) {
    toastUntil_ = 0;
    dirty_ = true;
  }
  bool b = blink();
  if (b != lastBlink_) {
    lastBlink_ = b;
    if (form.open || (!menu.open && !box.open && cur->hasCursor())) dirty_ = true;
  }
  return dirty_ || (cur->animating() && !menu.open && !form.open && !box.open);
}

void App::render(Gfx& g) {
  cur->render(g);
  if (menu.open) menu.draw(g);
  if (form.open) form.draw(g, blink());
  if (box.open) box.draw(g);
  if (toastUntil_) {
    int w = tw(toastMsg_) + 16;
    if (w > SCREEN_W - 8) w = SCREEN_W - 8;
    int x = (SCREEN_W - w) / 2, y = SCREEN_H - 46;
    g.fillRoundRect(x, y, w, 16, 7, th.panel2);
    g.drawRoundRect(x, y, w, 16, 7, toastErr_ ? th.err : th.accent);
    textClip(g, x + 8, y + 4, toastMsg_, (w - 16) / FONT_W, toastErr_ ? th.err : th.text);
  }
  dirty_ = false;
}

void App::statusBar(Gfx& g, const char* title, const char* center) {
  ui::statusBar(g, title, center, st.set.degrees != 0, plat->battery());
}

static const char kAbout[] =
    "#Prisma " PRISMA_VERSION "\n"
    "Calculadora grafica cientifica\n"
    "para o M5Stack Cardputer ADV.\n"
    "\n"
    "#Modos\n"
    "Calculo, graficos 2D com analise,\n"
    "superficies 3D z=f(x,y), tabela.\n"
    "\n"
    "#Teclas\n"
    "; , . /  setas (fn em campos)\n"
    "`        voltar\n"
    "Tab      opcoes de cada tela\n"
    "\n"
    "github.com/TiroLiroLa/Prisma";

// ======================================================================= Home
class HomeView : public View {
 public:
  int sel = 0;
  const char* title() override { return "Prisma"; }

  void key(const Key& k) override {
    Nav n = arrowOf(k, false);
    if (n == Nav::Left) sel = (sel + 5) % 6;
    if (n == Nav::Right) sel = (sel + 1) % 6;
    if (n == Nav::Up || n == Nav::Down) sel = (sel + 3) % 6;
    if (k.code == K_CHAR && k.ch >= '1' && k.ch <= '6') {
      sel = k.ch - '1';
      open();
    }
    if (k.code == K_ENTER) open();
    if (k.code == K_BACK && app->plat->embedded()) leaveToOrbita();
  }

  void leaveToOrbita() {
    app->flushSave();
    app->plat->exitToOrbita();
  }

  void menuPick(int tag, int idx) override {
    if (tag == 1 && idx == 0) {
      app->flushSave();
      app->plat->exitToOrbita();
    }
  }

  void open() {
    switch (sel) {
      case 0: app->go(app->calc); break;
      case 1: app->go(app->funcs); break;
      case 2: app->go(app->g3d); break;
      case 3: app->go(app->table); break;
      case 4: app->go(app->settings); break;
      case 5:
        if (app->plat->embedded()) {  // dentro do Órbita: só fecha o app
          leaveToOrbita();
          break;
        }
        if (!app->plat->hasOrbita()) {  // firmware sozinho: o bloco vira "Sobre"
          app->box.show("Sobre", kAbout);
          break;
        }
        app->menu.begin(this, 1, "Voltar ao Orbita?");
        app->menu.add("Sim");
        app->menu.add("Nao");
        app->openMenu();
        break;
    }
  }

  static void icon(Gfx& g, int i, int cx, int cy, color_t c, color_t d) {
    switch (i) {
      case 0: {  // calculadora
        g.drawRoundRect(cx - 9, cy - 12, 18, 24, 2, c);
        g.fillRect(cx - 6, cy - 9, 12, 5, c);
        for (int r = 0; r < 3; r++)
          for (int q = 0; q < 3; q++) g.fillRect(cx - 6 + q * 5, cy - 1 + r * 4, 3, 2, r == 2 && q == 2 ? c : d);
        break;
      }
      case 1: {  // gráfico
        g.hline(cx - 14, cy + 2, 29, d);
        g.vline(cx - 10, cy - 12, 25, d);
        int px = cx - 14, py = cy + 2;
        for (int x = -14; x <= 14; x++) {
          int y = cy + 2 - (int)lround(8 * sin(x * 0.33));
          g.line(px, py, cx + x, y, c);
          g.line(px, py + 1, cx + x, y + 1, c);
          px = cx + x;
          py = y;
        }
        break;
      }
      case 2: {  // malha 3D
        const int N = 5;
        int X[N + 1][N + 1], Y[N + 1][N + 1];
        for (int a = 0; a <= N; a++)
          for (int b = 0; b <= N; b++) {
            double u = a / (double)N - 0.5, v = b / (double)N - 0.5;
            double z = 0.35 * cos(6 * sqrt(u * u + v * v));
            X[a][b] = cx + (int)lround((u - v) * 26);
            Y[a][b] = cy + 2 + (int)lround((u + v) * 11 - z * 20);
          }
        for (int a = 0; a <= N; a++)
          for (int b = 0; b < N; b++) {
            g.line(X[a][b], Y[a][b], X[a][b + 1], Y[a][b + 1], b < 2 ? d : c);
            g.line(X[b][a], Y[b][a], X[b + 1][a], Y[b + 1][a], b < 2 ? d : c);
          }
        break;
      }
      case 3: {  // tabela
        g.drawRect(cx - 13, cy - 10, 26, 21, c);
        g.fillRect(cx - 13, cy - 10, 26, 5, c);
        for (int r = 1; r < 4; r++) g.hline(cx - 13, cy - 5 + r * 4, 26, d);
        g.vline(cx - 4, cy - 10, 21, d);
        g.vline(cx + 5, cy - 10, 21, d);
        break;
      }
      case 4: {  // engrenagem
        for (int t = 0; t < 8; t++) {
          double a = t * kPi / 4;
          int x0 = cx + (int)lround(cos(a) * 8), y0 = cy + (int)lround(sin(a) * 8);
          int x1 = cx + (int)lround(cos(a) * 12), y1 = cy + (int)lround(sin(a) * 12);
          g.line(x0, y0, x1, y1, c);
          g.line(x0 + 1, y0, x1 + 1, y1, c);
        }
        g.fillCircle(cx, cy, 9, c);
        g.fillCircle(cx, cy, 4, th.panel);
        break;
      }
      case 6: {  // "i" de informações
        g.drawCircle(cx, cy, 11, c);
        g.drawCircle(cx, cy, 12, c);
        g.fillRect(cx - 1, cy - 2, 3, 9, c);
        g.fillRect(cx - 1, cy - 7, 3, 3, c);
        break;
      }
      case 5: {  // planeta com órbita
        g.fillCircle(cx, cy, 7, c);
        int px = 0, py = 0;
        for (int t = 0; t <= 40; t++) {
          double a = t * 2 * kPi / 40;
          double ex = cos(a) * 15, ey = sin(a) * 5;
          int x = cx + (int)lround(ex * 0.94 - ey * 0.34), y = cy + (int)lround(ex * 0.34 + ey * 0.94);
          bool behind = sin(a) < 0;
          if (t > 0 && !(behind && abs(x - cx) < 7 && abs(y - cy) < 7)) g.line(px, py, x, y, d);
          px = x;
          py = y;
        }
        break;
      }
    }
  }

  void render(Gfx& g) override {
    static const char* const names[] = {"Calcular", "Graficos", "3D", "Tabela", "Ajustes", "Orbita"};
    bool orbita = app->plat->embedded() || app->plat->hasOrbita();
    g.fillScreen(th.bg);
    for (int i = 0; i < 6; i++) {
      int x = 5 + (i % 3) * 78, y = 16 + (i / 3) * 60;
      bool s = i == sel;
      g.fillRoundRect(x, y, 74, 56, 5, s ? th.accentDim : th.panel);
      if (s) {
        g.drawRoundRect(x, y, 74, 56, 5, th.accent);
        g.drawRoundRect(x + 1, y + 1, 72, 54, 4, th.accent);
      }
      char num[2] = {(char)('1' + i), 0};
      g.text(x + 5, y + 4, num, s ? th.accent : th.faint);
      bool info = i == 5 && !orbita;
      icon(g, info ? 6 : i, x + 37, y + 23, s ? th.accent : th.text, s ? mix(th.accent, th.bg, 110) : th.faint);
      textC(g, x + 37, y + 43, info ? "Sobre" : names[i], s ? th.text : th.dim);
    }
    app->statusBar(g, "Prisma", "calculadora grafica");
  }
};

// ================================================================== Ajustes
class SettingsView : public View {
 public:
  int sel = 0;
  enum { ROWS = 7 };
  const char* title() override { return "Ajustes"; }

  void leave() override { app->requestSave(); }

  void change(int dir) {
    Settings& s = app->st.set;
    switch (sel) {
      case 0: s.degrees = !s.degrees; break;
      case 1: s.fix = (int8_t)(s.fix + dir < -1 ? 9 : (s.fix + dir > 9 ? -1 : s.fix + dir)); break;
      case 2: s.accent = (uint8_t)((s.accent + dir + kAccentCount) % kAccentCount); break;
      case 3: {
        static const uint8_t r[] = {16, 24, 32, 40};
        int i = 0;
        while (i < 3 && r[i] < s.res3d) i++;
        i = (i + dir + 4) % 4;
        s.res3d = r[i];
        break;
      }
      case 4: {
        if (!app->plat->hasBrightness()) return;
        int b = s.brightness + dir * 10;
        s.brightness = (uint8_t)(b < 10 ? 10 : b > 100 ? 100 : b);
        break;
      }
    }
    app->applySettings();
    app->requestSave();
  }

  void key(const Key& k) override {
    Nav n = arrowOf(k, false);
    if (n == Nav::Up) sel = (sel + ROWS - 1) % ROWS;
    if (n == Nav::Down) sel = (sel + 1) % ROWS;
    if (n == Nav::Left) change(-1);
    if (n == Nav::Right) change(1);
    if (k.code == K_ENTER) {
      if (sel == 5) {
        app->menu.begin(this, 1, "Restaurar padroes?");
        app->menu.add("Sim, apagar funcoes e ajustes");
        app->menu.add("Nao");
        app->openMenu();
      } else if (sel == 6) {
        app->box.show("Sobre", kAbout);
      } else {
        change(1);
      }
    }
    if (k.code == K_BACK) app->go(app->home);
  }

  void menuPick(int tag, int idx) override {
    if (tag == 1 && idx == 0) {
      double vars[NVARS];
      memcpy(vars, app->ctx.vars, sizeof vars);
      app->defaults();
      app->applySettings();
      app->compileFunctions();
      app->compileZ();
      memcpy(app->ctx.vars, vars, sizeof vars);
      app->requestSave();
      app->toast("Padroes restaurados");
    }
  }

  void render(Gfx& g) override {
    g.fillScreen(th.bg);
    const Settings& s = app->st.set;
    char v[32];
    for (int i = 0; i < ROWS; i++) {
      int y = 16 + i * 15;
      bool on = i == sel;
      if (on) g.fillRoundRect(4, y - 2, SCREEN_W - 8, 14, 3, th.panel2);
      const char* label = "";
      v[0] = 0;
      switch (i) {
        case 0: label = "Angulo"; snprintf(v, sizeof v, "%s", s.degrees ? "Graus" : "Radianos"); break;
        case 1:
          label = "Casas decimais";
          if (s.fix < 0) snprintf(v, sizeof v, "Auto");
          else snprintf(v, sizeof v, "Fixo %d", s.fix);
          break;
        case 2: label = "Cor de destaque"; snprintf(v, sizeof v, "%s", kAccentNames[s.accent % kAccentCount]); break;
        case 3: label = "Grade 3D"; snprintf(v, sizeof v, "%dx%d", s.res3d, s.res3d); break;
        case 4:
          label = "Brilho";
          if (app->plat->hasBrightness()) snprintf(v, sizeof v, "%d%%", s.brightness);
          else snprintf(v, sizeof v, "pelo Orbita");
          break;
        case 5: label = "Restaurar padroes"; break;
        case 6: label = "Sobre o Prisma"; break;
      }
      g.text(10, y + 1, label, on ? th.text : th.dim);
      if (v[0]) {
        int rx = SCREEN_W - 22;  // fim do valor
        int lx = rx - tw(v);     // início do valor
        textR(g, rx, y + 1, v, on ? th.accent : th.text);
        if (on) {
          g.text(rx + 6, y + 1, ">", th.accent);
          g.text(lx - 12, y + 1, "<", th.accent);
          lx -= 12;
        }
        if (i == 2) g.fillCircle(lx - 8, y + 4, 3, th.accent);
      }
    }
    app->statusBar(g, "Ajustes");
  }
};

View* makeHome() { return new HomeView(); }
View* makeSettings() { return new SettingsView(); }

}  // namespace px
