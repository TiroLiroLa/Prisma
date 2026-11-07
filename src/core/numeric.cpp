#include "numeric.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace px {

// ------------------------------------------------------------ integral (GK15)
static const double XGK[8] = {0.991455371120812639206854697526329, 0.949107912342758524526189684047851,
                              0.864864423359769072789712788640926, 0.741531185599394439863864773280788,
                              0.586087235467691130294144845693013, 0.405845151377397166906606412076961,
                              0.207784955007898467600689403773245, 0.0};
static const double WGK[8] = {0.022935322010529224963732008058970, 0.063092092629978553290700663189204,
                              0.104790010322250183839876322541518, 0.140653259715525918745189590510238,
                              0.169004726639267902826583426598550, 0.190350578064785409913256402421014,
                              0.204432940075298892414161999234649, 0.209482141084727828012999174891714};
static const double WG[4] = {0.129484966168869693270611432679082, 0.279705391489276667901467771423780,
                             0.381830050505118944950369775488975, 0.417959183673469387755102040816327};

struct Quad {
  RealFn f;
  void* u;
  int evals;
};

static double gk15(Quad& q, double a, double b, double& err) {
  double c = 0.5 * (a + b), h = 0.5 * (b - a);
  double fc = q.f(c, q.u);
  double rk = fc * WGK[7], rg = fc * WG[3];
  for (int j = 0; j < 7; j++) {
    double dx = h * XGK[j];
    double s = q.f(c - dx, q.u) + q.f(c + dx, q.u);
    rk += WGK[j] * s;
    if (j & 1) rg += WG[j / 2] * s;
  }
  q.evals += 15;
  err = fabs((rk - rg) * h);
  return rk * h;
}

static double adapt(Quad& q, double a, double b, double whole, double err, double tol, int depth) {
  if (!isfinite(whole)) return whole;
  if (err <= tol || depth <= 0 || q.evals > 30000) return whole;
  double m = 0.5 * (a + b), e1, e2;
  double r1 = gk15(q, a, m, e1), r2 = gk15(q, m, b, e2);
  return adapt(q, a, m, r1, e1, tol * 0.5, depth - 1) + adapt(q, m, b, r2, e2, tol * 0.5, depth - 1);
}

// Troca de variável x = a + (b-a)(3s^2 - 2s^3): a derivada some nas pontas e
// amansa singularidades como 1/sqrt(x) em 0, sem atrapalhar funções suaves.
struct Smooth {
  RealFn f;
  void* u;
  double a, w;
  static double call(double s, void* p) {
    Smooth* m = (Smooth*)p;
    double x = m->a + m->w * s * s * (3 - 2 * s);
    return m->f(x, m->u) * m->w * 6 * s * (1 - s);
  }
};

double integrate(RealFn f, void* u, double a, double b) {
  if (!isfinite(a) || !isfinite(b)) return NAN;
  if (a == b) return 0;
  Smooth sm = {f, u, a, b - a};
  Quad q = {Smooth::call, &sm, 0};
  a = 0;
  b = 1;
  double err;
  double whole = gk15(q, a, b, err);
  double tol = fmax(1e-12, 1e-11 * fabs(whole));
  return adapt(q, a, b, whole, err, tol, 22);
}

// ------------------------------------------------------------ derivada
double derivative(RealFn f, void* u, double x) {
  double h = 1e-3 * fmax(1.0, fabs(x));
  double fp = f(x + h, u), fm = f(x - h, u);
  double d1 = (fp - fm) / (2 * h);
  double d2 = (f(x + h / 2, u) - f(x - h / 2, u)) / h;
  double d3 = (f(x + h / 4, u) - f(x - h / 4, u)) / (h / 2);
  double r1 = (4 * d2 - d1) / 3, r2 = (4 * d3 - d2) / 3;
  double r = (16 * r2 - r1) / 15;
  // abaixo do ruído de arredondamento: derivada nula
  double noise = 8e-16 * fmax(fabs(fp), fabs(fm)) / h;
  return fabs(r) <= noise ? 0.0 : r;
}

// ------------------------------------------------------------ raízes
double rootIn(RealFn f, void* u, double a, double b) {
  double fa = f(a, u), fb = f(b, u);
  if (fa == 0) return a;
  if (fb == 0) return b;
  if (!isfinite(fa) || !isfinite(fb) || (fa > 0) == (fb > 0)) return NAN;
  for (int i = 0; i < 200; i++) {
    double m = 0.5 * (a + b);
    if (m == a || m == b) return m;
    double fm = f(m, u);
    if (fm == 0) return m;
    if (!isfinite(fm)) return NAN;
    if ((fm > 0) == (fa > 0)) {
      a = m;
      fa = fm;
    } else {
      b = m;
    }
  }
  return 0.5 * (a + b);
}

static bool isRootAt(RealFn f, void* u, double x, double fx) {
  if (fabs(fx) < 1e-9) return true;
  double e = 1e-7 * fmax(1.0, fabs(x));
  double l = f(x - e, u), r = f(x + e, u);
  return isfinite(l) && isfinite(r) && (l > 0) != (r > 0);
}

double solveNear(RealFn f, void* u, double guess) {
  if (!isfinite(guess)) return NAN;
  double x = guess, fx = f(x, u);
  for (int i = 0; i < 80 && isfinite(fx); i++) {
    if (fx == 0) return x;
    double d = derivative(f, u, x);
    if (!isfinite(d) || d == 0) break;
    double dx = fx / d, xn = x - dx, fn = f(xn, u);
    for (int k = 0; k < 12 && !(fabs(fn) < fabs(fx)); k++) {  // amortecimento
      dx *= 0.5;
      xn = x - dx;
      fn = f(xn, u);
    }
    x = xn;
    fx = fn;
    if (fabs(dx) <= 1e-14 * fmax(1.0, fabs(x))) {
      if (isRootAt(f, u, x, fx)) return x;
      break;
    }
  }
  if (isfinite(fx) && isRootAt(f, u, x, fx)) return x;
  // reserva: anda para os dois lados procurando troca de sinal
  double step = 0.01 * fmax(1.0, fabs(guess));
  double pl = guess, pr = guess, fpl = f(guess, u), fpr = fpl;
  for (int k = 0; k < 90; k++) {
    double l = guess - step, r = guess + step;
    double fl = f(l, u), fr = f(r, u);
    if (isfinite(fr) && isfinite(fpr) && (fr > 0) != (fpr > 0)) return rootIn(f, u, pr, r);
    if (isfinite(fl) && isfinite(fpl) && (fl > 0) != (fpl > 0)) return rootIn(f, u, l, pl);
    pl = l;
    fpl = fl;
    pr = r;
    fpr = fr;
    step *= 1.35;
  }
  return NAN;
}

double extremumIn(RealFn f, void* u, double a, double b, bool maximum) {
  const double g = 0.6180339887498949;
  double s = maximum ? -1 : 1;
  double c = b - g * (b - a), d = a + g * (b - a);
  double fc = s * f(c, u), fd = s * f(d, u);
  if (!isfinite(fc)) fc = INFINITY;
  if (!isfinite(fd)) fd = INFINITY;
  for (int i = 0; i < 120 && fabs(b - a) > 1e-13 * fmax(1.0, fabs(c)); i++) {
    if (fc < fd) {
      b = d;
      d = c;
      fd = fc;
      c = b - g * (b - a);
      fc = s * f(c, u);
      if (!isfinite(fc)) fc = INFINITY;
    } else {
      a = c;
      c = d;
      fc = fd;
      d = a + g * (b - a);
      fd = s * f(d, u);
      if (!isfinite(fd)) fd = INFINITY;
    }
  }
  return 0.5 * (a + b);
}

// ------------------------------------------------------------ combinatória
static bool isInt(double v) { return isfinite(v) && v == floor(v); }

double factorial(double n) {
  if (isInt(n)) {
    if (n < 0) return NAN;
    if (n > 170) return INFINITY;
    double r = 1;
    for (int i = 2; i <= (int)n; i++) r *= i;
    return r;
  }
  return tgamma(n + 1);
}

double nCr(double n, double r) {
  if (!isInt(n) || !isInt(r) || n < 0 || r < 0) return NAN;
  if (r > n) return 0;
  if (r > n - r) r = n - r;
  if (r > 2000) return round(exp(lgamma(n + 1) - lgamma(r + 1) - lgamma(n - r + 1)));
  double v = 1;
  for (int i = 1; i <= (int)r; i++) v = v * (n - r + i) / i;
  return round(v);
}

double nPr(double n, double r) {
  if (!isInt(n) || !isInt(r) || n < 0 || r < 0) return NAN;
  if (r > n) return 0;
  double v = 1;
  for (int i = 0; i < (int)r && isfinite(v); i++) v *= (n - i);
  return v;
}

double gcd(double a, double b) {
  if (!isInt(a) || !isInt(b)) return NAN;
  a = fabs(a);
  b = fabs(b);
  while (b > 0) {
    double t = fmod(a, b);
    a = b;
    b = t;
  }
  return a;
}

double lcm(double a, double b) {
  double g = gcd(a, b);
  if (!isfinite(g)) return NAN;
  if (g == 0) return 0;
  return fabs(a / g * b);
}

// ------------------------------------------------------------ formatação
// "1.2340000e+05" -> "1.234e5"
static void tidySci(char* s) {
  char* e = strchr(s, 'e');
  if (!e) return;
  char mant[40], ex[16];
  size_t ml = (size_t)(e - s);
  if (ml >= sizeof mant) return;
  memcpy(mant, s, ml);
  mant[ml] = 0;
  if (strchr(mant, '.')) {
    char* t = mant + strlen(mant) - 1;
    while (*t == '0') *t-- = 0;
    if (*t == '.') *t = 0;
  }
  int expo = atoi(e + 1);
  snprintf(ex, sizeof ex, "e%d", expo);
  strcpy(s, mant);
  strcat(s, ex);
}

void formatNumber(double v, char* out, size_t n, int sig, int fix) {
  if (isnan(v)) {
    snprintf(out, n, "Erro");
    return;
  }
  if (isinf(v)) {
    snprintf(out, n, v > 0 ? "inf" : "-inf");
    return;
  }
  if (v == 0) {
    snprintf(out, n, fix > 0 ? "%.*f" : "0", fix, 0.0);
    return;
  }
  char buf[48];
  double a = fabs(v);
  if (fix >= 0 && a < 1e10 && a >= 0.5 * pow(10.0, -fix)) {
    snprintf(buf, sizeof buf, "%.*f", fix, v);
  } else if (a >= 1e10 || a < 1e-4) {
    snprintf(buf, sizeof buf, "%.*e", (fix >= 0 ? fix : sig - 1), v);
    tidySci(buf);
  } else {
    snprintf(buf, sizeof buf, "%.*g", sig, v);
    tidySci(buf);
  }
  snprintf(out, n, "%s", buf);
}

void formatShort(double v, char* out, size_t n) {
  if (fabs(v) < 1e-12) v = 0;
  char buf[32];
  snprintf(buf, sizeof buf, "%.4g", v);
  tidySci(buf);
  snprintf(out, n, "%s", buf);
}

void formatFit(double v, char* out, size_t n, int maxChars) {
  for (int sig = 10; sig >= 1; sig--) {
    formatNumber(v, out, n, sig);
    if ((int)strlen(out) <= maxChars) return;
  }
}

static bool fraction(double v, long& p, long& q, long maxQ) {
  double x = v;
  double h0 = 0, h1 = 1, k0 = 1, k1 = 0;
  for (int i = 0; i < 40; i++) {
    double a = floor(x);
    if (fabs(a) > 1e9) break;
    double h2 = a * h1 + h0, k2 = a * k1 + k0;
    if (k2 > maxQ) break;
    h0 = h1;
    h1 = h2;
    k0 = k1;
    k1 = k2;
    if (fabs(v - h1 / k1) <= 1e-11 * fmax(1.0, fabs(v))) {
      p = (long)h1;
      q = (long)k1;
      return true;
    }
    double fr = x - a;
    if (fr < 1e-14) break;
    x = 1.0 / fr;
  }
  return false;
}

bool exactForm(double v, char* out, size_t n) {
  if (!isfinite(v) || v == 0 || fabs(v) > 1e6) return false;
  if (fabs(v - round(v)) < 1e-12 * fmax(1.0, fabs(v))) return false;
  long p, q;
  if (fraction(v, p, q, 9999)) {
    snprintf(out, n, "%ld/%ld", p, q);
    return true;
  }
  if (fraction(v / kPi, p, q, 12) && labs(p) <= 99) {
    char num[16] = "";
    if (p == -1) strcpy(num, "-");
    else if (p != 1) snprintf(num, sizeof num, "%ld", p);
    if (q == 1) snprintf(out, n, "%spi", num);
    else snprintf(out, n, "%spi/%ld", num, q);
    return true;
  }
  // raiz: v^2 racional => v = (a/q)*sqrt(b)
  if (fraction(v * v, p, q, 100) && p > 0 && p * q <= 10000) {
    long m = p * q, a = 1, b = m;
    for (long d = 2; d * d <= b; d++)
      while (b % (d * d) == 0) {
        b /= d * d;
        a *= d;
      }
    if (b == 1) return false;
    long g = a, h = q;
    while (h) {
      long t = g % h;
      g = h;
      h = t;
    }
    a /= g;
    long den = q / g;
    const char* sg = v < 0 ? "-" : "";
    char num[64];
    if (a == 1) snprintf(num, sizeof num, "%ssqrt(%ld)", sg, b);
    else snprintf(num, sizeof num, "%s%ldsqrt(%ld)", sg, a, b);
    if (den == 1) snprintf(out, n, "%s", num);
    else snprintf(out, n, "%s/%ld", num, den);
    return true;
  }
  return false;
}

double niceStep(double raw) {
  if (!(raw > 0) || !isfinite(raw)) return 1;
  double e = floor(log10(raw));
  double b = pow(10.0, e);
  double f = raw / b;
  double m = f < 1.5 ? 1 : f < 3.5 ? 2 : f < 7.5 ? 5 : 10;
  return m * b;
}

}  // namespace px
