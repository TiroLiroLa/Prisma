// Prisma — motor de expressões
// Compila texto ("2x^2+sin(x)") para bytecode e avalia rápido (usado milhares
// de vezes por quadro nos gráficos). Sem alocação dinâmica.
#pragma once
#include <stdint.h>

namespace px {

enum class Err : uint8_t { None, Empty, Syntax, Paren, Args, TooLong, Assign };
const char* errText(Err e);

enum : uint8_t { VAR_X = 'x' - 'a', VAR_Y = 'y' - 'a', VAR_T = 't' - 'a', VAR_ANS = 26, NVARS = 27 };

struct Context {
  double vars[NVARS];
  bool degrees;
  // f1..f4 dentro de expressões (ex.: "f1(2)" na calculadora)
  double (*userFn)(void* user, int idx, double x);
  void* user;
  uint8_t depth;
  uint32_t rng;
  // sem lista de inicialização para "degrees": o Arduino.h define degrees(x) como macro
  Context() : userFn(nullptr), user(nullptr), depth(0), rng(0x9E3779B9u) {
    degrees = false;
    for (int i = 0; i < NVARS; i++) vars[i] = 0;
  }
};

struct Op {
  uint8_t code;
  uint8_t a;
  uint16_t b;
};

class Program {
 public:
  enum { MAXOPS = 128, MAXK = 24, MAXSTACK = 24 };

  Program() { clear(); }
  void clear();
  // Compila. Aceita atribuição "a=expr" (uma letra). Retorna Err::None se ok.
  Err compile(const char* src);
  bool ok() const { return err_ == Err::None && nops_ > 0; }
  Err error() const { return err_; }
  int errorPos() const { return errPos_; }
  int assignVar() const { return assign_; }  // -1 se não for atribuição
  bool uses(int var) const;                  // a expressão lê esta variável?

  double eval(Context& c) const { return ok() ? run(0, nops_, c) : __builtin_nan(""); }

 private:
  friend class Parser;
  friend struct ChunkFn;
  double run(int pc, int end, Context& c) const;

  Op ops_[MAXOPS];
  double k_[MAXK];
  int nops_, nk_;
  Err err_;
  int errPos_;
  int assign_;
};

// Nomes reconhecidos (para o catálogo da interface)
struct CatalogItem {
  const char* insert;  // texto inserido
  const char* help;    // descrição curta
};
extern const CatalogItem kCatalog[];
extern const int kCatalogCount;

}  // namespace px
