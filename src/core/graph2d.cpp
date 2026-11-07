// Gráficos 2D: editor de funções, plotagem com traço e análise, tabela
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "numeric.h"

namespace px {

using namespace ui;

static const char kGraphHelp[] =
    "#Funcoes\n"
    "fn ; .     troca de linha\n"
    "Enter      plota\n"
    "fn+Enter   liga/desliga a funcao\n"
    "Tab        opcoes (janela, tabela)\n"
    "Use a..z guardadas: f1 = a*x^2\n"
    "#Grafico\n"
    "; , . /    move a janela\n"
    "= -        zoom (+ tambem)\n"
    "Enter / t  modo traco\n"
    "1..4       traco na funcao\n"
    "a          ajusta o eixo y\n"
    "r          janela padrao\n"
    "Tab        analise: raiz, min, max,\n"
    "           intersecao, dy/dx, integral\n"
    "`          volta\n"
    "#Traco\n"
    ", /        anda em x (fn: 10x)\n"
    "; .        troca de funcao";

struct FnRef {
  App* app;
  int i, j;
  static double one(double x, void* u) {
    FnRef* r = (FnRef*)u;
    return r->app->evalF(r->i, x);
  }
  static double diff(double x, void* u) {
    FnRef* r = (FnRef*)u;
    return r->app->evalF(r->i, x) - r->app->evalF(r->j, x);
  }
};

// =================================================================== Funções
class FuncsView : public View {
 public:
  LineEdit ed[NFUNC];
  int row = 0;
  bool loaded = false;

  const char* title() override { return "Funcoes"; }
  bool hasCursor() override { return true; }

  void enter() override {
    for (int i = 0; i < NFUNC; i++) {
      ed[i].maxLen = FN_LEN - 1;
      if (!loaded || strcmp(ed[i].str(), app->st.f[i])) ed[i].set(app->st.f[i]);
    }
    loaded = true;
  }

  void commit() {
    bool changed = false;
    for (int i = 0; i < NFUNC; i++) {
      if (strcmp(app->st.f[i], ed[i].str())) {
        snprintf(app->st.f[i], FN_LEN, "%s", ed[i].str());
        changed = true;
        if (!ed[i].empty()) app->st.fen[i] = 1;
      }
    }
    if (changed) {
      app->compileFunctions();
      app->requestSave();
    }
  }
  void leave() override { commit(); }

  void plot() {
    commit();
    int ok = 0;
    for (int i = 0; i < NFUNC; i++) ok += app->fActive(i);
    if (!ok) {
      app->toast("Nenhuma funcao valida ativa", true);
      return;
    }
    app->go(app->graph);
  }

  void menuPick(int tag, int idx) override {
    if (tag != 1) return;
    switch (idx) {
      case 0: plot(); break;
      case 1:
        app->st.fen[row] = !app->st.fen[row];
        app->requestSave();
        break;
      case 2:
        ed[row].clear();
        commit();
        break;
      case 3:
        app->form.begin(this, 1, "Janela do grafico");
        app->form.addNum("x min", app->st.win[0]);
        app->form.addNum("x max", app->st.win[1]);
        app->form.addNum("y min", app->st.win[2]);
        app->form.addNum("y max", app->st.win[3]);
        app->form.show();
        break;
      case 4:
        commit();
        app->go(app->table);
        break;
      case 5: app->box.show("Ajuda - Graficos", kGraphHelp); break;
    }
  }

  void formDone(int tag) override {
    if (tag != 1) return;
    double* v = app->form.val;
    if (!(v[0] < v[1]) || !(v[2] < v[3])) {
      app->toast("min precisa ser menor que max", true);
      return;
    }
    for (int i = 0; i < 4; i++) app->st.win[i] = v[i];
    app->requestSave();
  }

  void key(const Key& k) override {
    Nav n = arrowOf(k, true);
    if (n == Nav::Up) {
      row = (row + NFUNC - 1) % NFUNC;
      return;
    }
    if (n == Nav::Down) {
      row = (row + 1) % NFUNC;
      return;
    }
    if (k.code == K_ENTER) {
      if (k.fn) {
        app->st.fen[row] = !app->st.fen[row];
        app->requestSave();
      } else {
        plot();
      }
      return;
    }
    if (k.code == K_TAB) {
      char buf[32];
      app->menu.begin(this, 1, "Graficos");
      app->menu.add("Plotar");
      snprintf(buf, sizeof buf, "%s f%d", app->st.fen[row] ? "Desligar" : "Ligar", row + 1);
      app->menu.add(buf);
      snprintf(buf, sizeof buf, "Limpar f%d", row + 1);
      app->menu.add(buf);
      app->menu.add("Janela...");
      app->menu.add("Tabela de valores");
      app->menu.add("Ajuda");
      app->openMenu();
      return;
    }
    if (k.code == K_BACK) {
      commit();
      app->go(app->home);
      return;
    }
    bool wasEmpty = ed[row].empty();
    ed[row].key(k);
    if (wasEmpty && !ed[row].empty()) app->st.fen[row] = 1;  // digitou numa linha vazia: liga
  }

  void render(Gfx& g) override {
    g.fillScreen(th.bg);
    for (int i = 0; i < NFUNC; i++) {
      int y = 16 + i * 24;
      bool s = i == row;
      bool on = app->st.fen[i];
      color_t fc = th.func[i];
      if (on) g.fillCircle(10, y + 9, 4, fc);
      else g.drawCircle(10, y + 9, 4, th.faint);
      char lbl[8];
      snprintf(lbl, sizeof lbl, "f%d(x)=", i + 1);
      g.text(18, y + 6, lbl, on ? fc : th.faint);
      int ex = 60, ew = SCREEN_W - ex - 4;
      // erro de compilação desta linha (só depois de editar)
      Program p;
      bool bad = false;
      if (!ed[i].empty()) bad = p.compile(ed[i].str()) != Err::None;
      g.fillRoundRect(ex, y, ew, 19, 3, s ? th.panel2 : th.panel);
      g.drawRoundRect(ex, y, ew, 19, 3, bad ? th.err : (s ? th.accent : th.panel));
      ed[i].draw(g, ex + 6, y + 6, ew - 12, 1, on ? th.text : th.dim, s, app->blink());
    }
    // rodapé
    Program p;
    Err e = ed[row].empty() ? Err::None : p.compile(ed[row].str());
    if (e != Err::None) {
      char msg[48];
      snprintf(msg, sizeof msg, "f%d: %s", row + 1, errText(e));
      g.text(6, SCREEN_H - 11, msg, th.err);
    } else {
      g.text(6, SCREEN_H - 11, "Enter plota  Tab opcoes  fn+Ent on/off", th.faint);
    }
    app->statusBar(g, "Funcoes");
  }
};

// ================================================================== Gráfico
class GraphView : public View {
 public:
  enum { TOP = 12, W = SCREEN_W, H = SCREEN_H - 12 };
  bool trace = false;
  int tf = 0;
  double tx = 0;
  // resultado de análise
  bool mark = false, tangent = false, shade = false;
  double mx = 0, my = 0, slope = 0, sa = 0, sb = 0;
  int sf = 0;
  char info[64] = "";
  color_t infoColor = 0;
  float ys_[W * NFUNC / 2];  // amostras para o ajuste automático de y

  const char* title() override { return "Grafico"; }

  double& xmin() { return app->st.win[0]; }
  double& xmax() { return app->st.win[1]; }
  double& ymin() { return app->st.win[2]; }
  double& ymax() { return app->st.win[3]; }
  double sxf(double x) { return (x - xmin()) / (xmax() - xmin()) * W; }
  double syf(double y) { return TOP + (ymax() - y) / (ymax() - ymin()) * H; }
  double xAt(double px) { return xmin() + (px + 0.5) * (xmax() - xmin()) / W; }
  static int clampi(double v) { return v < -4000 ? -4000 : v > 4000 ? 4000 : (int)lround(v); }

  void enter() override {
    if (!app->fActive(tf)) firstActive(0, 1);
    clearAnalysis();
  }
  void leave() override { app->requestSave(); }

  void clearAnalysis() {
    mark = tangent = shade = false;
    info[0] = 0;
  }

  bool firstActive(int from, int dir) {
    for (int k = 0; k < NFUNC; k++) {
      int i = ((from + dir * k) % NFUNC + NFUNC) % NFUNC;
      if (app->fActive(i)) {
        tf = i;
        return true;
      }
    }
    return false;
  }

  void setTraceInfo() {
    double y = app->evalF(tf, tx);
    char xs[20], ys[20];
    formatFit(tx, xs, sizeof xs, 9);
    if (isfinite(y)) formatFit(y, ys, sizeof ys, 9);
    else snprintf(ys, sizeof ys, "indef.");
    snprintf(info, sizeof info, "f%d  x=%s  y=%s", tf + 1, xs, ys);
    infoColor = th.func[tf];
  }

  void ensureVisible() {
    double w = xmax() - xmin();
    if (tx < xmin()) {
      xmin() = tx - w * 0.1;
      xmax() = xmin() + w;
    } else if (tx > xmax()) {
      xmax() = tx + w * 0.1;
      xmin() = xmax() - w;
    }
    double y = app->evalF(tf, tx), h = ymax() - ymin();
    if (isfinite(y) && (y < ymin() || y > ymax())) {
      ymin() = y - h / 2;
      ymax() = y + h / 2;
    }
  }

  void zoom(double f) {
    double cx = (xmin() + xmax()) / 2, cy = (ymin() + ymax()) / 2;
    if (trace) {
      cx = tx;
      double y = app->evalF(tf, tx);
      if (isfinite(y)) cy = y;
    }
    double hw = (xmax() - xmin()) / 2 * f, hh = (ymax() - ymin()) / 2 * f;
    if (hw < 1e-9 || hw > 1e12) return;
    xmin() = cx - hw;
    xmax() = cx + hw;
    ymin() = cy - hh;
    ymax() = cy + hh;
  }

  void autoY() {
    float* ys = ys_;
    int n = 0;
    for (int i = 0; i < NFUNC; i++) {
      if (!app->fActive(i)) continue;
      for (int px = 0; px < W; px += 2) {
        double y = app->evalF(i, xAt(px));
        if (isfinite(y) && fabs(y) < 1e15) ys[n++] = (float)y;
      }
    }
    if (n < 2) return;
    // quantis 3%..97% para ignorar assíntotas
    for (int i = 1; i < n; i++) {  // insertion sort (n <= 480)
      float v = ys[i];
      int j = i - 1;
      while (j >= 0 && ys[j] > v) {
        ys[j + 1] = ys[j];
        j--;
      }
      ys[j + 1] = v;
    }
    double lo = ys[n * 3 / 100], hi = ys[n - 1 - n * 3 / 100];
    if (hi - lo < 1e-9) {
      lo -= 1;
      hi += 1;
    }
    double pad = (hi - lo) * 0.08;
    ymin() = lo - pad;
    ymax() = hi + pad;
  }

  void resetWindow() {
    xmin() = -10;
    xmax() = 10;
    ymin() = -5.125;
    ymax() = 5.125;
  }

  // ----------------------------------------------------------- análise
  double refX() { return trace ? tx : (xmin() + xmax()) / 2; }

  void showResult(const char* label, double x, double y, int f) {
    trace = true;
    tf = f;
    tx = x;
    mark = true;
    mx = x;
    my = y;
    char xs[20], ys[20];
    formatFit(x, xs, sizeof xs, 9);
    formatFit(y, ys, sizeof ys, 9);
    snprintf(info, sizeof info, "%s x=%s y=%s", label, xs, ys);
    infoColor = th.accent;
    ensureVisible();
  }

  void findRoot(int f, int g2) {
    FnRef r = {app, f, g2};
    RealFn fn = g2 < 0 ? FnRef::one : FnRef::diff;
    const int N = 240;
    double best = NAN, bestD = 1e300, ref = refX();
    double px = xAt(-0.5), py = fn(px, &r);
    for (int i = 1; i <= N; i++) {
      double x = xmin() + i * (xmax() - xmin()) / N, y = fn(x, &r);
      if (isfinite(py) && isfinite(y) && (py == 0 || (py > 0) != (y > 0))) {
        double root = rootIn(fn, &r, px, x);
        double fr = fn(root, &r);
        double scale = (ymax() - ymin());
        if (isfinite(fr) && fabs(fr) < 1e-6 * scale) {  // descarta assíntotas
          double d = fabs(root - ref);
          if (d < bestD) {
            bestD = d;
            best = root;
          }
        }
      }
      px = x;
      py = y;
    }
    if (!isfinite(best)) {
      app->toast(g2 < 0 ? "Nenhuma raiz na janela" : "Nenhuma intersecao na janela", true);
      return;
    }
    if (fabs(best) < 1e-12 * (xmax() - xmin())) best = 0;
    showResult(g2 < 0 ? "Raiz" : "Inters.", best, app->evalF(f, best), f);
  }

  void findExtremum(int f, bool maximum) {
    FnRef r = {app, f, -1};
    const int N = 240;
    double ys[N + 1];
    for (int i = 0; i <= N; i++) ys[i] = app->evalF(f, xmin() + i * (xmax() - xmin()) / N);
    double best = NAN, bestD = 1e300, ref = refX();
    for (int i = 1; i < N; i++) {
      double a = ys[i - 1], b = ys[i], c = ys[i + 1];
      if (!isfinite(a) || !isfinite(b) || !isfinite(c)) continue;
      bool ext = maximum ? (b >= a && b >= c && (b > a || b > c)) : (b <= a && b <= c && (b < a || b < c));
      if (!ext) continue;
      double x0 = xmin() + (i - 1) * (xmax() - xmin()) / N, x1 = xmin() + (i + 1) * (xmax() - xmin()) / N;
      double x = extremumIn(FnRef::one, &r, x0, x1, maximum);
      double d = fabs(x - ref);
      if (d < bestD) {
        bestD = d;
        best = x;
      }
    }
    if (!isfinite(best)) {
      app->toast(maximum ? "Nenhum maximo na janela" : "Nenhum minimo na janela", true);
      return;
    }
    showResult(maximum ? "Max" : "Min", best, app->evalF(f, best), f);
  }

  int nextActive(int f) {
    for (int k = 1; k < NFUNC; k++) {
      int i = (f + k) % NFUNC;
      if (app->fActive(i)) return i;
    }
    return -1;
  }

  void menuPick(int tag, int idx) override {
    if (tag != 1) return;
    int f = tf;
    if (!app->fActive(f) && !firstActive(0, 1)) return;
    f = tf;
    switch (idx) {
      case 0: findRoot(f, -1); break;
      case 1: findExtremum(f, false); break;
      case 2: findExtremum(f, true); break;
      case 3: {
        int g2 = nextActive(f);
        if (g2 < 0) app->toast("Precisa de 2 funcoes ativas", true);
        else findRoot(f, g2);
        break;
      }
      case 4: {  // derivada no ponto do traço
        if (!trace) tx = refX();
        FnRef r = {app, f, -1};
        double y = app->evalF(f, tx);
        slope = derivative(FnRef::one, &r, tx);
        if (!isfinite(slope) || !isfinite(y)) {
          app->toast("Derivada indefinida aqui", true);
          return;
        }
        trace = true;
        mark = tangent = true;
        mx = tx;
        my = y;
        char xs[20], ds[20];
        formatFit(tx, xs, sizeof xs, 8);
        formatFit(slope, ds, sizeof ds, 10);
        snprintf(info, sizeof info, "dy/dx=%s em x=%s", ds, xs);
        infoColor = th.accent;
        break;
      }
      case 5: {
        char fa[24], fb[24];
        double a = trace ? tx : xmin() + (xmax() - xmin()) * 0.25;
        formatShort(a, fa, sizeof fa);
        formatShort(a + niceStep((xmax() - xmin()) / 4), fb, sizeof fb);
        app->form.begin(this, 2, "Integral de f");
        app->form.add("de", fa);
        app->form.add("ate", fb);
        app->form.show();
        break;
      }
      case 6:
        app->form.begin(this, 3, "Ir para x");
        app->form.add("x", "0");
        app->form.show();
        break;
      case 7: autoY(); break;
      case 8:
        app->form.begin(this, 1, "Janela do grafico");
        app->form.addNum("x min", xmin());
        app->form.addNum("x max", xmax());
        app->form.addNum("y min", ymin());
        app->form.addNum("y max", ymax());
        app->form.show();
        break;
      case 9: resetWindow(); clearAnalysis(); break;
      case 10: app->go(app->table); break;
      case 11: app->box.show("Ajuda - Graficos", kGraphHelp); break;
    }
  }

  void formDone(int tag) override {
    double* v = app->form.val;
    if (tag == 1) {
      if (!(v[0] < v[1]) || !(v[2] < v[3])) {
        app->toast("min precisa ser menor que max", true);
        return;
      }
      for (int i = 0; i < 4; i++) app->st.win[i] = v[i];
      clearAnalysis();
    } else if (tag == 2) {
      FnRef r = {app, tf, -1};
      double a = v[0], b = v[1];
      double s = integrate(FnRef::one, &r, a, b);
      if (!isfinite(s)) {
        app->toast("Integral nao converge", true);
        return;
      }
      clearAnalysis();
      shade = true;
      sf = tf;
      sa = a < b ? a : b;
      sb = a < b ? b : a;
      char ss[24], as[16], bs[16];
      formatFit(s, ss, sizeof ss, 11);
      formatShort(a, as, sizeof as);
      formatShort(b, bs, sizeof bs);
      snprintf(info, sizeof info, "int[%s,%s] = %s", as, bs, ss);
      infoColor = th.func[tf];
      double w = xmax() - xmin();
      if (sa < xmin() || sb > xmax()) {  // mostra a área inteira
        double m = (sb - sa) * 0.15 + 1e-9;
        xmin() = sa - m;
        xmax() = sb + m;
        double k = (xmax() - xmin()) / w, cy = (ymin() + ymax()) / 2, hh = (ymax() - ymin()) / 2 * k;
        ymin() = cy - hh;
        ymax() = cy + hh;
      }
      trace = false;
    } else if (tag == 3) {
      trace = true;
      tx = v[0];
      clearAnalysis();
      ensureVisible();
      setTraceInfo();
    }
  }

  void key(const Key& k) override {
    Nav n = arrowOf(k, false);
    double w = xmax() - xmin(), h = ymax() - ymin();
    if (trace && n != Nav::None) {
      clearAnalysis();
      double step = w / W * (k.fn ? 10 : 1);
      if (n == Nav::Left) tx -= step;
      if (n == Nav::Right) tx += step;
      if (n == Nav::Up) firstActive(tf + 1, 1);
      if (n == Nav::Down) firstActive(tf - 1, -1);
      ensureVisible();
      setTraceInfo();
      return;
    }
    switch (n) {
      case Nav::Left: xmin() -= w / 8; xmax() -= w / 8; return;
      case Nav::Right: xmin() += w / 8; xmax() += w / 8; return;
      case Nav::Up: ymin() += h / 8; ymax() += h / 8; return;
      case Nav::Down: ymin() -= h / 8; ymax() -= h / 8; return;
      default: break;
    }
    if (k.code == K_ENTER || (k.code == K_CHAR && k.ch == 't')) {
      trace = !trace;
      clearAnalysis();
      if (trace) {
        if (!(tx > xmin() && tx < xmax())) tx = (xmin() + xmax()) / 2;
        if (!app->fActive(tf)) firstActive(0, 1);
        setTraceInfo();
      }
      return;
    }
    if (k.code == K_TAB) {
      app->menu.begin(this, 1, "Analise");
      static const char* const items[] = {"Raiz",       "Minimo",      "Maximo",         "Intersecao",
                                          "Derivada dy/dx", "Integral...", "Ir para x...", "Ajustar y",
                                          "Janela...",  "Janela padrao", "Tabela",       "Ajuda"};
      for (int i = 0; i < 12; i++) app->menu.add(items[i]);
      app->openMenu();
      return;
    }
    if (k.code == K_BACK) {
      if (trace || shade || mark) {
        trace = false;
        clearAnalysis();
      } else {
        app->go(app->funcs);
      }
      return;
    }
    if (k.code != K_CHAR) return;
    switch (k.ch) {
      case '=': case '+': zoom(1 / 1.5); break;
      case '-': zoom(1.5); break;
      case 'a': autoY(); break;
      case 'r': resetWindow(); clearAnalysis(); break;
      case '1': case '2': case '3': case '4': {
        int i = k.ch - '1';
        if (app->fActive(i)) {
          tf = i;
          trace = true;
          clearAnalysis();
          if (!(tx > xmin() && tx < xmax())) tx = (xmin() + xmax()) / 2;
          setTraceInfo();
        }
        break;
      }
    }
  }

  // ----------------------------------------------------------- desenho
  void drawGrid(Gfx& g) {
    double sx = niceStep((xmax() - xmin()) / 6), sy = niceStep((ymax() - ymin()) / 4);
    long k0 = (long)ceil(xmin() / sx), k1 = (long)floor(xmax() / sx);
    for (long k = k0; k <= k1 && k - k0 < 100; k++) g.vline(clampi(sxf(k * sx)), TOP, H, th.grid);
    long j0 = (long)ceil(ymin() / sy), j1 = (long)floor(ymax() / sy);
    for (long j = j0; j <= j1 && j - j0 < 100; j++) g.hline(0, clampi(syf(j * sy)), W, th.grid);
    int ax = clampi(sxf(0)), ay = clampi(syf(0));
    bool xAxis = ay >= TOP && ay < TOP + H, yAxis = ax >= 0 && ax < W;
    if (xAxis) g.hline(0, ay, W, th.axis);
    if (yAxis) g.vline(ax, TOP, H, th.axis);
    // rótulos
    char b[16];
    int ly = xAxis ? ay + 3 : TOP + H - 10;
    if (ly > TOP + H - 10) ly = ay - 10;
    int lastEnd = -100;
    for (long k = k0; k <= k1 && k - k0 < 100; k++) {
      if (k == 0) continue;
      formatShort(k * sx, b, sizeof b);
      int px = clampi(sxf(k * sx)), w = tw(b), x = px - w / 2;
      if (x < 1 || x + w > W - 1 || x < lastEnd + 6) continue;
      if (xAxis) g.vline(px, ay - 2, 5, th.axis);
      g.text(x, ly, b, th.faint);
      lastEnd = x + w;
    }
    for (long j = j0; j <= j1 && j - j0 < 100; j++) {
      if (j == 0) continue;
      formatShort(j * sy, b, sizeof b);
      int py = clampi(syf(j * sy)), w = tw(b);
      if (py - 4 < TOP + 1 || py + 4 > TOP + H - 1) continue;
      int x = yAxis ? ax + 4 : 2;
      if (x + w > W - 2) x = ax - 3 - w;
      if (yAxis) g.hline(ax - 2, py, 5, th.axis);
      g.text(x, py - 3, b, th.faint);
    }
  }

  void drawShade(Gfx& g) {
    color_t c = mix(th.bg, th.func[sf], 105);
    int y0 = clampi(syf(0));
    y0 = y0 < TOP ? TOP : y0 > TOP + H ? TOP + H : y0;
    int p0 = clampi(ceil(sxf(sa) - 0.5)), p1 = clampi(floor(sxf(sb) - 0.5));
    for (int px = p0 < 0 ? 0 : p0; px <= p1 && px < W; px++) {
      double y = app->evalF(sf, xAt(px));
      if (!isfinite(y)) continue;
      int py = clampi(syf(y));
      py = py < TOP ? TOP : py > TOP + H ? TOP + H : py;
      if (py < y0) g.vline(px, py, y0 - py, c);
      else g.vline(px, y0, py - y0 + 1, c);
    }
    int a = clampi(sxf(sa)), b = clampi(sxf(sb));
    g.vline(a, TOP, H, mix(th.bg, th.func[sf], 140));
    g.vline(b, TOP, H, mix(th.bg, th.func[sf], 140));
  }

  void plotFn(Gfx& g, int i) {
    color_t c = th.func[i];
    bool have = false;
    double prev = 0;
    double dx = (xmax() - xmin()) / W;
    for (int px = 0; px < W; px++) {
      double x = xAt(px), y = app->evalF(i, x);
      if (!isfinite(y)) {
        have = false;
        continue;
      }
      double sy = syf(y);
      if (have) {
        bool brk = false;
        if (fabs(sy - prev) > H / 3) {
          // assíntota? confere no meio do caminho
          double mid = app->evalF(i, x - dx / 2);
          if (!isfinite(mid)) brk = true;
          else {
            double sm = syf(mid), lo = sy < prev ? sy : prev, hi = sy < prev ? prev : sy;
            brk = sm < lo - 2 || sm > hi + 2;
          }
        }
        if (!brk && !((prev < TOP && sy < TOP) || (prev > TOP + H && sy > TOP + H))) {
          int a = clampi(prev), b = clampi(sy);
          g.line(px - 1, a, px, b, c);
          g.line(px - 1, a + 1, px, b + 1, c);  // 2 px de espessura
        }
      } else if (sy >= TOP && sy < TOP + H) {
        g.fillRect(px, clampi(sy), 1, 2, c);
      }
      prev = sy;
      have = true;
    }
  }

  void render(Gfx& g) override {
    g.fillRect(0, TOP, W, H, th.bg);
    g.setClip(0, TOP, W, H);
    drawGrid(g);
    if (shade) drawShade(g);
    for (int i = NFUNC - 1; i >= 0; i--)
      if (app->fActive(i) && i != tf) plotFn(g, i);
    if (app->fActive(tf)) plotFn(g, tf);
    if (tangent) {
      double x0 = xmin(), x1 = xmax();
      g.line(0, clampi(syf(my + slope * (x0 - mx))), W, clampi(syf(my + slope * (x1 - mx))), th.text);
    }
    if (trace && app->fActive(tf)) {
      double y = app->evalF(tf, tx);
      int px = clampi(sxf(tx));
      for (int yy = TOP; yy < TOP + H; yy += 4) g.pixel(px, yy, th.dim);
      if (isfinite(y)) {
        int py = clampi(syf(y));
        for (int xx = 0; xx < W; xx += 4) g.pixel(xx, py, th.dim);
        g.drawCircle(px, py, 4, th.text);
        g.fillCircle(px, py, 2, th.func[tf]);
      }
      if (!mark && !info[0]) setTraceInfo();
    }
    if (mark && !tangent) {
      int px = clampi(sxf(mx)), py = clampi(syf(my));
      g.drawCircle(px, py, 5, th.accent);
      g.drawCircle(px, py, 6, th.accent);
    }
    g.clearClip();
    if (info[0]) {
      int y = SCREEN_H - 12;
      g.fillRect(0, y, W, 12, th.panel);
      g.hline(0, y, W, th.edge);
      textClip(g, 4, y + 2, info, (W - 8) / FONT_W, infoColor);
    }
    char ctr[40] = "";
    if (!trace && !info[0]) {
      char a[12], b[12];
      formatShort(xmin(), a, sizeof a);
      formatShort(xmax(), b, sizeof b);
      snprintf(ctr, sizeof ctr, "x:[%s, %s]", a, b);
    } else if (trace) {
      snprintf(ctr, sizeof ctr, "traco f%d", tf + 1);
    }
    app->statusBar(g, "Grafico", ctr);
  }
};

// =================================================================== Tabela
class TableView : public View {
 public:
  int sel = 0, top = 0;
  enum { ROWS = 10, RH = 11 };
  const char* title() override { return "Tabela"; }
  void leave() override { app->requestSave(); }

  void key(const Key& k) override {
    Nav n = arrowOf(k, false);
    if (n == Nav::Up) {
      if (sel > 0) sel--;
      else top--;
    }
    if (n == Nav::Down) {
      if (sel < ROWS - 1) sel++;
      else top++;
    }
    if (n == Nav::Left) top -= ROWS;
    if (n == Nav::Right) top += ROWS;
    if (k.code == K_TAB) {
      app->menu.begin(this, 1, "Tabela");
      app->menu.add("Inicio e passo...");
      app->menu.add("Editar funcoes");
      app->menu.add("Ver grafico");
      app->openMenu();
    }
    if (k.code == K_BACK) app->go(app->funcs);
  }

  void menuPick(int tag, int idx) override {
    if (tag != 1) return;
    if (idx == 0) {
      app->form.begin(this, 1, "Tabela");
      app->form.addNum("inicio", app->st.tbl[0]);
      app->form.addNum("passo", app->st.tbl[1]);
      app->form.show();
    } else if (idx == 1) {
      app->go(app->funcs);
    } else if (idx == 2) {
      for (int i = 0; i < NFUNC; i++)
        if (app->fActive(i)) {
          app->go(app->graph);
          return;
        }
      app->toast("Nenhuma funcao ativa", true);
    }
  }

  void formDone(int tag) override {
    if (tag != 1) return;
    if (!(app->form.val[1] != 0)) {
      app->toast("Passo nao pode ser 0", true);
      return;
    }
    app->st.tbl[0] = app->form.val[0];
    app->st.tbl[1] = app->form.val[1];
    top = sel = 0;
  }

  void render(Gfx& g) override {
    g.fillScreen(th.bg);
    int cols[NFUNC], nc = 0;
    for (int i = 0; i < NFUNC && nc < 3; i++)
      if (app->fActive(i)) cols[nc++] = i;
    const int x0 = 4, xw = 58;
    int cw = nc ? (SCREEN_W - x0 - xw - 4) / nc : 0;
    int hy = 14;
    g.fillRect(0, hy, SCREEN_W, 11, th.panel);
    g.text(x0 + 4, hy + 2, "x", th.dim);
    for (int c = 0; c < nc; c++) {
      char h[16];
      snprintf(h, sizeof h, "f%d", cols[c] + 1);
      g.text(x0 + xw + c * cw + 4, hy + 2, h, th.func[cols[c]]);
    }
    if (!nc) textC(g, SCREEN_W / 2, 64, "Nenhuma funcao ativa", th.dim);
    for (int r = 0; r < ROWS; r++) {
      int y = hy + 13 + r * RH;
      double x = app->st.tbl[0] + (top + r) * app->st.tbl[1];
      if (fabs(x) < 1e-12 * fabs(app->st.tbl[1])) x = 0;
      bool s = r == sel;
      if (s) g.fillRect(0, y - 1, SCREEN_W, RH, th.panel2);
      char b[24];
      formatFit(x, b, sizeof b, (xw - 6) / FONT_W);
      g.text(x0 + 4, y + 1, b, s ? th.accent : th.dim);
      for (int c = 0; c < nc; c++) {
        double v = app->evalF(cols[c], x);
        if (isfinite(v)) formatFit(v, b, sizeof b, (cw - 6) / FONT_W);
        else snprintf(b, sizeof b, "--");
        g.text(x0 + xw + c * cw + 4, y + 1, b, isfinite(v) ? th.text : th.faint);
      }
    }
    g.vline(x0 + xw, hy, SCREEN_H - hy, th.edge);
    char ctr[32], p[16];
    formatShort(app->st.tbl[1], p, sizeof p);
    snprintf(ctr, sizeof ctr, "passo %s", p);
    app->statusBar(g, "Tabela", ctr);
  }
};

View* makeFuncs() { return new FuncsView(); }
View* makeGraph() { return new GraphView(); }
View* makeTable() { return new TableView(); }

}  // namespace px
