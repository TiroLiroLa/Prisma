#!/bin/sh
# Compila o simulador e gera as capturas de tela em sim/out/*.png
set -e
cd "$(dirname "$0")/.."
OUT=sim/out
mkdir -p "$OUT" build
g++ -std=c++11 -O2 -Wall -Wno-format-truncation -Isrc -Isim sim/sim.cpp src/core/*.cpp -o build/prisma_sim
S=build/prisma_sim

$S "{shot:01_inicio}" $OUT
$S "{enter}sqrt(8)/4{enter}2pi/3{enter}0.1+0.2{enter}{shot:02b_exato}" $OUT
$S "{enter}sqrt(8)/4{enter}2pi/3{enter}int(x^2,0,3){enter}solve(x^3-x-1){enter}*2{enter}der(sin(x)/x,1{shot:02_calculo}" $OUT
$S "{enter}12/{tab}1{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{fn+.}{shot:03_catalogo}" $OUT
$S "2{fn+.}{fn+.}3cos(x)/(1+x^2){fn+;}{shot:04_funcoes}" $OUT
$S "2{fn+.}{fn+.}3cos(x)/(1+x^2){enter}{shot:05_grafico}" $OUT
$S "2{enter}{tab}{shot:06_analise_menu}" $OUT
$S "2{enter}{tab}3{shot:07_maximo}" $OUT
$S "2{enter}{tab}6{enter}{enter}{shot:08_integral}" $OUT
$S "2{enter}t,,,,,,,,,,{shot:09_traco}" $OUT
$S "2{enter}{tab}4{shot:10_intersecao}" $OUT
$S "3{wait:100}{shot:11_3d}" $OUT
$S "3{enter}{ctrl+del}sin(sqrt(x^2+y^2))/sqrt(x^2+y^2){enter}{tab}2{ctrl+del}-12{tab}{ctrl+del}12{tab}{ctrl+del}-12{tab}{ctrl+del}12{enter}{shot:12_3d_sinc}" $OUT
$S "3{enter}{ctrl+del}x^2-y^2{enter}{tab}3{tab}3{shot:13_3d_malha}" $OUT
$S "3{enter}{ctrl+del}exp(-(x^2+y^2)/2)*cos(2x){enter}2{shot:14_3d_topo}" $OUT
$S "3{enter}{shot:15_3d_editar}" $OUT
$S "4{shot:16_tabela}" $OUT
$S "5..{shot:17_ajustes}" $OUT
$S "{enter}{tab}5{shot:18_ajuda}" $OUT
$S "{enter}2+{enter}{shot:19_erro}" $OUT
$S "2{fn+.}{fn+.}tan(x){enter}{shot:20_assintota}" $OUT

for f in $OUT/*.ppm; do
  python3 -c "from PIL import Image; import sys; Image.open(sys.argv[1]).save(sys.argv[1][:-4]+'.png')" "$f"
  rm "$f"
done
ls $OUT
