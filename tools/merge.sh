#!/usr/bin/env bash
# Gera out/prisma-full.bin (bootloader + particoes + app, gravar em 0x0)
# e out/prisma.bin (so o app, para M5Launcher / M5Burner / atualizar).
set -euo pipefail
cd "$(dirname "$0")/.."
B=.pio/build/cardputer-adv
ESPTOOL=${ESPTOOL:-esptool.py}
mkdir -p out
$ESPTOOL --chip esp32s3 merge_bin -o out/prisma-full.bin \
  --flash_mode dio --flash_size 8MB \
  0x0 "$B/bootloader.bin" 0x8000 "$B/partitions.bin" 0x10000 "$B/firmware.bin"
cp "$B/firmware.bin" out/prisma.bin
ls -la out
