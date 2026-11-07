// Testes nativos do motor: g++ -std=c++11 -I../src test_expr.cpp ../src/core/*.cpp
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "core/expr.h"
#include "core/numeric.h"

using namespace px;

static int fails = 0, total = 0;

static double ev(const char* s, Context& c, Err* e = nullptr) {
  Program p;
  Err r = p.compile(s);
  if (e) *e = r;
  if (r != Err::None) return NAN;
  double v = p.eval(c);
  if (p.assignVar() >= 0) c.vars[p.assignVar()] = v;
  return v;
}

static void near(const char* s, double want, double tol = 1e-9, bool deg = false) {
  Context c;
  c.degrees = deg;
  c.vars[0] = 2;  // a = 2
  c.vars[VAR_X] = 3;
  double v = ev(s, c);
  total++;
  bool ok = (isnan(want) && isnan(v)) || v == want || fabs(v - want) <= tol * fmax(1.0, fabs(want));
  if (!ok) {
    fails++;
    printf("FALHOU  %-28s = %.15g (esperado %.15g)\n", s, v, want);
  }
}

static void err(const char* s, Err want) {
  Program p;
  Err e = p.compile(s);
  total++;
  if (e != want) {
    fails++;
    printf("FALHOU  erro de '%s': %d (esperado %d) pos %d\n", s, (int)e, (int)want, p.errorPos());
  }
}

static void fmt(double v, const char* want, int fix = -1) {
  char b[48];
  formatNumber(v, b, sizeof b, 10, fix);
  total++;
  if (strcmp(b, want)) {
    fails++;
    printf("FALHOU  formato %.17g -> '%s' (esperado '%s')\n", v, b, want);
  }
}

static void exact(double v, const char* want) {
  char b[48] = "";
  bool ok = exactForm(v, b, sizeof b);
  total++;
  if (strcmp(ok ? b : "-", want)) {
    fails++;
    printf("FALHOU  exata %.17g -> '%s' (esperado '%s')\n", v, ok ? b : "-", want);
  }
}

int main() {
  const double PI = kPi;
  // aritmética e precedência
  near("1+2*3", 7);
  near("(1+2)*3", 9);
  near("2^3^2", 512);
  near("-2^2", -4);
  near("2^-1", 0.5);
  near("10/4/5", 0.5);
  near("7-2-1", 4);
  near("0.1+0.2", 0.3);
  near("1.5e3", 1500);
  near("2e-3", 0.002);
  near(".5", 0.5);
  near("2**3", 8);
  // multiplicação implícita
  near("2x", 6);
  near("2x^2", 18);
  near("3(x+1)", 12);
  near("(x+1)(x-1)", 8);
  near("2pi", 2 * PI);
  near("xsin(x)", 3 * sin(3.0));
  near("ax", 6);
  near("2e", 2 * kE);
  near("1/2x", 1.5);
  near("sin x^2", sin(9.0));
  near("ln 2", log(2.0));
  near("|x-5|", 2);
  near("2|1-x|", 4);
  near("||x|-5|", 2);
  // funções
  near("sqrt(16)", 4);
  near("cbrt(-27)", -3);
  near("root(-32,5)", -2);
  near("(-8)^(1/3)", -2);
  near("log(1000)", 3);
  near("log(8,2)", 3);
  near("log2(1024)", 10);
  near("log20", log10(20.0));
  near("exp(1)", kE);
  near("5!", 120);
  near("0!", 1);
  near("3!!", 720);
  near("ncr(5,2)", 10);
  near("npr(5,2)", 20);
  near("gcd(12,18)", 6);
  near("lcm(4,6)", 12);
  near("mod(-7,3)", 2);
  near("min(4,2,8)", 2);
  near("max(4,2,8)", 8);
  near("round(2.567,2)", 2.57);
  near("floor(-1.5)", -2);
  near("ipart(-1.5)", -1);
  near("fpart(2.25)", 0.25);
  near("hypot(3,4)", 5);
  near("gamma(5)", 24);
  near("sinh(0)+cosh(0)", 1);
  near("atan2(1,1)", PI / 4);
  // trigonometria
  near("sin(pi)", 0, 1e-15);
  near("cos(pi/2)", 0, 1e-15);
  near("tan(pi/4)", 1);
  near("sin(30)", 0.5, 1e-12, true);
  near("cos(60)", 0.5, 1e-12, true);
  near("asin(1)", 90, 1e-12, true);
  near("tan(90)", INFINITY, 0, true);
  near("sin(180)", 0, 1e-15, true);
  near("sin(1e-20)", 1e-20, 1e-30);
  // constantes, variáveis
  near("pi", PI);
  near("phi", 1.618033988749895);
  near("A+X", 5);
  // especiais
  near("int(x^2,0,3)", 9, 1e-10);
  near("int(sin(x),0,pi)", 2, 1e-10);
  near("int(1/sqrt(x),0,1)", 2, 1e-8);
  near("int(ln(x),0,1)", -1, 1e-8);
  near("int(sin(x)^2,0,10pi)", 5 * PI, 1e-9);
  near("int(1/(1+x^2),-100,100)", 2 * atan(100.0), 1e-9);
  near("int(exp(-x^2),-5,5)", sqrt(PI), 1e-9);
  near("int(x,3,0)", -4.5, 1e-10);
  near("der(x^3,2)", 12, 1e-8);
  near("der(sin(x),0)", 1, 1e-9);
  near("der(cos(x),0)", 0, 1e-12);
  near("sum(x,1,100)", 5050);
  near("sum(1/x^2,1,1000)", 1.6439345666815615, 1e-12);
  near("prod(x,1,5)", 120);
  near("solve(x^2-2,1)", sqrt(2.0), 1e-12);
  near("solve(cos(x)-x)", 0.7390851332151607, 1e-12);
  near("solve(x^3-x-1,0)", 1.324717957244746, 1e-12);
  near("solve(x^2+1,0)", NAN);
  near("int(int(x,0,1),0,2)", 1, 1e-9);
  near("sin(x", sin(3.0));
  near("2(3+4", 14);
  // erros
  err("", Err::Empty);
  err("   ", Err::Empty);
  err("2+", Err::Syntax);
  err("2+*3", Err::Syntax);
  err("(2+3))", Err::Paren);
  err("1.2.3", Err::Syntax);
  err("ncr(5)", Err::Args);
  err("int(x,1)", Err::Args);
  err("2#3", Err::Syntax);
  err("e=5", Err::Assign);
  err("a=3+1", Err::None);
  // atribuição e ans
  {
    Context c;
    ev("a=5", c);
    ev("b = a*2", c);
    total++;
    if (c.vars[1] != 10) {
      fails++;
      printf("FALHOU  atribuicao b=%g\n", c.vars[1]);
    }
  }
  // formatação
  fmt(0.30000000000000004, "0.3");
  fmt(1.0 / 3, "0.3333333333");
  fmt(123456789012.0, "1.23456789e11");
  fmt(-0.00001234, "-1.234e-5");
  fmt(1e100, "1e100");
  fmt(42, "42");
  fmt(9999999999.6, "1e10");
  fmt(2.5, "2.500", 3);
  fmt(-0.0, "0");
  fmt(INFINITY, "inf");
  exact(0.75, "3/4");
  exact(-2.5, "-5/2");
  exact(1.0 / 3, "1/3");
  exact(PI / 4, "pi/4");
  exact(2 * PI / 3, "2pi/3");
  exact(-PI, "-pi");
  exact(sqrt(2.0) / 2, "sqrt(2)/2");
  exact(2 * sqrt(3.0), "2sqrt(3)");
  exact(3, "-");
  exact(sqrt(2.0) + 0.1, "-");
  // passo bonito
  total++;
  if (niceStep(3.7) != 5 || niceStep(0.13) != 0.1 || niceStep(170) != 200) {
    fails++;
    printf("FALHOU  niceStep\n");
  }
  printf("%d/%d testes ok\n", total - fails, total);
  return fails ? 1 : 0;
}
