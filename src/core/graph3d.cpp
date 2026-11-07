// Gráfico 3D: superfície z = f(x,y) com sombreamento, malha e rotação livre
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "app.h"
#include "numeric.h"

namespace px {

using namespace ui;

static const char k3DHelp[] =
    "#Funcao\n"
    "z = f(x,y). Ex.: sin(x)cos(y)\n"
    "x^2-y^2   exp(-(x^2+y^2))\n"
    "sin(sqrt(x^2+y^2))/sqrt(x^2+y^2)\n"
    "#Teclas\n"
    ", /        gira em volta\n"
    "; .        inclina\n"
    "= -        zoom\n"
    "[ ]        estica/achata z\n"
    "espaco     gira sozinho\n"
    "1 2 3 4    vistas: iso, topo,\n"
    "           frente, lado\n"
    "r          vista padrao\n"
    "Enter      editar a funcao\n"
    "Tab        dominio, estilo, grade\n"
    "`          menu principal";

enum { MAXN = 40, MAXV = (MAXN + 1) * (MAXN + 1), MAXQ = MAXN * MAXN };

// ~45 KB, alocados ao entrar no 3D e liberados ao sair (o Cardputer ADV não tem
// PSRAM; dentro do Órbita essa memória volta para os outros apps).
struct Buf3D {
  float Z[MAXV];     // z calculado
  float W[MAXV];     // z normalizado (mundo)
  float D[MAXV];     // profundidade do vértice
  float tmp[MAXV];
  float QD[MAXQ];    // profundidade do quadrilátero
  int16_t PX[MAXV], PY[MAXV];
  uint16_t QI[MAXQ]; // ordem de desenho
};
static Buf3D* sB = nullptr;
#define sZ (sB->Z)
#define sW (sB->W)
#define sD (sB->D)
#define sTmp (sB->tmp)
#define sQD (sB->QD)
#define sPX (sB->PX)
#define sPY (sB->PY)
#define sQI (sB->QI)

static color_t colormap(float t, float light) {
  static const uint8_t stops[5][3] = {{59, 76, 192}, {58, 160, 225}, {100, 205, 140}, {245, 200, 70}, {232, 84, 62}};
  if (!(t >= 0)) t = 0;
  if (t > 1) t = 1;
  float p = t * 4;
  int i = (int)p;
  if (i > 3) i = 3;
  float f = p - i;
  float r = stops[i][0] + (stops[i + 1][0] - stops[i][0]) * f;
  float g = stops[i][1] + (stops[i + 1][1] - stops[i][1]) * f;
  float b = stops[i][2] + (stops[i + 1][2] - stops[i][2]) * f;
  r *= light;
  g *= light;
  b *= light;
  return rgb((uint8_t)(r > 255 ? 255 : r), (uint8_t)(g > 255 ? 255 : g), (uint8_t)(b > 255 ? 255 : b));
}

class Graph3DView : public View {
 public:
  float yaw = -0.62f, pitch = 0.52f, zoomf = 1.0f, zs = 1.0f;
  bool autoRot = false;
  uint32_t lastT = 0;
  bool valid = false;
  int N = 0;
  float zlo = -1, zhi = 1;
  char why[40] = "";

  const char* title() override { return "3D"; }
  bool animating() override { return autoRot; }
  ~Graph3DView() override { release(); }
  void release() {
    free(sB);
    sB = nullptr;
    valid = false;
  }
  void enter() override {
    if (!sB) sB = (Buf3D*)malloc(sizeof(Buf3D));
    rebuild();
    lastT = app->now();
  }
  void leave() override {
    app->requestSave();
    release();
  }

  void rebuild() {
    valid = false;
    if (!sB) {
      snprintf(why, sizeof why, "Sem memoria para o 3D");
      return;
    }
    N = app->st.set.res3d;
    if (N < 8) N = 8;
    if (N > MAXN) N = MAXN;
    if (!app->zprog.ok()) {
      snprintf(why, sizeof why, "%s", app->st.z[0] ? errText(app->zprog.error()) : "Sem funcao");
      return;
    }
    const double* r = app->st.r3;
    double ex = 1e-7 * (r[1] - r[0]), ey = 1e-7 * (r[3] - r[2]);
    int n = 0;
    for (int j = 0; j <= N; j++) {
      double y = r[2] + (r[3] - r[2]) * j / N;
      for (int i = 0; i <= N; i++) {
        double x = r[0] + (r[1] - r[0]) * i / N;
        double z = app->evalZ(x, y);
        if (!isfinite(z)) z = app->evalZ(x + ex, y + ey);  // singularidade removível (sinc)
        float zf = (isfinite(z) && fabs(z) < 1e30) ? (float)z : NAN;
        sZ[j * (N + 1) + i] = zf;
        if (!isnan(zf)) sTmp[n++] = zf;
      }
    }
    if (n == 0) {
      snprintf(why, sizeof why, "Indefinida no dominio");
      return;
    }
    std::sort(sTmp, sTmp + n);
    float mn = sTmp[0], mx = sTmp[n - 1];
    float q1 = sTmp[n / 100], q9 = sTmp[n - 1 - n / 100];
    if (q9 > q1 && (mx - mn) > 6 * (q9 - q1)) {  // picos (ex.: 1/(x^2+y^2)): corta
      zlo = q1 - 0.15f * (q9 - q1);
      zhi = q9 + 0.15f * (q9 - q1);
    } else {
      zlo = mn;
      zhi = mx;
    }
    if (zhi - zlo < 1e-9f * (fabsf(zhi) + 1)) {
      zlo -= 1;
      zhi += 1;
    }
    valid = true;
  }

  void setView(int v) {
    static const float views[4][2] = {{-0.62f, 0.52f}, {0.0f, 1.5707f}, {0.0f, 0.0f}, {-1.5708f, 0.0f}};
    yaw = views[v][0];
    pitch = views[v][1];
    if (v == 0) {
      zoomf = 1;
      zs = 1;
    }
  }

  void editFunction() {
    app->form.begin(this, 1, "Superficie");
    app->form.add("z =", app->st.z, Form::EXPR);
    app->form.ed[0].maxLen = FN_LEN - 1;
    app->form.show();
  }

  void formDone(int tag) override {
    if (tag == 1) {
      snprintf(app->st.z, FN_LEN, "%s", app->form.ed[0].str());
      app->compileZ();
      app->requestSave();
      rebuild();
    } else if (tag == 2) {
      double* v = app->form.val;
      if (!(v[0] < v[1]) || !(v[2] < v[3])) {
        app->toast("min precisa ser menor que max", true);
        return;
      }
      for (int i = 0; i < 4; i++) app->st.r3[i] = v[i];
      app->requestSave();
      rebuild();
    }
  }

  void menuPick(int tag, int idx) override {
    if (tag != 1) return;
    Settings& s = app->st.set;
    switch (idx) {
      case 0: editFunction(); break;
      case 1:
        app->form.begin(this, 2, "Dominio");
        app->form.addNum("x min", app->st.r3[0]);
        app->form.addNum("x max", app->st.r3[1]);
        app->form.addNum("y min", app->st.r3[2]);
        app->form.addNum("y max", app->st.r3[3]);
        app->form.show();
        break;
      case 2: s.style3d = (s.style3d + 1) % 3; app->requestSave(); break;
      case 3: {
        s.res3d = s.res3d >= 40 ? 16 : s.res3d + 8;
        app->requestSave();
        rebuild();
        break;
      }
      case 4: s.box3d = !s.box3d; app->requestSave(); break;
      case 5: autoRot = !autoRot; lastT = app->now(); break;
      case 6: setView(0); break;
      case 7: app->box.show("Ajuda - 3D", k3DHelp); break;
    }
  }

  void key(const Key& k) override {
    Nav n = arrowOf(k, false);
    float step = k.fn ? 0.04f : 0.14f;
    if (n == Nav::Left) yaw += step;
    if (n == Nav::Right) yaw -= step;
    if (n == Nav::Up) pitch += step * 0.8f;
    if (n == Nav::Down) pitch -= step * 0.8f;
    if (pitch > 1.5707f) pitch = 1.5707f;
    if (pitch < -1.5707f) pitch = -1.5707f;
    if (n != Nav::None) return;
    if (k.code == K_ENTER) {
      editFunction();
      return;
    }
    if (k.code == K_BACK) {
      app->go(app->home);
      return;
    }
    if (k.code == K_TAB) {
      static const char* const st[] = {"solido + malha", "solido", "malha"};
      const Settings& s = app->st.set;
      char b[40];
      app->menu.begin(this, 1, "Grafico 3D");
      app->menu.add("Editar funcao");
      app->menu.add("Dominio x, y...");
      snprintf(b, sizeof b, "Estilo: %s", st[s.style3d % 3]);
      app->menu.add(b);
      snprintf(b, sizeof b, "Grade: %dx%d", s.res3d, s.res3d);
      app->menu.add(b);
      app->menu.add(s.box3d ? "Caixa e eixos: sim" : "Caixa e eixos: nao");
      app->menu.add(autoRot ? "Girar sozinho: sim" : "Girar sozinho: nao");
      app->menu.add("Vista padrao");
      app->menu.add("Ajuda");
      app->openMenu();
      return;
    }
    if (k.code != K_CHAR) return;
    switch (k.ch) {
      case '=': case '+': zoomf = zoomf * 1.2f > 6 ? 6 : zoomf * 1.2f; break;
      case '-': zoomf = zoomf / 1.2f < 0.3f ? 0.3f : zoomf / 1.2f; break;
      case ']': zs = zs * 1.25f > 5 ? 5 : zs * 1.25f; break;
      case '[': zs = zs / 1.25f < 0.15f ? 0.15f : zs / 1.25f; break;
      case ' ': autoRot = !autoRot; lastT = app->now(); break;
      case 'e': editFunction(); break;
      case 'r': setView(0); break;
      case '1': case '2': case '3': case '4': setView(k.ch - '1'); break;
    }
  }

  // projeção
  float cy_, sy_, cp_, sp_, sc_, ox_, oy_;
  enum { CAM = 4 };
  void project(float u, float v, float w, int16_t& X, int16_t& Y, float& d) {
    float x1 = u * cy_ - v * sy_;
    float y1 = u * sy_ + v * cy_;
    float up = w * cp_ + y1 * sp_;
    d = y1 * cp_ - w * sp_;
    float f = CAM / (CAM + d);
    float fx = ox_ + sc_ * f * x1, fy = oy_ - sc_ * f * up;
    X = (int16_t)(fx < -3000 ? -3000 : fx > 3000 ? 3000 : fx);
    Y = (int16_t)(fy < -3000 ? -3000 : fy > 3000 ? 3000 : fy);
  }

  void render(Gfx& g) override {
    if (autoRot) {
      uint32_t t = app->now(), dt = t - lastT;
      if (dt > 100) dt = 100;
      yaw -= dt * 0.0007f;
      lastT = t;
    }
    g.fillScreen(th.bg);
    char ctr[FN_LEN + 4];
    snprintf(ctr, sizeof ctr, "z=%s", app->st.z);
    if (!valid) {
      textC(g, SCREEN_W / 2, 56, why, th.err);
      textC(g, SCREEN_W / 2, 74, "Enter: editar a funcao", th.dim);
      app->statusBar(g, "3D", ctr);
      return;
    }
    cy_ = cosf(yaw);
    sy_ = sinf(yaw);
    cp_ = cosf(pitch);
    sp_ = sinf(pitch);
    sc_ = 50.0f * zoomf;
    ox_ = SCREEN_W / 2;
    oy_ = 12 + (SCREEN_H - 12) / 2 + 5;
    const int S = N + 1;
    const float zh = 0.62f * zs;
    const float zmid = 0.5f * (zlo + zhi), zhalf = 0.5f * (zhi - zlo);
    for (int j = 0; j <= N; j++) {
      float v = -1 + 2.0f * j / N;
      for (int i = 0; i <= N; i++) {
        int k = j * S + i;
        float z = sZ[k];
        if (isnan(z)) {
          sW[k] = NAN;
          continue;
        }
        if (z < zlo) z = zlo;
        if (z > zhi) z = zhi;
        float w = (z - zmid) / zhalf * zh;
        sW[k] = w;
        project(-1 + 2.0f * i / N, v, w, sPX[k], sPY[k], sD[k]);
      }
    }
    if (app->st.set.box3d) drawBackBox(g, zh);

    // ordena os quadriláteros do fundo para a frente (algoritmo do pintor)
    int nq = 0;
    for (int j = 0; j < N; j++)
      for (int i = 0; i < N; i++) {
        int k = j * S + i;
        if (isnan(sW[k]) || isnan(sW[k + 1]) || isnan(sW[k + S]) || isnan(sW[k + S + 1])) continue;
        sQD[j * N + i] = sD[k] + sD[k + 1] + sD[k + S] + sD[k + S + 1];
        sQI[nq++] = (uint16_t)(j * N + i);
      }
    std::sort(sQI, sQI + nq, [](uint16_t a, uint16_t b) { return sQD[a] > sQD[b]; });

    // luz no espaço da câmera: de cima, à esquerda, vindo da frente
    const float Lx = -0.45f, Lu = 0.78f, Ld = -0.43f;
    const float du = 2.0f / N;
    const int style = app->st.set.style3d % 3;
    for (int q = 0; q < nq; q++) {
      int qi = sQI[q], i = qi % N, j = qi / N;
      int k00 = j * S + i, k10 = k00 + 1, k01 = k00 + S, k11 = k01 + 1;
      float w00 = sW[k00], w10 = sW[k10], w01 = sW[k01], w11 = sW[k11];
      float nu = -((w10 - w00) + (w11 - w01)) / (2 * du);
      float nv = -((w01 - w00) + (w11 - w10)) / (2 * du);
      float nw = 1;
      float inv = 1.0f / sqrtf(nu * nu + nv * nv + nw * nw);
      nu *= inv;
      nv *= inv;
      nw *= inv;
      float nx1 = nu * cy_ - nv * sy_, ny1 = nu * sy_ + nv * cy_;
      float nup = nw * cp_ + ny1 * sp_, nd = ny1 * cp_ - nw * sp_;
      float dot = fabsf(nx1 * Lx + nup * Lu + nd * Ld);
      float h = ((w00 + w10 + w01 + w11) * 0.25f / zh + 1) * 0.5f;
      color_t fill, edge;
      if (style == 2) {
        fill = th.bg;
        edge = colormap(h, 0.95f);
      } else {
        fill = colormap(h, 0.30f + 0.78f * dot);
        edge = style == 0 ? mix(fill, rgb(0, 0, 0), 120) : fill;
      }
      g.fillTriangle(sPX[k00], sPY[k00], sPX[k10], sPY[k10], sPX[k11], sPY[k11], fill);
      g.fillTriangle(sPX[k00], sPY[k00], sPX[k11], sPY[k11], sPX[k01], sPY[k01], fill);
      g.line(sPX[k00], sPY[k00], sPX[k10], sPY[k10], edge);
      g.line(sPX[k10], sPY[k10], sPX[k11], sPY[k11], edge);
      g.line(sPX[k11], sPY[k11], sPX[k01], sPY[k01], edge);
      g.line(sPX[k01], sPY[k01], sPX[k00], sPY[k00], edge);
    }
    if (app->st.set.box3d) drawLabels(g, zh);

    // legenda de cores e faixa de z
    char a[16], b[16], line[40];
    formatShort(zlo, a, sizeof a);
    formatShort(zhi, b, sizeof b);
    int lx = SCREEN_W - 8, ly0 = 18, lh = 50;
    for (int y = 0; y < lh; y++) g.hline(lx, ly0 + y, 4, colormap(1 - (float)y / (lh - 1), 1));
    textR(g, lx - 3, ly0, b, th.dim);
    textR(g, lx - 3, ly0 + lh - 8, a, th.dim);
    if (autoRot) {
      snprintf(line, sizeof line, "girando (espaco para)");
    } else {
      formatShort(app->st.r3[0], a, sizeof a);
      formatShort(app->st.r3[1], b, sizeof b);
      snprintf(line, sizeof line, "x:[%s,%s]", a, b);
    }
    g.fillRoundRect(1, SCREEN_H - 12, tw(line) + 6, 11, 2, th.bg);
    g.text(4, SCREEN_H - 10, line, th.dim);
    app->statusBar(g, "3D", ctr);
  }

  void drawBackBox(Gfx& g, float zh) {
    // cantos do chão e do teto
    static const float cu[4] = {-1, 1, 1, -1}, cv[4] = {-1, -1, 1, 1};
    int16_t fx[4], fy[4], tx[4], ty[4];
    float fd[4], td;
    for (int c = 0; c < 4; c++) {
      project(cu[c], cv[c], -zh, fx[c], fy[c], fd[c]);
      project(cu[c], cv[c], zh, tx[c], ty[c], td);
    }
    int back = 0;
    for (int c = 1; c < 4; c++)
      if (fd[c] > fd[back]) back = c;
    // grade no chão
    for (int t = 1; t < 4; t++) {
      float s = -1 + t * 0.5f;
      int16_t ax, ay, bx, by;
      float d;
      project(s, -1, -zh, ax, ay, d);
      project(s, 1, -zh, bx, by, d);
      g.line(ax, ay, bx, by, th.grid);
      project(-1, s, -zh, ax, ay, d);
      project(1, s, -zh, bx, by, d);
      g.line(ax, ay, bx, by, th.grid);
    }
    for (int c = 0; c < 4; c++) g.line(fx[c], fy[c], fx[(c + 1) % 4], fy[(c + 1) % 4], th.faint);
    // paredes do fundo: aresta vertical do canto de trás e das duas vizinhas
    int nb[3] = {back, (back + 1) % 4, (back + 3) % 4};
    for (int m = 0; m < 3; m++) g.line(fx[nb[m]], fy[nb[m]], tx[nb[m]], ty[nb[m]], th.faint);
    g.line(tx[back], ty[back], tx[nb[1]], ty[nb[1]], th.grid);
    g.line(tx[back], ty[back], tx[nb[2]], ty[nb[2]], th.grid);
  }

  void drawLabels(Gfx& g, float zh) {
    int16_t X, Y;
    float d1, d2;
    // eixo x: aresta do chão (v = ±1) mais próxima da câmera
    int16_t a1x, a1y, a2x, a2y;
    project(0, -1, -zh, a1x, a1y, d1);
    project(0, 1, -zh, a2x, a2y, d2);
    float vside = d1 < d2 ? -1.25f : 1.25f;
    project(0, vside, -zh, X, Y, d1);
    g.text(X - 2, Y - 3, "x", th.dim);
    project(-1, 0, -zh, a1x, a1y, d1);
    project(1, 0, -zh, a2x, a2y, d2);
    float uside = d1 < d2 ? -1.25f : 1.25f;
    project(uside, 0, -zh, X, Y, d1);
    g.text(X - 2, Y - 3, "y", th.dim);
    // z: no canto de trás
    static const float cu[4] = {-1, 1, 1, -1}, cv[4] = {-1, -1, 1, 1};
    int back = 0;
    float bd = -1e9f;
    for (int c = 0; c < 4; c++) {
      float d;
      project(cu[c], cv[c], -zh, X, Y, d);
      if (d > bd) {
        bd = d;
        back = c;
      }
    }
    project(cu[back], cv[back], zh + 0.15f, X, Y, d1);
    g.text(X - 2, Y - 4, "z", th.dim);
  }
};

View* makeGraph3D() { return new Graph3DView(); }

}  // namespace px
