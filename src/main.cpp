// Prisma — camada do M5Stack Cardputer ADV
// Tela 240x135 desenhada num sprite (sem piscar), teclado com repetição,
// estado salvo na NVS (namespace "prisma") e volta ao Órbita pelo menu.
#include <M5Cardputer.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include "core/app.h"

using namespace px;

static M5Canvas canvas(&M5Cardputer.Display);

class CanvasGfx : public Gfx {
 public:
  explicit CanvasGfx(M5Canvas& c) : c_(c) {}
  void fillScreen(color_t c) override { c_.fillScreen(c); }
  void fillRect(int x, int y, int w, int h, color_t c) override { c_.fillRect(x, y, w, h, c); }
  void drawRect(int x, int y, int w, int h, color_t c) override { c_.drawRect(x, y, w, h, c); }
  void hline(int x, int y, int w, color_t c) override { c_.drawFastHLine(x, y, w, c); }
  void vline(int x, int y, int h, color_t c) override { c_.drawFastVLine(x, y, h, c); }
  void line(int x0, int y0, int x1, int y1, color_t c) override { c_.drawLine(x0, y0, x1, y1, c); }
  void pixel(int x, int y, color_t c) override { c_.drawPixel(x, y, c); }
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, color_t c) override {
    c_.fillTriangle(x0, y0, x1, y1, x2, y2, c);
  }
  void fillRoundRect(int x, int y, int w, int h, int r, color_t c) override { c_.fillRoundRect(x, y, w, h, r, c); }
  void drawRoundRect(int x, int y, int w, int h, int r, color_t c) override { c_.drawRoundRect(x, y, w, h, r, c); }
  void fillCircle(int x, int y, int r, color_t c) override { c_.fillCircle(x, y, r, c); }
  void drawCircle(int x, int y, int r, color_t c) override { c_.drawCircle(x, y, r, c); }
  void text(int x, int y, const char* s, color_t c, int size) override {
    c_.setTextSize(size);
    c_.setTextColor(c);  // sem cor de fundo = transparente
    c_.drawString(s, x, y);
  }
  void setClip(int x, int y, int w, int h) override { c_.setClipRect(x, y, w, h); }
  void clearClip() override { c_.clearClipRect(); }

 private:
  M5Canvas& c_;
};

class CardputerPlatform : public Platform {
 public:
  uint32_t millis() override { return ::millis(); }

  int battery() override {
    uint32_t t = ::millis();
    if (t - batAt_ > 10000 || batAt_ == 0) {
      batAt_ = t ? t : 1;
      int b = M5.Power.getBatteryLevel();
      bat_ = (b >= 0 && b <= 100) ? b : -1;
    }
    return bat_;
  }

  bool load(void* buf, size_t len) override {
    Preferences p;
    if (!p.begin("prisma", true)) return false;
    bool ok = p.getBytesLength("state") == len && p.getBytes("state", buf, len) == len;
    p.end();
    return ok;
  }

  void save(const void* buf, size_t len) override {
    Preferences p;
    if (!p.begin("prisma", false)) return;
    p.putBytes("state", buf, len);
    p.end();
  }

  void setBrightness(uint8_t level) override { M5Cardputer.Display.setBrightness((uint8_t)(level * 255 / 100)); }

  // Dentro do Órbita o Prisma roda num slot OTA e a partição factory é o Órbita.
  bool hasOrbita() override {
    const esp_partition_t* f = factory();
    return f && esp_ota_get_running_partition() != f;
  }

  void exitToOrbita() override {
    const esp_partition_t* f = factory();
    if (f) esp_ota_set_boot_partition(f);
    esp_restart();
  }

 private:
  static const esp_partition_t* factory() {
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr);
  }
  uint32_t batAt_ = 0;
  int bat_ = -1;
};

static CanvasGfx gfx(canvas);
static CardputerPlatform platform;
static App app;

// ------------------------------------------------------------------ teclado
static Keyboard_Class::KeysState prevKeys;
static bool held = false;
static uint32_t repeatAt = 0;
static Key repeatKey;
static bool haveRepeat = false;

static Key makeKey(KeyCode code, char ch, const Keyboard_Class::KeysState& st) {
  Key k;
  k.code = code;
  k.ch = ch;
  k.fn = st.fn;
  k.ctrl = st.ctrl;
  k.alt = st.alt;
  k.opt = st.opt;
  if (code == K_CHAR && ch == '`') k.code = K_BACK;  // ` (Esc) = voltar, como no Órbita
  return k;
}

static bool contains(const std::vector<char>& w, char c) {
  for (char x : w)
    if (x == c) return true;
  return false;
}

// Envia só o que acabou de ser apertado (segurar 'a' e apertar 'b' não repete o 'a').
static void handleKeys(const Keyboard_Class::KeysState& st, const Keyboard_Class::KeysState& prev) {
  std::vector<char> word = st.word;
  if (st.space && !contains(word, ' ')) word.push_back(' ');
  std::vector<char> pw = prev.word;
  if (prev.space && !contains(pw, ' ')) pw.push_back(' ');
  haveRepeat = false;
  for (char c : word) {
    if (contains(pw, c)) continue;
    repeatKey = makeKey(K_CHAR, c, st);
    haveRepeat = true;
    app.key(repeatKey);
  }
  if (st.enter && !prev.enter) app.key(makeKey(K_ENTER, 0, st));
  if (st.tab && !prev.tab) app.key(makeKey(K_TAB, 0, st));
  if (st.del && !prev.del) {
    repeatKey = makeKey(K_DEL, 0, st);
    haveRepeat = true;
    app.key(repeatKey);
  }
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  canvas.setColorDepth(16);
  canvas.createSprite(SCREEN_W, SCREEN_H);
  canvas.setFont(&fonts::Font0);
  canvas.setTextWrap(false);
  app.begin(&platform);
}

void loop() {
  M5Cardputer.update();
  uint32_t now = ::millis();
  if (M5Cardputer.Keyboard.isChange()) {
    Keyboard_Class::KeysState st = M5Cardputer.Keyboard.keysState();
    if (M5Cardputer.Keyboard.isPressed()) {
      handleKeys(st, held ? prevKeys : Keyboard_Class::KeysState());
      held = true;
      repeatAt = now + 420;
    } else {
      held = false;
      haveRepeat = false;
    }
    prevKeys = st;
  } else if (held && haveRepeat && now >= repeatAt) {
    // repetição: setas, apagar e caracteres
    Keyboard_Class::KeysState st = M5Cardputer.Keyboard.keysState();
    repeatKey.fn = st.fn;
    repeatKey.ctrl = st.ctrl;
    app.key(repeatKey);
    repeatAt = now + 55;
  }

  static uint32_t lastFrame = 0;
  if (app.frameDue(now) && now - lastFrame >= 15) {
    lastFrame = now;
    app.render(gfx);
    canvas.pushSprite(0, 0);
  }
  delay(1);
}
