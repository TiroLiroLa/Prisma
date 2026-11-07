// Prisma — rotinas numéricas e formatação
#pragma once
#include <stddef.h>

namespace px {

const double kPi = 3.14159265358979323846;
const double kE = 2.71828182845904523536;

typedef double (*RealFn)(double x, void* user);

// Integral definida (Gauss–Kronrod 15 adaptativo; não avalia as pontas,
// então aguenta singularidades integráveis como 1/sqrt(x) em 0).
double integrate(RealFn f, void* u, double a, double b);
// Derivada numérica (diferença central + extrapolação de Richardson).
double derivative(RealFn f, void* u, double x);
// Resolve f(x)=0 perto de 'guess' (Newton, com busca de troca de sinal como reserva).
double solveNear(RealFn f, void* u, double guess);
// Raiz num intervalo [a,b] com f(a)*f(b)<=0.
double rootIn(RealFn f, void* u, double a, double b);
// Mínimo (ou máximo) local em [a,b] por seção áurea.
double extremumIn(RealFn f, void* u, double a, double b, bool maximum);

double factorial(double n);
double nCr(double n, double r);
double nPr(double n, double r);
double gcd(double a, double b);
double lcm(double a, double b);

// Formata um resultado. sig = algarismos significativos; fix >= 0 força casas decimais.
void formatNumber(double v, char* out, size_t n, int sig = 10, int fix = -1);
// Forma curta para rótulos de eixo (4 algarismos).
void formatShort(double v, char* out, size_t n);
// Ajusta um número a no máximo 'maxChars' caracteres.
void formatFit(double v, char* out, size_t n, int maxChars);
// Forma exata alternativa, se houver: "3/4", "-5/2", "pi/3", "2pi/3". Retorna false se não achar.
bool exactForm(double v, char* out, size_t n);
// Passo "bonito" (1, 2, 5 x 10^k) próximo de 'raw'.
double niceStep(double raw);

}  // namespace px
