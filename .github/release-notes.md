## Como gravar

1. No Chrome, abra o [ESP Web Flasher](https://espressif.github.io/esptool-js/) e ligue o Cardputer ADV pelo USB-C.
2. **Connect** (se nao aparecer: desligue, segure **G0** e ligue o cabo).
3. Grave **`prisma-full.bin`** no endereco **`0x0`**. Desligue e ligue.

| Arquivo | Para que serve |
|---|---|
| `prisma-full.bin` | Tudo junto (bootloader, particoes e Prisma). Gravar em `0x0`. |
| `prisma.bin` | So o app: para o M5Launcher (copie para o SD) ou M5Burner. |

Gravar o `prisma-full.bin` apaga o firmware que estava no Cardputer.
