#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Preferences.h>

#include "touch.h"
#include "keyboard.h"

#include "desktop.h"
#include "chat_app.h"
#include "paint.h"
#include "ai_client.h"
#include "wifi_app.h"
#include "internet_app.h"

#include "welcome.h"

TFT_eSPI tft;

enum AppState { APP_DESKTOP, APP_CHAT, APP_PAINT, APP_WIFI, APP_INTERNET };
static AppState app = APP_DESKTOP;

static bool autoConnectStarted = false;
static uint32_t autoConnectStartMs = 0;

static inline bool inRect(int x,int y,int rx,int ry,int rw,int rh){
  return x>=rx && x<rx+rw && y>=ry && y<ry+rh;
}

static void show_welcome(uint32_t ms = 800) {
  tft.setSwapBytes(true);
  tft.pushImage(0, 0, WELCOME_WIDTH, WELCOME_HEIGHT, (uint16_t*)welcome_map);
  delay(ms);
}

static void startAutoConnectNonBlocking() {
  Preferences p;
  p.begin("wifi", true);
  String ssid = p.getString("ssid", "");
  String pass = p.getString("pass", "");
  p.end();

  ssid.trim();
  pass.trim();
  if (ssid.length() == 0) return;

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  if (pass.length() == 0) WiFi.begin(ssid.c_str());
  else WiFi.begin(ssid.c_str(), pass.c_str());

  autoConnectStarted = true;
  autoConnectStartMs = millis();
}

void setup() {
  Serial.begin(115200);

  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  tft.init();
  tft.setRotation(1);

  show_welcome(800);

  touch_init();
  keyboard_init(&tft);

  paint_init(&tft);
  wifi_app_init(&tft);
  internet_app_init(&tft);

  desktop_init(&tft);
  chat_init(&tft);

  desktop_draw();

  startAutoConnectNonBlocking();
}

void loop() {
  static bool lastPressed = false;

  wifi_app_tick();
  internet_app_tick();
  ai_pollSerial();

  if (autoConnectStarted) {
    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) {
      autoConnectStarted = false;
      Serial.println("Auto WiFi connected ✅");
    } else if (millis() - autoConnectStartMs > 8000) {
      autoConnectStarted = false;
      Serial.println("Auto WiFi timed out ❌");
    }
  }

  int x = 0, y = 0;
  bool pressed = touch_is_pressed();
  if (pressed) {
    if (!touch_get(x, y)) {
      lastPressed = pressed;
      return;
    }
  }

  if (!pressed && lastPressed) {
    keyboard_release();
    paint_release();
    if (app == APP_CHAT) chat_release();
  }

  if (app == APP_WIFI) {
    bool keepOpen = wifi_app_handleTouch(pressed, lastPressed, x, y);
    if (!keepOpen) {
      app = APP_DESKTOP;
      desktop_draw();
      lastPressed = true;
      return;
    }
    lastPressed = pressed;
    return;
  }

  if (app == APP_INTERNET) {
    bool keepOpen = internet_app_handleTouch(pressed, lastPressed, x, y);
    if (!keepOpen) {
      app = APP_DESKTOP;
      desktop_draw();
      lastPressed = true;
      return;
    }
    lastPressed = pressed;
    return;
  }

  if (app == APP_DESKTOP) {
    DesktopAction a = desktop_handleTouch(pressed, lastPressed, x, y);

    if (a == DESKTOP_OPEN_CHAT) {
      app = APP_CHAT;
      keyboard_clear();
      chat_draw();
      lastPressed = true;
      return;
    }
    else if (a == DESKTOP_OPEN_PAINT) {
      app = APP_PAINT;
      paint_draw();
      lastPressed = true;
      return;
    }
    else if (a == DESKTOP_OPEN_WIFI) {
      app = APP_WIFI;
      wifi_app_open();
      lastPressed = true;
      return;
    }
    else if (a == DESKTOP_OPEN_INTERNET) {
      app = APP_INTERNET;
      internet_app_open();
      lastPressed = true;
      return;
    }

    lastPressed = pressed;
    return;
  }

  if (app == APP_CHAT) {
    if (pressed && !lastPressed && inRect(x, y, 260, 4, 52, 17)) {
      app = APP_DESKTOP;
      desktop_draw();
      lastPressed = true;
      return;
    }
    chat_handleTouch(pressed, lastPressed, x, y);
    lastPressed = pressed;
    return;
  }

  if (app == APP_PAINT) {
    if (pressed && !lastPressed) {
      if (x >= 320 - 16 - 6 && x < 320 - 6 && y >= 2 && y < 16) {
        app = APP_DESKTOP;
        desktop_draw();
        lastPressed = true;
        return;
      }
    }

    if (pressed) {
      bool keepOpen = paint_handleTouch(x, y);
      if (!keepOpen) {
        app = APP_DESKTOP;
        desktop_draw();
        lastPressed = true;
        return;
      }
    }

    lastPressed = pressed;
    return;
  }

  lastPressed = pressed;
}
