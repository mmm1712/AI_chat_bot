#include "chat_app.h"
#include "keyboard.h"
#include "ai_client.h"
#include <Arduino.h>

static TFT_eSPI* tft = nullptr;

#define SCREEN_W 320
#define SCREEN_H 240

#define MAX_MSG 12
#define MAX_LEN 400

static char chatUser[MAX_MSG][MAX_LEN];
static char chatAI[MAX_MSG][MAX_LEN];
static int chatCount = 0;

// Full width chat
static const int RIGHT_PANEL_X = 320;

static int CHAT_TOP = 32;
static int CHAT_BOTTOM = 110;
static int INPUT_Y = 118;
static const int INPUT_H = 28;

static int chatCursorY = 40;
static bool kbVisible = true;
static const int UI_GAP = 6;

static int scrollLine = 0;
static int totalLines = 0;
static int visibleLines = 0;

static const int CHAT_X0 = 8;
static const int CHAT_X1 = RIGHT_PANEL_X - 4;
static const int LINE_H  = 16;

static bool draggingChat = false;
static int dragStartY = 0;
static int dragAccum = 0;

static inline bool inRect(int x,int y,int rx,int ry,int rw,int rh){
  return x>=rx && x<=rx+rw && y>=ry && y<=ry+rh;
}

static bool inChatArea(int x, int y) {
  return (x < RIGHT_PANEL_X) && (y >= CHAT_TOP) && (y <= CHAT_BOTTOM);
}

static void pushMessage(const char* user, const char* ai) {
  if (chatCount >= MAX_MSG) {
    for (int i = 1; i < MAX_MSG; i++) {
      strncpy(chatUser[i-1], chatUser[i], MAX_LEN);
      strncpy(chatAI[i-1],   chatAI[i],   MAX_LEN);
    }
    chatCount = MAX_MSG - 1;
  }

  strncpy(chatUser[chatCount], user, MAX_LEN - 1);
  chatUser[chatCount][MAX_LEN - 1] = 0;

  strncpy(chatAI[chatCount], ai, MAX_LEN - 1);
  chatAI[chatCount][MAX_LEN - 1] = 0;

  chatCount++;
}

static void applyLayout() {
  if (kbVisible) {
    INPUT_Y = KB_Y - INPUT_H - UI_GAP;
    CHAT_BOTTOM = INPUT_Y - UI_GAP;
  } else {
    INPUT_Y = SCREEN_H - INPUT_H - UI_GAP;
    CHAT_BOTTOM = INPUT_Y - UI_GAP;
  }
}

static void drawBackButton(bool pressed) {
  const int x = 260, y = 6, w = 52, h = 17;
  uint16_t bg = pressed ? TFT_DARKGREY : TFT_LIGHTGREY;

  tft->fillRoundRect(x, y, w, h, 2, bg);
  tft->drawRoundRect(x, y, w, h, 2, TFT_BLACK);
  tft->setTextColor(TFT_BLACK, bg);
  tft->drawCentreString("BACK", x + w / 2, y + 4, 1);
}

static void drawHeader() {
  tft->fillRect(0, 0, 320, 28, TFT_BLUE);
  tft->setTextColor(TFT_WHITE, TFT_BLUE);
  tft->drawString("AI Chat", 10, 6, 2);
  drawBackButton(false);
}

static void clearChatArea() {
  tft->fillRect(0, CHAT_TOP, RIGHT_PANEL_X, CHAT_BOTTOM - CHAT_TOP, TFT_WHITE);
  chatCursorY = CHAT_TOP + 8;
}

static void drawInputBar() {
  tft->drawRect(4, INPUT_Y, 240, INPUT_H, TFT_BLACK);

  tft->setTextColor(TFT_BLACK, TFT_WHITE);
  tft->drawString(">", 8, INPUT_Y + 6, 2);

  tft->fillRoundRect(250, INPUT_Y, 66, INPUT_H, 6, TFT_GREEN);
  tft->setTextColor(TFT_WHITE, TFT_GREEN);
  tft->drawCentreString("SEND", 283, INPUT_Y + 6, 2);

  tft->fillRoundRect(250, INPUT_Y - 24, 66, 20, 6, TFT_LIGHTGREY);
  tft->setTextColor(TFT_BLACK, TFT_LIGHTGREY);
  tft->drawCentreString(kbVisible ? "HIDE" : "SHOW", 283, INPUT_Y - 20, 2);
}

static void drawInputTextClipped(const String& s) {
  int maxW = 210;
  String out = s;

  while (out.length() > 0 && tft->textWidth(out, 2) > maxW) {
    out.remove(0, 1);
  }

  tft->setTextColor(TFT_BLACK, TFT_WHITE);
  tft->drawString(out, 22, INPUT_Y + 6, 2);
}

static void updateInputText() {
  tft->fillRect(20, INPUT_Y + 2, 210, INPUT_H - 4, TFT_WHITE);
  drawInputTextClipped(keyboard_get_text());
}

// ============================================================
// Word wrapping (clean word boundaries)
// ============================================================
#define MAX_LINES 200
static String wrapBuffer[MAX_LINES];
static int wrapCount = 0;

static void wrapLines(const String& s, int maxW) {
  wrapCount = 0;
  String line = "";
  String word = "";

  auto pushLine = [&](const String& ln) {
    if (wrapCount < MAX_LINES) {
      wrapBuffer[wrapCount++] = ln;
    }
  };

  auto flushWord = [&]() {
    if (word.length() == 0) return;

    if (line.length() == 0 && tft->textWidth(word, 2) > maxW) {
      // Force split a very long word
      String chunk = "";
      for (int i = 0; i < (int)word.length(); i++) {
        String test = chunk + word[i];
        if (tft->textWidth(test, 2) > maxW) {
          pushLine(chunk);
          chunk = "";
        }
        chunk += word[i];
      }
      if (chunk.length() > 0) pushLine(chunk);
      word = "";
      return;
    }

    String test = (line.length() == 0) ? word : (line + " " + word);

    if (tft->textWidth(test, 2) <= maxW) {
      line = test;
    } else {
      if (line.length() > 0) pushLine(line);
      line = word;
    }

    word = "";
  };

  for (int i = 0; i < (int)s.length(); i++) {
    char c = s[i];

    if (c == '\n') {
      flushWord();
      if (line.length() > 0) pushLine(line);
      line = "";
      continue;
    }

    if (c == ' ') {
      flushWord();
      continue;
    }

    word += c;
  }

  flushWord();
  if (line.length() > 0) pushLine(line);
}

static int wrapAndCountLines(const String& s, int maxW) {
  wrapLines(s, maxW);
  return max(1, wrapCount);
}

static void drawWrappedLineWindow(const String& s, int maxW,
                                  int firstLineToDraw, int lastLineToDraw,
                                  int &currentLineIndex, int &y) {
  wrapLines(s, maxW);

  for (int i = 0; i < wrapCount; i++) {
    if (currentLineIndex >= firstLineToDraw && currentLineIndex < lastLineToDraw) {
      tft->drawString(wrapBuffer[i], CHAT_X0, y, 2);
      y += LINE_H;
    }
    currentLineIndex++;
  }
}

static void drawChatHistory() {
  clearChatArea();

  int maxW = CHAT_X1 - CHAT_X0;
  visibleLines = (CHAT_BOTTOM - chatCursorY) / LINE_H;

  totalLines = 0;
  for (int i = 0; i < chatCount; i++) {
    String u = String("You: ") + chatUser[i];
    String a = String("AI:  ") + chatAI[i];
    totalLines += wrapAndCountLines(u, maxW);
    totalLines += wrapAndCountLines(a, maxW);
  }

  int maxScroll = max(0, totalLines - visibleLines);
  scrollLine = constrain(scrollLine, 0, maxScroll);

  tft->setTextColor(TFT_BLACK, TFT_WHITE);

  int first = scrollLine;
  int last  = scrollLine + visibleLines;

  int currentLine = 0;
  int y = chatCursorY;

  for (int i = 0; i < chatCount; i++) {
    String u = String("You: ") + chatUser[i];
    String a = String("AI:  ") + chatAI[i];

    drawWrappedLineWindow(u, maxW, first, last, currentLine, y);
    drawWrappedLineWindow(a, maxW, first, last, currentLine, y);

    if (currentLine >= last) break;
  }
}

void chat_init(TFT_eSPI* display) {
  tft = display;

  kbVisible = true;
}

void chat_draw() {
  applyLayout();

  tft->fillScreen(TFT_WHITE);
  drawHeader();
  drawChatHistory();
  drawInputBar();
  updateInputText();

  keyboard_set_visible(kbVisible);
  if (kbVisible) keyboard_draw();
}

void chat_release() {
  keyboard_release();
  draggingChat = false;
  dragAccum = 0;
}

void chat_handleTouch(bool pressed, bool lastPressed, int x, int y) {
  if (!tft) return;

  if (pressed && kbVisible) {
    KB_Action tickA = keyboard_tick(true, x, y);
    if (tickA == KB_CHANGED) {
      updateInputText();
      return;
    }
  }

  if (pressed && !lastPressed && inRect(x, y, 260, 4, 52, 17)) {
    return;
  }

  if (pressed && !lastPressed && inRect(x, y, 250, INPUT_Y - 24, 66, 20)) {
    kbVisible = !kbVisible;
    chat_draw();
    return;
  }

  if (pressed && !lastPressed && !kbVisible && inRect(x, y, 4, INPUT_Y, 240, INPUT_H)) {
    kbVisible = true;
    chat_draw();
    return;
  }

  if (pressed && inChatArea(x, y)) {
    if (!draggingChat) {
      draggingChat = true;
      dragStartY = y;
      dragAccum = 0;
      return;
    }

    int dy = y - dragStartY;
    dragStartY = y;
    dragAccum += dy;

    if (abs(dragAccum) >= LINE_H) {
      int steps = dragAccum / LINE_H;
      dragAccum -= steps * LINE_H;

      int maxScroll = max(0, totalLines - visibleLines);
      scrollLine = constrain(scrollLine - steps, 0, maxScroll);

      drawChatHistory();
      drawInputBar();
      updateInputText();
    }
    return;
  }

  if (pressed && !lastPressed && inRect(x, y, 250, INPUT_Y, 66, INPUT_H)) {
    String userText = keyboard_get_text();
    userText.trim();

    if (userText.length() > 0) {
      String aiText = ai_sendMessage(userText);
      pushMessage(userText.c_str(), aiText.c_str());
      keyboard_clear();

      scrollLine = 999999;
      drawChatHistory();
      drawInputBar();
      updateInputText();
    }
    return;
  }

  if (kbVisible && pressed && !lastPressed) {
    KB_Action a = keyboard_touch(x, y);

    if (a == KB_CHANGED) {
      updateInputText();
    } else if (a == KB_REDRAW) {
      keyboard_draw();
    } else if (a == KB_HIDE) {
      kbVisible = false;
      chat_draw();
    }
    return;
  }
}
