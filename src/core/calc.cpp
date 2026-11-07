// Modo Cálculo: histórico estilo console, resultado exato (fração, pi, raiz)
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "numeric.h"

namespace px {

using namespace ui;

static const char kCalcHelp[] =
    "#Digitar\n"
    "2x+1  3(4+5)  2pi  sin x  5!  |x-1|\n"
    "Potencia: ^   Notacao: 1.5e-3\n"
    "Guardar variavel: a=5  (a..z)\n"
    "Comecar com + * / ^ usa ans\n"
    "#Teclas\n"
    "Enter        calcula\n"
    "fn ; .       percorre o historico\n"
    "fn , /       move o cursor\n"
    "ctrl , /     inicio / fim\n"
    "Del          apaga (fn+Del: a frente)\n"
    "ctrl+Del     limpa a linha\n"
    "Tab          funcoes e opcoes\n"
    "`            limpa / volta ao menu\n"
    "#No historico\n"
    "Enter        reutiliza a expressao\n"
    "Tab          insere o resultado\n"
    "#Especiais (variavel x)\n"
    "int(x^2,0,3)    integral\n"
    "der(sin(x),0)   derivada\n"
    "sum(1/x^2,1,99) somatorio\n"
    "solve(x^2-2,1)  raiz perto de 1\n"
    "f1(2)           funcao do grafico";

class CalcView : public View {
 public:
  enum { HMAX = 24 };
  struct Entry {
    char expr[LineEdit::CAP];
    char res[40];
    char alt[32];
    bool err;
  };
  Entry hist[HMAX];
  int hn = 0;
  int sel = -1;  // -1: editando; 0 = entrada mais recente
  LineEdit in;
  char varsText[700];

  const char* title() override { return "Calculo"; }
  bool hasCursor() override { return sel < 0; }
  void leave() override { app->requestSave(); }

  void push(const Entry& e) {
    if (hn == HMAX) {
      memmove(hist, hist + 1, sizeof(Entry) * (HMAX - 1));
      hn--;
    }
    hist[hn++] = e;
  }

  void evaluate() {
    if (in.empty()) return;
    Program p;
    Err e = p.compile(in.str());
    if (e != Err::None) {
      in.cur = p.errorPos() <= in.len ? p.errorPos() : in.len;
      app->toast(errText(e), true);
      return;
    }
    double v = p.eval(app->ctx);
    Entry en;
    snprintf(en.expr, sizeof en.expr, "%s", in.str());
    en.alt[0] = 0;
    en.err = !isfinite(v);
    if (en.err) {
      snprintf(en.res, sizeof en.res, isnan(v) ? "Erro matematico" : (v > 0 ? "Infinito" : "-Infinito"));
    } else {
      formatNumber(v, en.res, sizeof en.res, 10, app->st.set.fix);
      if (!exactForm(v, en.alt, sizeof en.alt)) en.alt[0] = 0;
      app->ctx.vars[VAR_ANS] = v;
      if (p.assignVar() >= 0) app->ctx.vars[p.assignVar()] = v;
      app->requestSave();
    }
    push(en);
    in.clear();
  }

  void openOptions() {
    app->menu.begin(this, 1, "Calculo");
    app->menu.add("Catalogo de funcoes");
    app->menu.add("Variaveis guardadas");
    app->menu.add(app->st.set.degrees ? "Angulo: graus -> radianos" : "Angulo: radianos -> graus");
    app->menu.add("Limpar historico");
    app->menu.add("Ajuda");
    app->openMenu();
  }

  void showVars() {
    int n = 0;
    char* p = varsText;
    size_t left = sizeof varsText;
    char num[40];
    formatNumber(app->ctx.vars[VAR_ANS], num, sizeof num);
    int w = snprintf(p, left, "ans = %s", num);
    p += w;
    left -= w;
    for (int i = 0; i < 26 && left > 50; i++) {
      if (i == 'e' - 'a' || app->ctx.vars[i] == 0) continue;
      formatNumber(app->ctx.vars[i], num, sizeof num);
      w = snprintf(p, left, "\n%c = %s", 'a' + i, num);
      p += w;
      left -= w;
      n++;
    }
    if (!n) snprintf(p, left, "\n\nNenhuma variavel. Ex.: a=5");
    app->box.show("Variaveis", varsText);
  }

  void menuPick(int tag, int idx) override {
    if (tag == 1) {
      switch (idx) {
        case 0:
          app->menu.begin(this, 2, "Funcoes");
          for (int i = 0; i < kCatalogCount; i++) {
            char line[48];
            snprintf(line, sizeof line, "%-7s %s", kCatalog[i].insert, kCatalog[i].help);
            app->menu.add(line);
          }
          app->openMenu();
          break;
        case 1: showVars(); break;
        case 2:
          app->st.set.degrees = !app->st.set.degrees;
          app->applySettings();
          app->requestSave();
          app->toast(app->st.set.degrees ? "Angulo em graus" : "Angulo em radianos");
          break;
        case 3: hn = 0; sel = -1; break;
        case 4: app->box.show("Ajuda - Calculo", kCalcHelp); break;
      }
    } else if (tag == 2 && idx >= 0 && idx < kCatalogCount) {
      in.insert(kCatalog[idx].insert);
    }
  }

  void key(const Key& k) override {
    Nav n = arrowOf(k, true);
    if (n == Nav::Up) {
      if (sel < hn - 1) sel++;
      return;
    }
    if (n == Nav::Down) {
      if (sel >= 0) sel--;
      return;
    }
    if (sel >= 0) {
      const Entry& e = hist[hn - 1 - sel];
      if (k.code == K_ENTER) {
        in.set(e.expr);
        sel = -1;
        return;
      }
      if (k.code == K_TAB) {
        if (!e.err) in.insert(e.res);
        sel = -1;
        return;
      }
      if (k.code == K_BACK) {
        sel = -1;
        return;
      }
      sel = -1;  // qualquer outra tecla volta a editar
    }
    switch (k.code) {
      case K_ENTER: evaluate(); return;
      case K_TAB: openOptions(); return;
      case K_BACK:
        if (!in.empty()) in.clear();
        else app->go(app->home);
        return;
      default: break;
    }
    // operador no começo usa o último resultado: "+3" -> "ans+3"
    if (k.code == K_CHAR && !k.fn && !k.ctrl && in.empty() && hn > 0 && strchr("+*/^!", k.ch)) in.insert("ans");
    in.key(k);
  }

  void render(Gfx& g) override {
    g.fillScreen(th.bg);
    // entrada
    const int iy = SCREEN_H - 24;
    g.fillRoundRect(3, iy, SCREEN_W - 6, 22, 4, th.panel2);
    g.drawRoundRect(3, iy, SCREEN_W - 6, 22, 4, sel < 0 ? th.accent : th.edge);
    in.draw(g, 10, iy + 3, SCREEN_W - 22, 2, th.text, sel < 0, app->blink());
    // histórico, de baixo para cima
    int y = iy - 3;
    if (hn == 0) {
      textC(g, SCREEN_W / 2, 36, "Digite uma expressao", th.dim);
      textC(g, SCREEN_W / 2, 50, "e aperte Enter", th.dim);
      textC(g, SCREEN_W / 2, 72, "Tab: funcoes e ajuda", th.faint);
      textC(g, SCREEN_W / 2, 84, "`: menu principal", th.faint);
    }
    g.setClip(0, 12, SCREEN_W, iy - 14);
    for (int i = hn - 1; i >= 0 && y > 12; i--) {
      const Entry& e = hist[i];
      bool s = (hn - 1 - i) == sel;
      int rw = tw(e.res, 2);
      int rsz = rw <= SCREEN_W - 16 ? 2 : 1;
      int eh = rsz == 2 ? 29 : 21;
      int top = y - eh;
      if (s) g.fillRoundRect(3, top, SCREEN_W - 6, eh, 3, th.panel2);
      // linha 1: expressão e forma exata
      int altW = e.alt[0] ? tw(e.alt) + 12 : 0;
      int maxc = (SCREEN_W - 16 - altW) / FONT_W;
      textClip(g, 8, top + 3, e.expr, maxc, s ? th.text : th.dim);
      if (e.alt[0]) {
        char buf[40];
        snprintf(buf, sizeof buf, "= %s", e.alt);
        textR(g, SCREEN_W - 8, top + 3, buf, th.accent);
      }
      // linha 2: resultado
      color_t rc = e.err ? th.err : th.text;
      textR(g, SCREEN_W - 8, top + (rsz == 2 ? 12 : 12), e.res, rc, rsz);
      y = top - 2;
      if (!s && i > 0) g.hline(8, y + 1, SCREEN_W - 16, th.grid);
    }
    g.clearClip();
    char hint[24] = "";
    if (sel >= 0) snprintf(hint, sizeof hint, "historico %d/%d", sel + 1, hn);
    app->statusBar(g, "Calculo", hint);
  }
};

View* makeCalc() { return new CalcView(); }

}  // namespace px
