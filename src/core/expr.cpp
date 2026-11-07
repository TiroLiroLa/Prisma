#include "expr.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "numeric.h"

namespace px {

const char* errText(Err e) {
  switch (e) {
    case Err::None: return "";
    case Err::Empty: return "Expressao vazia";
    case Err::Syntax: return "Erro de sintaxe";
    case Err::Paren: return "Parenteses";
    case Err::Args: return "Numero de argumentos";
    case Err::TooLong: return "Expressao longa demais";
    case Err::Assign: return "Nao da para guardar em e";
  }
  return "Erro";
}

// ---------------------------------------------------------------- opcodes
enum : uint8_t { O_K, O_VAR, O_ADD, O_SUB, O_MUL, O_DIV, O_POW, O_NEG, O_FACT, O_ABS, O_F, O_UF, O_SKIP, O_SPECIAL };

enum : uint8_t {
  F_SIN, F_COS, F_TAN, F_SEC, F_CSC, F_COT, F_ASIN, F_ACOS, F_ATAN, F_SINH, F_COSH, F_TANH, F_ASINH, F_ACOSH,
  F_ATANH, F_EXP, F_LN, F_LOG, F_LOG2, F_SQRT, F_CBRT, F_ABS, F_FLOOR, F_CEIL, F_ROUND, F_SIGN, F_IPART,
  F_FPART, F_MIN, F_MAX, F_MOD, F_GCD, F_LCM, F_NCR, F_NPR, F_ATAN2, F_ROOT, F_HYPOT, F_GAMMA, F_RAND
};
enum : uint8_t { S_INT, S_DER, S_SUM, S_PROD, S_SOLVE };
enum : uint8_t { C_PI, C_E, C_PHI, C_ANS };
enum : uint8_t { NK_FN, NK_SPEC, NK_CONST, NK_USERF };

struct NameDef {
  const char* s;
  uint8_t kind, id, minA, maxA;
};

static const NameDef kNames[] = {
    {"sin", NK_FN, F_SIN, 1, 1},       {"cos", NK_FN, F_COS, 1, 1},       {"tan", NK_FN, F_TAN, 1, 1},
    {"sec", NK_FN, F_SEC, 1, 1},       {"csc", NK_FN, F_CSC, 1, 1},       {"cot", NK_FN, F_COT, 1, 1},
    {"asin", NK_FN, F_ASIN, 1, 1},     {"acos", NK_FN, F_ACOS, 1, 1},     {"atan", NK_FN, F_ATAN, 1, 1},
    {"sinh", NK_FN, F_SINH, 1, 1},     {"cosh", NK_FN, F_COSH, 1, 1},     {"tanh", NK_FN, F_TANH, 1, 1},
    {"asinh", NK_FN, F_ASINH, 1, 1},   {"acosh", NK_FN, F_ACOSH, 1, 1},   {"atanh", NK_FN, F_ATANH, 1, 1},
    {"exp", NK_FN, F_EXP, 1, 1},       {"ln", NK_FN, F_LN, 1, 1},         {"log", NK_FN, F_LOG, 1, 2},
    {"log2", NK_FN, F_LOG2, 1, 1},     {"sqrt", NK_FN, F_SQRT, 1, 1},     {"cbrt", NK_FN, F_CBRT, 1, 1},
    {"abs", NK_FN, F_ABS, 1, 1},       {"floor", NK_FN, F_FLOOR, 1, 1},   {"ceil", NK_FN, F_CEIL, 1, 1},
    {"round", NK_FN, F_ROUND, 1, 2},   {"sign", NK_FN, F_SIGN, 1, 1},     {"ipart", NK_FN, F_IPART, 1, 1},
    {"fpart", NK_FN, F_FPART, 1, 1},   {"min", NK_FN, F_MIN, 2, 8},       {"max", NK_FN, F_MAX, 2, 8},
    {"mod", NK_FN, F_MOD, 2, 2},       {"gcd", NK_FN, F_GCD, 2, 2},       {"lcm", NK_FN, F_LCM, 2, 2},
    {"ncr", NK_FN, F_NCR, 2, 2},       {"npr", NK_FN, F_NPR, 2, 2},       {"atan2", NK_FN, F_ATAN2, 2, 2},
    {"root", NK_FN, F_ROOT, 2, 2},     {"hypot", NK_FN, F_HYPOT, 2, 2},   {"gamma", NK_FN, F_GAMMA, 1, 1},
    {"rand", NK_FN, F_RAND, 0, 0},     {"int", NK_SPEC, S_INT, 3, 3},     {"der", NK_SPEC, S_DER, 2, 2},
    {"sum", NK_SPEC, S_SUM, 3, 3},     {"prod", NK_SPEC, S_PROD, 3, 3},   {"solve", NK_SPEC, S_SOLVE, 1, 2},
    {"pi", NK_CONST, C_PI, 0, 0},      {"e", NK_CONST, C_E, 0, 0},        {"phi", NK_CONST, C_PHI, 0, 0},
    {"ans", NK_CONST, C_ANS, 0, 0},    {"f1", NK_USERF, 0, 1, 1},         {"f2", NK_USERF, 1, 1, 1},
    {"f3", NK_USERF, 2, 1, 1},         {"f4", NK_USERF, 3, 1, 1},
};
static const int kNameCount = sizeof(kNames) / sizeof(kNames[0]);

const CatalogItem kCatalog[] = {
    {"sqrt(", "raiz quadrada"},          {"cbrt(", "raiz cubica"},
    {"root(", "root(x,n) raiz n-esima"}, {"abs(", "valor absoluto  |x|"},
    {"ln(", "log natural"},              {"log(", "log10; log(x,b) base b"},
    {"log2(", "log base 2"},             {"exp(", "e elevado a x"},
    {"sin(", "seno"},                    {"cos(", "cosseno"},
    {"tan(", "tangente"},                {"asin(", "arco seno"},
    {"acos(", "arco cosseno"},           {"atan(", "arco tangente"},
    {"atan2(", "atan2(y,x)"},            {"sec(", "secante"},
    {"csc(", "cossecante"},              {"cot(", "cotangente"},
    {"sinh(", "seno hiperbolico"},       {"cosh(", "cosseno hiperbolico"},
    {"tanh(", "tangente hiperbolica"},   {"asinh(", "arco seno hiperb."},
    {"acosh(", "arco cosseno hiperb."},  {"atanh(", "arco tangente hiperb."},
    {"int(", "int(f,a,b) integral em x"}, {"der(", "der(f,a) derivada em x=a"},
    {"sum(", "sum(f,a,b) somatorio x"},  {"prod(", "prod(f,a,b) produtorio"},
    {"solve(", "solve(f,chute) f(x)=0"}, {"!", "fatorial  5!"},
    {"ncr(", "ncr(n,r) combinacoes"},    {"npr(", "npr(n,r) arranjos"},
    {"gcd(", "mdc"},                     {"lcm(", "mmc"},
    {"mod(", "mod(a,b) resto"},          {"floor(", "arredonda p/ baixo"},
    {"ceil(", "arredonda p/ cima"},      {"round(", "round(x[,casas])"},
    {"ipart(", "parte inteira"},         {"fpart(", "parte fracionaria"},
    {"sign(", "sinal"},                  {"min(", "minimo"},
    {"max(", "maximo"},                  {"hypot(", "hypot(a,b)"},
    {"gamma(", "funcao gama"},           {"rand", "aleatorio [0,1)"},
    {"pi", "3.14159..."},                {"e", "2.71828..."},
    {"phi", "razao aurea"},              {"ans", "ultimo resultado"},
    {"f1(", "funcao f1 do grafico"},     {"f2(", "funcao f2 do grafico"},
};
const int kCatalogCount = sizeof(kCatalog) / sizeof(kCatalog[0]);

// ---------------------------------------------------------------- lexer
enum TokT : uint8_t { T_END, T_NUM, T_VAR, T_NAME, T_OP, T_LP, T_RP, T_COMMA, T_BAR, T_BANG, T_BAD };

struct Tok {
  TokT t;
  char ch;
  int pos;
  int name;  // índice em kNames
  int var;
  double num;
};

class Parser {
 public:
  Parser(Program& P, const char* s, int start) : P(P), s(s), p(start), depth(0), absDepth(0) {}
  bool run() {
    if (!next()) return false;
    if (tok.t == T_END) return fail(Err::Empty, p);
    if (!expr()) return false;
    if (tok.t == T_RP) return fail(Err::Paren, tok.pos);
    if (tok.t != T_END) return fail(Err::Syntax, tok.pos);
    return true;
  }

 private:
  Program& P;
  const char* s;
  int p;
  Tok tok;
  int depth;
  int absDepth;

  bool fail(Err e, int pos) {
    if (P.err_ == Err::None) {
      P.err_ = e;
      P.errPos_ = pos;
    }
    return false;
  }

  bool next() {
    while (s[p] == ' ') p++;
    tok.pos = p;
    char c = s[p];
    if (!c) {
      tok.t = T_END;
      return true;
    }
    if (isdigit((unsigned char)c) || (c == '.' && isdigit((unsigned char)s[p + 1]))) {
      int st = p;
      while (isdigit((unsigned char)s[p])) p++;
      if (s[p] == '.') {
        p++;
        while (isdigit((unsigned char)s[p])) p++;
      }
      if ((s[p] == 'e' || s[p] == 'E') &&
          (isdigit((unsigned char)s[p + 1]) ||
           ((s[p + 1] == '+' || s[p + 1] == '-') && isdigit((unsigned char)s[p + 2])))) {
        p += 2;
        while (isdigit((unsigned char)s[p])) p++;
      }
      if (s[p] == '.') return fail(Err::Syntax, p);
      char buf[40];
      int n = p - st;
      if (n >= (int)sizeof(buf)) return fail(Err::Syntax, st);
      memcpy(buf, s + st, n);
      buf[n] = 0;
      tok.t = T_NUM;
      tok.num = strtod(buf, nullptr);
      return true;
    }
    if (isalpha((unsigned char)c)) {
      int best = -1, bestLen = 0;
      for (int i = 0; i < kNameCount; i++) {
        const char* nm = kNames[i].s;
        int L = (int)strlen(nm);
        if (L <= bestLen) continue;
        int j = 0;
        while (j < L && tolower((unsigned char)s[p + j]) == nm[j]) j++;
        if (j != L) continue;
        // nomes com dígito (log2, atan2, f1) só valem seguidos de '('
        bool hasDigit = false;
        for (int k = 0; k < L; k++) hasDigit |= isdigit((unsigned char)nm[k]) != 0;
        if (hasDigit && s[p + L] != '(') continue;
        best = i;
        bestLen = L;
      }
      if (best >= 0) {
        tok.t = T_NAME;
        tok.name = best;
        p += bestLen;
        return true;
      }
      tok.t = T_VAR;
      tok.var = tolower((unsigned char)c) - 'a';
      p++;
      return true;
    }
    p++;
    switch (c) {
      case '+': case '-': case '*': case '/': case '^':
        tok.t = T_OP;
        tok.ch = c;
        if (c == '*' && s[p] == '*') {  // ** também é potência
          p++;
          tok.ch = '^';
        }
        return true;
      case '(': tok.t = T_LP; return true;
      case ')': tok.t = T_RP; return true;
      case ',': tok.t = T_COMMA; return true;
      case '|': tok.t = T_BAR; return true;
      case '!': tok.t = T_BANG; return true;
    }
    return fail(Err::Syntax, p - 1);
  }

  bool emit(uint8_t code, uint8_t a, uint16_t b, int delta) {
    if (P.nops_ >= Program::MAXOPS) return fail(Err::TooLong, tok.pos);
    Op& o = P.ops_[P.nops_++];
    o.code = code;
    o.a = a;
    o.b = b;
    depth += delta;
    if (depth > Program::MAXSTACK) return fail(Err::TooLong, tok.pos);
    return true;
  }
  bool pushK(double v) {
    int i = 0;
    while (i < P.nk_ && memcmp(&P.k_[i], &v, sizeof v) != 0) i++;
    if (i == P.nk_) {
      if (P.nk_ >= Program::MAXK) return fail(Err::TooLong, tok.pos);
      P.k_[P.nk_++] = v;
    }
    return emit(O_K, 0, (uint16_t)i, 1);
  }

  bool startsPrimary() const {
    switch (tok.t) {
      case T_NUM: case T_VAR: case T_NAME: case T_LP: return true;
      case T_BAR: return absDepth == 0;
      default: return false;
    }
  }

  bool expr() {
    if (!term()) return false;
    while (tok.t == T_OP && (tok.ch == '+' || tok.ch == '-')) {
      char op = tok.ch;
      if (!next() || !term()) return false;
      if (!emit(op == '+' ? O_ADD : O_SUB, 0, 0, -1)) return false;
    }
    return true;
  }

  bool term() {
    if (!unary()) return false;
    for (;;) {
      if (tok.t == T_OP && (tok.ch == '*' || tok.ch == '/')) {
        char op = tok.ch;
        if (!next() || !unary()) return false;
        if (!emit(op == '*' ? O_MUL : O_DIV, 0, 0, -1)) return false;
      } else if (startsPrimary()) {  // multiplicação implícita: 2x, 3(x+1), xsin(x)
        if (!power()) return false;
        if (!emit(O_MUL, 0, 0, -1)) return false;
      } else {
        return true;
      }
    }
  }

  bool unary() {
    if (tok.t == T_OP && tok.ch == '-') {
      if (!next() || !unary()) return false;
      return emit(O_NEG, 0, 0, 0);
    }
    if (tok.t == T_OP && tok.ch == '+') {
      if (!next()) return false;
      return unary();
    }
    return power();
  }

  bool power() {
    if (!postfix()) return false;
    if (tok.t == T_OP && tok.ch == '^') {
      if (!next() || !unary()) return false;
      return emit(O_POW, 0, 0, -1);
    }
    return true;
  }

  bool postfix() {
    if (!primary()) return false;
    while (tok.t == T_BANG) {
      if (!emit(O_FACT, 0, 0, 0) || !next()) return false;
    }
    return true;
  }

  bool closeParen() {
    if (tok.t == T_RP) return next();
    if (tok.t == T_END) return true;  // fecha sozinho no fim: "sin(x"
    return fail(Err::Paren, tok.pos);
  }

  bool primary() {
    switch (tok.t) {
      case T_NUM: {
        double v = tok.num;
        return pushK(v) && next();
      }
      case T_VAR: {
        int v = tok.var;
        return emit(O_VAR, (uint8_t)v, 0, 1) && next();
      }
      case T_LP:
        return next() && expr() && closeParen();
      case T_BAR: {
        if (!next()) return false;
        absDepth++;
        bool ok = expr();
        absDepth--;
        if (!ok) return false;
        if (tok.t == T_BAR) {
          if (!next()) return false;
        } else if (tok.t != T_END) {
          return fail(Err::Paren, tok.pos);
        }
        return emit(O_ABS, 0, 0, 0);
      }
      case T_NAME:
        return name();
      case T_END:
        return fail(Err::Syntax, tok.pos);
      default:
        return fail(Err::Syntax, tok.pos);
    }
  }

  bool name() {
    const NameDef& d = kNames[tok.name];
    int pos = tok.pos;
    if (!next()) return false;
    if (d.kind == NK_CONST) {
      switch (d.id) {
        case C_PI: return pushK(kPi);
        case C_E: return pushK(kE);
        case C_PHI: return pushK(1.6180339887498948482);
        default: return emit(O_VAR, VAR_ANS, 0, 1);
      }
    }
    if (d.kind == NK_SPEC) {
      if (tok.t != T_LP) return fail(Err::Syntax, tok.pos);
      if (!next()) return false;
      int skipAt = P.nops_;
      if (!emit(O_SKIP, 0, 0, 0)) return false;
      int saved = depth;
      depth = 0;
      if (!expr()) return false;  // corpo: avaliado com x variando
      depth = saved;
      P.ops_[skipAt].b = (uint16_t)(P.nops_ - skipAt - 1);
      int n = 1;
      while (tok.t == T_COMMA) {
        if (!next() || !expr()) return false;
        n++;
      }
      if (!closeParen()) return false;
      if (n < d.minA || n > d.maxA) return fail(Err::Args, pos);
      if (d.id == S_SOLVE && n == 1) {
        if (!pushK(0)) return false;
        n = 2;
      }
      return emit(O_SPECIAL, d.id, (uint16_t)skipAt, 1 - (n - 1));
    }
    int n = 0;
    if (tok.t == T_LP) {
      if (!next()) return false;
      if (tok.t == T_RP) {
        if (!next()) return false;
      } else {
        if (!expr()) return false;
        n = 1;
        while (tok.t == T_COMMA) {
          if (!next() || !expr()) return false;
          n++;
        }
        if (!closeParen()) return false;
      }
    } else if (d.maxA == 0) {
      n = 0;
    } else if (d.minA <= 1) {  // sem parênteses: "sin x", "ln 2"
      if (!unary()) return false;
      n = 1;
    } else {
      return fail(Err::Syntax, tok.pos);
    }
    if (n < d.minA || n > d.maxA) return fail(Err::Args, pos);
    if (d.kind == NK_USERF) return emit(O_UF, d.id, 0, 0);
    return emit(O_F, d.id, (uint16_t)n, 1 - n);
  }
};

// ---------------------------------------------------------------- Program
void Program::clear() {
  nops_ = 0;
  nk_ = 0;
  err_ = Err::Empty;
  errPos_ = 0;
  assign_ = -1;
}

Err Program::compile(const char* src) {
  clear();
  err_ = Err::None;
  int start = 0;
  // atribuição: "a = expr"
  int q = 0;
  while (src[q] == ' ') q++;
  if (isalpha((unsigned char)src[q])) {
    int r = q + 1;
    while (src[r] == ' ') r++;
    if (src[r] == '=') {
      char v = (char)tolower((unsigned char)src[q]);
      if (v == 'e') {
        err_ = Err::Assign;
        errPos_ = q;
        return err_;
      }
      assign_ = v - 'a';
      start = r + 1;
    }
  }
  Parser ps(*this, src, start);
  if (!ps.run()) {
    nops_ = 0;
    return err_;
  }
  return Err::None;
}

bool Program::uses(int var) const {
  for (int i = 0; i < nops_; i++)
    if (ops_[i].code == O_VAR && ops_[i].a == var) return true;
  return false;
}

struct ChunkFn {
  const Program* p;
  int cs, ce;
  Context* c;
  static double call(double x, void* u) {
    ChunkFn* f = (ChunkFn*)u;
    f->c->vars[VAR_X] = x;
    return f->p->run(f->cs, f->ce, *f->c);
  }
};

static inline double trigIn(const Context& c, double v) { return c.degrees ? v * (kPi / 180.0) : v; }
static inline double trigOut(const Context& c, double v) { return c.degrees ? v * (180.0 / kPi) : v; }
// sin(pi) dá 1.2e-16; arredonda para 0 resíduos desse tamanho
static inline double snap(double r, double arg) {
  return (fabs(arg) > 0.5 && fabs(r) < 1e-15 * fabs(arg)) ? 0.0 : r;
}

static double powReal(double a, double b) {
  if (a < 0 && b != floor(b)) {
    double inv = 1.0 / b;  // (-8)^(1/3) = -2
    double ri = floor(inv + 0.5);
    if (fabs(inv - ri) < 1e-9 && fmod(fabs(ri), 2.0) == 1.0) return -pow(-a, b);
  }
  return pow(a, b);
}

static double callFn(uint8_t id, const double* a, int n, Context& c) {
  double x = a[0];
  switch (id) {
    case F_SIN: { double r = trigIn(c, x); return snap(sin(r), r); }
    case F_COS: { double r = trigIn(c, x); return snap(cos(r), r); }
    case F_TAN: { double r = trigIn(c, x); return snap(sin(r), r) / snap(cos(r), r); }
    case F_SEC: { double r = trigIn(c, x); return 1.0 / snap(cos(r), r); }
    case F_CSC: { double r = trigIn(c, x); return 1.0 / snap(sin(r), r); }
    case F_COT: { double r = trigIn(c, x); return snap(cos(r), r) / snap(sin(r), r); }
    case F_ASIN: return trigOut(c, asin(x));
    case F_ACOS: return trigOut(c, acos(x));
    case F_ATAN: return trigOut(c, atan(x));
    case F_ATAN2: return trigOut(c, atan2(a[0], a[1]));
    case F_SINH: return sinh(x);
    case F_COSH: return cosh(x);
    case F_TANH: return tanh(x);
    case F_ASINH: return asinh(x);
    case F_ACOSH: return acosh(x);
    case F_ATANH: return atanh(x);
    case F_EXP: return exp(x);
    case F_LN: return log(x);
    case F_LOG: return n == 2 ? log(x) / log(a[1]) : log10(x);
    case F_LOG2: return log2(x);
    case F_SQRT: return sqrt(x);
    case F_CBRT: return cbrt(x);
    case F_ROOT: {
      double k = a[1];
      if (x < 0 && k == floor(k) && fmod(fabs(k), 2.0) == 1.0) return -pow(-x, 1.0 / k);
      return pow(x, 1.0 / k);
    }
    case F_ABS: return fabs(x);
    case F_FLOOR: return floor(x);
    case F_CEIL: return ceil(x);
    case F_ROUND: {
      if (n == 1) return round(x);
      double f = pow(10.0, floor(a[1]));
      return round(x * f) / f;
    }
    case F_SIGN: return x > 0 ? 1 : (x < 0 ? -1 : 0);
    case F_IPART: return trunc(x);
    case F_FPART: return x - trunc(x);
    case F_MIN: { double m = a[0]; for (int i = 1; i < n; i++) m = a[i] < m ? a[i] : m; return m; }
    case F_MAX: { double m = a[0]; for (int i = 1; i < n; i++) m = a[i] > m ? a[i] : m; return m; }
    case F_MOD: return a[1] == 0 ? __builtin_nan("") : a[0] - a[1] * floor(a[0] / a[1]);
    case F_GCD: return gcd(a[0], a[1]);
    case F_LCM: return lcm(a[0], a[1]);
    case F_NCR: return nCr(a[0], a[1]);
    case F_NPR: return nPr(a[0], a[1]);
    case F_HYPOT: return hypot(a[0], a[1]);
    case F_GAMMA: return tgamma(x);
    case F_RAND: {
      uint32_t s = c.rng;
      s ^= s << 13;
      s ^= s >> 17;
      s ^= s << 5;
      c.rng = s;
      return (s >> 8) * (1.0 / 16777216.0);
    }
  }
  return __builtin_nan("");
}

double Program::run(int pc, int end, Context& c) const {
  double st[MAXSTACK];
  int sp = 0;
  while (pc < end) {
    const Op& o = ops_[pc++];
    switch (o.code) {
      case O_K: st[sp++] = k_[o.b]; break;
      case O_VAR: st[sp++] = c.vars[o.a]; break;
      case O_ADD: sp--; st[sp - 1] += st[sp]; break;
      case O_SUB: sp--; st[sp - 1] -= st[sp]; break;
      case O_MUL: sp--; st[sp - 1] *= st[sp]; break;
      case O_DIV: sp--; st[sp - 1] /= st[sp]; break;
      case O_POW: sp--; st[sp - 1] = powReal(st[sp - 1], st[sp]); break;
      case O_NEG: st[sp - 1] = -st[sp - 1]; break;
      case O_ABS: st[sp - 1] = fabs(st[sp - 1]); break;
      case O_FACT: st[sp - 1] = factorial(st[sp - 1]); break;
      case O_F: {
        int n = o.b;
        double r = callFn(o.a, st + sp - n, n, c);
        sp -= n;
        st[sp++] = r;
        break;
      }
      case O_UF: {
        double r = __builtin_nan("");
        if (c.userFn && c.depth < 6) {
          c.depth++;
          r = c.userFn(c.user, o.a, st[sp - 1]);
          c.depth--;
        }
        st[sp - 1] = r;
        break;
      }
      case O_SKIP: pc += o.b; break;
      case O_SPECIAL: {
        ChunkFn cf;
        cf.p = this;
        cf.cs = o.b + 1;
        cf.ce = cf.cs + ops_[o.b].b;
        cf.c = &c;
        double r = __builtin_nan("");
        int nargs = (o.a == S_DER || o.a == S_SOLVE) ? 1 : 2;
        double a0 = st[sp - nargs], a1 = nargs == 2 ? st[sp - 1] : 0;
        sp -= nargs;
        if (c.depth < 6) {
          c.depth++;
          double saveX = c.vars[VAR_X];
          switch (o.a) {
            case S_INT: r = integrate(ChunkFn::call, &cf, a0, a1); break;
            case S_DER: r = derivative(ChunkFn::call, &cf, a0); break;
            case S_SOLVE: r = solveNear(ChunkFn::call, &cf, a0); break;
            case S_SUM:
            case S_PROD: {
              double lo = ceil(a0 - 1e-9), hi = floor(a1 + 1e-9);
              if (!(hi - lo <= 100000)) break;
              r = o.a == S_SUM ? 0 : 1;
              for (double i = lo; i <= hi; i += 1) {
                double v = ChunkFn::call(i, &cf);
                r = o.a == S_SUM ? r + v : r * v;
              }
              break;
            }
          }
          c.vars[VAR_X] = saveX;
          c.depth--;
        }
        st[sp++] = r;
        break;
      }
    }
  }
  return sp > 0 ? st[sp - 1] : __builtin_nan("");
}

}  // namespace px
