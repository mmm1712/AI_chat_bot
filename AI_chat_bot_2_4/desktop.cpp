#include "desktop.h"
#include "wallpaper.h"

#include "paint_icon.h"
#include "trash_icon.h"
#include "internet_icon.h"
#include "notes_icon.h"
#include "wifi_icon.h"

#include <Arduino.h>

static TFT_eSPI* tft = nullptr;

static const int SCREEN_W = 320;
static const int SCREEN_H = 240;

static int aiX      = 24, aiY      = 40;
static int paintX   = 24, paintY   = 100;
static int trashX   = 24, trashY   = 160;
static int netX     = 90, netY     = 40;
static int notesX   = 90, notesY   = 100;

static const int aiW = 40, aiH = 40;

static const int LABEL_FONT  = 2;
static const int LABEL_W     = 96;
static const int LABEL_H     = 22;
static const int LABEL_Y_GAP = 4;
static const int LABEL_PAD   = 2;
static int wifiX    = 90, wifiY    = 160;

enum DragTarget { DRAG_NONE, DRAG_AI, DRAG_PAINT, DRAG_TRASH, DRAG_NET, DRAG_NOTES, DRAG_WIFI };
static void drawLabelXP(const char* label, int cx, int topY, bool selected);

static DragTarget dragTarget = DRAG_NONE;

static int dragOffX = 0, dragOffY = 0;
static int downX = 0, downY = 0;
static bool moved = false;
static bool pressedOnIconBody = false;

static const int DRAG_THRESHOLD = 10;
static const uint32_t HOLD_TO_DRAG_MS = 380;
static uint32_t pressStartMs = 0;

static DragTarget selectedTarget = DRAG_NONE;

static DragTarget lastTapTarget = DRAG_NONE;
static uint32_t lastTapMs = 0;
static const uint32_t DBL_TAP_MS = 450;

static inline void clearDoubleTap() {
  lastTapTarget = DRAG_NONE;
  lastTapMs = 0;
}

enum MenuFor { MENU_NONE, MENU_AI, MENU_PAINT, MENU_TRASH, MENU_NET, MENU_NOTES, MENU_WIFI };

static MenuFor menuFor = MENU_NONE;

static bool menuVisible = false;
static int menuX = 0, menuY = 0;

static const int MENU_W = 150;
static const int MENU_ITEM_H = 20;
static const int MENU_ITEMS = 4;

static bool menuFingerDown = false;
static int menuActiveItem = -1;

static DragTarget forceDragTarget = DRAG_NONE;

static inline bool inRect(int x,int y,int rx,int ry,int rw,int rh){
  return (x>=rx && x<rx+rw && y>=ry && y<ry+rh);
}

static inline bool rectIntersects(int ax,int ay,int aw,int ah, int bx,int by,int bw,int bh){
  return !(ax+aw <= bx || bx+bw <= ax || ay+ah <= by || by+bh <= ay);
}

struct Rect { int x,y,w,h; };

static Rect iconWithLabelRect(int ix,int iy,int iw,int ih) {
  int labelX = ix + iw/2 - LABEL_W/2;
  int labelY = iy + ih + LABEL_Y_GAP;

  int x0 = min(ix, labelX) - LABEL_PAD;
  int y0 = min(iy, labelY) - LABEL_PAD;

  int x1 = max(ix + iw, labelX + LABEL_W) + LABEL_PAD;
  int y1 = max(iy + ih, labelY + LABEL_H) + LABEL_PAD;

  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > SCREEN_W) x1 = SCREEN_W;
  if (y1 > SCREEN_H) y1 = SCREEN_H;

  return { x0, y0, x1 - x0, y1 - y0 };
}

static Rect aiRect()    { return iconWithLabelRect(aiX, aiY, aiW, aiH); }
static Rect paintRect() { return iconWithLabelRect(paintX, paintY, PAINT_ICON_WIDTH, PAINT_ICON_HEIGHT); }
static Rect trashRect() { return iconWithLabelRect(trashX, trashY, TRASH_ICON_WIDTH, TRASH_ICON_HEIGHT); }
static Rect netRect()   { return iconWithLabelRect(netX, netY, INTERNET_ICON_WIDTH, INTERNET_ICON_HEIGHT); }
static Rect notesRect() { return iconWithLabelRect(notesX, notesY, NOTES_ICON_WIDTH, NOTES_ICON_HEIGHT); }
static Rect wifiRect()  { return iconWithLabelRect(wifiX, wifiY, WIFI_ICON_WIDTH, WIFI_ICON_HEIGHT); }

static Rect rectForTarget(DragTarget t) {
  if (t == DRAG_AI)    return aiRect();
  if (t == DRAG_PAINT) return paintRect();
  if (t == DRAG_TRASH) return trashRect();
  if (t == DRAG_NET)   return netRect();
  if (t == DRAG_NOTES) return notesRect();
  if (t == DRAG_WIFI) return wifiRect();

  return {0,0,0,0};
}

static void setSelected(DragTarget t);

static uint16_t wallBuf[200 * 80];
static const int WALLBUF_MAX = (int)(sizeof(wallBuf) / sizeof(wallBuf[0]));

static void redrawWallpaperRect(int x, int y, int w, int h) {
  if (!tft) return;

  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > SCREEN_W) w = SCREEN_W - x;
  if (y + h > SCREEN_H) h = SCREEN_H - y;
  if (w <= 0 || h <= 0) return;

  int pixels = w * h;
  if (pixels > WALLBUF_MAX) {
    tft->setSwapBytes(true);
    tft->pushImage(0, 0, WALLPAPER_WIDTH, WALLPAPER_HEIGHT, wallpaper_map);
    return;
  }

  int idx = 0;
  for (int yy = 0; yy < h; yy++) {
    int srcRow = (y + yy) * SCREEN_W;
    int srcBase = srcRow + x;
    for (int xx = 0; xx < w; xx++) {
      wallBuf[idx++] = wallpaper_map[srcBase + xx];
    }
  }

  tft->setSwapBytes(true);
  tft->pushImage(x, y, w, h, wallBuf);
}

static void drawWifiIcon() {
  bool sel = (selectedTarget == DRAG_WIFI);

  tft->setSwapBytes(true);

  tft->pushImage(wifiX, wifiY, WIFI_ICON_WIDTH, WIFI_ICON_HEIGHT, wifi_icon_map, 0x0000);

  int labelTop = wifiY + WIFI_ICON_HEIGHT + LABEL_Y_GAP;
  drawLabelXP("WiFi", wifiX + WIFI_ICON_WIDTH/2, labelTop, sel);
}

static void drawLabelXP(const char* label, int cx, int topY, bool selected) {

  const uint16_t sel1 = 0x1C9F;
  const uint16_t sel2 = 0x047F;

  int tw = tft->textWidth(label, LABEL_FONT);
  int boxW = tw + 14;
  if (boxW < 34) boxW = 34;
  if (boxW > LABEL_W) boxW = LABEL_W;

  int boxH = 18;
  int x = cx - boxW/2;
  int y = topY + (LABEL_H - boxH)/2;

  if (x < 2) x = 2;
  if (x + boxW > SCREEN_W - 2) x = SCREEN_W - 2 - boxW;

  if (selected) {
    tft->fillRect(x, y, boxW, boxH/2, sel1);
    tft->fillRect(x, y + boxH/2, boxW, boxH - boxH/2, sel2);
    tft->drawRect(x, y, boxW, boxH, TFT_WHITE);

    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(TFT_WHITE, sel2);
    tft->drawString(label, cx, y + boxH/2, LABEL_FONT);
    tft->setTextDatum(TL_DATUM);
  } else {

    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(TFT_BLACK);
    tft->drawString(label, cx+1, topY + LABEL_H/2 + 1, LABEL_FONT);
    tft->setTextColor(TFT_WHITE);
    tft->drawString(label, cx,   topY + LABEL_H/2,     LABEL_FONT);
    tft->setTextDatum(TL_DATUM);
  }
}

static void drawAIIcon() {
  bool sel = (selectedTarget == DRAG_AI);

  tft->fillRoundRect(aiX, aiY, aiW, aiH, 8, TFT_BLUE);
  tft->drawRoundRect(aiX, aiY, aiW, aiH, 8, TFT_WHITE);

  tft->setTextColor(TFT_WHITE, TFT_BLUE);
  tft->drawCentreString("AI", aiX + aiW/2, aiY + 10, 4);

  int labelTop = aiY + aiH + LABEL_Y_GAP;
  drawLabelXP("Chat", aiX + aiW/2, labelTop, sel);
}

static void drawPaintIcon() {
  bool sel = (selectedTarget == DRAG_PAINT);

  tft->setSwapBytes(true);
  tft->pushImage(paintX, paintY, PAINT_ICON_WIDTH, PAINT_ICON_HEIGHT, paint_icon_map, 0x0000);

  int labelTop = paintY + PAINT_ICON_HEIGHT + LABEL_Y_GAP;
  drawLabelXP("Paint", paintX + PAINT_ICON_WIDTH/2, labelTop, sel);
}

static void drawTrashIcon() {
  bool sel = (selectedTarget == DRAG_TRASH);

  tft->setSwapBytes(true);
  tft->pushImage(trashX, trashY, TRASH_ICON_WIDTH, TRASH_ICON_HEIGHT, trash_icon_map, 0x0000);

  int labelTop = trashY + TRASH_ICON_HEIGHT + LABEL_Y_GAP;
  drawLabelXP("Trash", trashX + TRASH_ICON_WIDTH/2, labelTop, sel);
}

static void drawInternetIcon() {
  bool sel = (selectedTarget == DRAG_NET);

  tft->setSwapBytes(true);
  tft->pushImage(netX, netY, INTERNET_ICON_WIDTH, INTERNET_ICON_HEIGHT, internet_icon_map, 0x0000);

  int labelTop = netY + INTERNET_ICON_HEIGHT + LABEL_Y_GAP;
  drawLabelXP("Internet", netX + INTERNET_ICON_WIDTH/2, labelTop, sel);
}

static void drawNotesIcon() {
  bool sel = (selectedTarget == DRAG_NOTES);

  tft->setSwapBytes(true);
  tft->pushImage(notesX, notesY, NOTES_ICON_WIDTH, NOTES_ICON_HEIGHT, notes_icon_map, 0x0000);

  int labelTop = notesY + NOTES_ICON_HEIGHT + LABEL_Y_GAP;
  drawLabelXP("Notes", notesX + NOTES_ICON_WIDTH/2, labelTop, sel);
}

static int menuHeight() { return MENU_ITEM_H * MENU_ITEMS + 6; }

static void menu_draw_xp(int activeItem) {
  menuVisible = true;

  int h = menuHeight();
  if (menuX + MENU_W > SCREEN_W) menuX = SCREEN_W - MENU_W - 2;
  if (menuY + h > SCREEN_H)      menuY = SCREEN_H - h - 2;
  if (menuX < 0) menuX = 0;
  if (menuY < 0) menuY = 0;

  uint16_t bg        = 0xEF7D;
  uint16_t borderDk  = 0x7BEF;
  uint16_t borderLt  = 0xFFFF;
  uint16_t innerDk   = 0xC618;
  uint16_t hlBlue    = 0x1C9F;
  uint16_t hlBlue2   = 0x047F;

  tft->fillRect(menuX, menuY, MENU_W, h, bg);

  tft->drawFastHLine(menuX, menuY, MENU_W, borderLt);
  tft->drawFastVLine(menuX, menuY, h, borderLt);
  tft->drawFastHLine(menuX, menuY + h - 1, MENU_W, borderDk);
  tft->drawFastVLine(menuX + MENU_W - 1, menuY, h, borderDk);

  tft->drawRect(menuX + 1, menuY + 1, MENU_W - 2, h - 2, innerDk);

  const char* items[MENU_ITEMS] = {"Open", "Move", "Properties", "Cancel"};

  for (int i = 0; i < MENU_ITEMS; i++) {
    int ix = menuX + 3;
    int iy = menuY + 3 + i * MENU_ITEM_H;
    int iw = MENU_W - 6;
    int ih = MENU_ITEM_H;

    if (i == activeItem) {
      tft->fillRect(ix, iy, iw, ih/2, hlBlue);
      tft->fillRect(ix, iy + ih/2, iw, ih - ih/2, hlBlue2);
      tft->drawRect(ix, iy, iw, ih, TFT_WHITE);
      tft->setTextColor(TFT_WHITE, hlBlue2);
    } else {
      tft->setTextColor(TFT_BLACK, bg);
    }

    tft->drawString(items[i], menuX + 12, iy + 3, 2);
  }
}

static int menu_hitItem(int x, int y) {
  if (!menuVisible) return -1;
  int h = menuHeight();
  if (!inRect(x, y, menuX, menuY, MENU_W, h)) return -1;

  int relY = y - (menuY + 3);
  int idx = relY / MENU_ITEM_H;
  if (idx < 0 || idx >= MENU_ITEMS) return -1;
  return idx;
}

static void redrawSceneRect(int x,int y,int w,int h) {
  redrawWallpaperRect(x,y,w,h);

  Rect ar = aiRect();
  Rect pr = paintRect();
  Rect tr = trashRect();
  Rect nr = netRect();
  Rect br = notesRect();
  Rect wr = wifiRect();

  if (rectIntersects(x,y,w,h, ar.x,ar.y,ar.w,ar.h)) drawAIIcon();
  if (rectIntersects(x,y,w,h, pr.x,pr.y,pr.w,pr.h)) drawPaintIcon();
  if (rectIntersects(x,y,w,h, tr.x,tr.y,tr.w,tr.h)) drawTrashIcon();
  if (rectIntersects(x,y,w,h, nr.x,nr.y,nr.w,nr.h)) drawInternetIcon();
  if (rectIntersects(x,y,w,h, br.x,br.y,br.w,br.h)) drawNotesIcon();
  if (rectIntersects(x,y,w,h, wr.x,wr.y,wr.w,wr.h)) drawWifiIcon();

  if (menuVisible) {
    int mh = menuHeight();
    if (rectIntersects(x,y,w,h, menuX,menuY, MENU_W,mh)) {
      menu_draw_xp(menuActiveItem);
    }
  }
}

static void menu_hide() {
  if (!menuVisible) return;
  int h = menuHeight();

  menuVisible = false;
  menuFingerDown = false;
  menuActiveItem = -1;
  menuFor = MENU_NONE;

  redrawSceneRect(menuX, menuY, MENU_W, h);
}

static void setSelected(DragTarget t) {
  if (t == selectedTarget) return;

  Rect oldR = rectForTarget(selectedTarget);
  Rect newR = rectForTarget(t);

  selectedTarget = t;

  if (oldR.w > 0) redrawSceneRect(oldR.x, oldR.y, oldR.w, oldR.h);
  if (newR.w > 0) redrawSceneRect(newR.x, newR.y, newR.w, newR.h);
}

void desktop_init(TFT_eSPI* display) {
  tft = display;
}

void desktop_draw() {
  if (!tft) return;

  tft->fillScreen(TFT_BLACK);  // ✅ clears leftover UI like Wi-Fi back button

  tft->setSwapBytes(true);
  tft->pushImage(0, 0, WALLPAPER_WIDTH, WALLPAPER_HEIGHT, wallpaper_map);

  drawAIIcon();
  drawPaintIcon();
  drawTrashIcon();
  drawInternetIcon();
  drawNotesIcon();
  drawWifiIcon();

  if (menuVisible) menu_draw_xp(menuActiveItem);
}


static DesktopAction openForTarget(DragTarget t) {
  if (t == DRAG_AI)    return DESKTOP_OPEN_CHAT;
  if (t == DRAG_PAINT) return DESKTOP_OPEN_PAINT;
  if (t == DRAG_TRASH) return DESKTOP_OPEN_TRASH;
  if (t == DRAG_NET)   return DESKTOP_OPEN_INTERNET;
  if (t == DRAG_NOTES) return DESKTOP_OPEN_NOTES;
  if (t == DRAG_WIFI) return DESKTOP_OPEN_WIFI;

  return DESKTOP_NONE;
}

static DesktopAction propsForMenu(MenuFor mf) {
  if (mf == MENU_AI)    return DESKTOP_PROPERTIES_CHAT;
  if (mf == MENU_PAINT) return DESKTOP_PROPERTIES_PAINT;
  if (mf == MENU_TRASH) return DESKTOP_PROPERTIES_TRASH;
  if (mf == MENU_NET)   return DESKTOP_PROPERTIES_INTERNET;
  if (mf == MENU_NOTES) return DESKTOP_PROPERTIES_NOTES;
  if (mf == MENU_WIFI) return DESKTOP_PROPERTIES_WIFI;

  return DESKTOP_NONE;
}

static DesktopAction openForMenu(MenuFor mf) {
  if (mf == MENU_AI)    return DESKTOP_OPEN_CHAT;
  if (mf == MENU_PAINT) return DESKTOP_OPEN_PAINT;
  if (mf == MENU_TRASH) return DESKTOP_OPEN_TRASH;
  if (mf == MENU_NET)   return DESKTOP_OPEN_INTERNET;
  if (mf == MENU_NOTES) return DESKTOP_OPEN_NOTES;
  if (mf == MENU_WIFI) return DESKTOP_OPEN_WIFI;

  return DESKTOP_NONE;
}

static MenuFor menuForTarget(DragTarget t) {
  if (t == DRAG_AI)    return MENU_AI;
  if (t == DRAG_PAINT) return MENU_PAINT;
  if (t == DRAG_TRASH) return MENU_TRASH;
  if (t == DRAG_NET)   return MENU_NET;
  if (t == DRAG_NOTES) return MENU_NOTES;
  if (t == DRAG_WIFI) return MENU_WIFI;

  return MENU_NONE;
}

static DragTarget hitTestTarget(int x, int y, bool* onIconBody) {
  *onIconBody = false;

  if (inRect(x,y, aiX,aiY,aiW,aiH)) { *onIconBody = true; return DRAG_AI; }
  if (inRect(x,y, paintX,paintY,PAINT_ICON_WIDTH,PAINT_ICON_HEIGHT)) { *onIconBody = true; return DRAG_PAINT; }
  if (inRect(x,y, trashX,trashY,TRASH_ICON_WIDTH,TRASH_ICON_HEIGHT)) { *onIconBody = true; return DRAG_TRASH; }
  if (inRect(x,y, netX,netY,INTERNET_ICON_WIDTH,INTERNET_ICON_HEIGHT)) { *onIconBody = true; return DRAG_NET; }
  if (inRect(x,y, notesX,notesY,NOTES_ICON_WIDTH,NOTES_ICON_HEIGHT)) { *onIconBody = true; return DRAG_NOTES; }
  if (inRect(x,y, wifiX,wifiY,WIFI_ICON_WIDTH,WIFI_ICON_HEIGHT)) { *onIconBody = true; return DRAG_WIFI; }

  Rect ar = aiRect();
  Rect pr = paintRect();
  Rect tr = trashRect();
  Rect nr = netRect();
  Rect br = notesRect();
  Rect wr = wifiRect();

  if (inRect(x,y, ar.x,ar.y,ar.w,ar.h)) return DRAG_AI;
  if (inRect(x,y, pr.x,pr.y,pr.w,pr.h)) return DRAG_PAINT;
  if (inRect(x,y, tr.x,tr.y,tr.w,tr.h)) return DRAG_TRASH;
  if (inRect(x,y, nr.x,nr.y,nr.w,nr.h)) return DRAG_NET;
  if (inRect(x,y, br.x,br.y,br.w,br.h)) return DRAG_NOTES;
  if (inRect(x,y, wr.x,wr.y,wr.w,wr.h)) return DRAG_WIFI;

  return DRAG_NONE;
}

DesktopAction desktop_handleTouch(bool pressed, bool lastPressed, int x, int y) {
  if (!tft) return DESKTOP_NONE;

  if (menuVisible) {
    if (pressed && !lastPressed) {
      menuFingerDown = true;
      menuActiveItem = menu_hitItem(x, y);
      menu_draw_xp(menuActiveItem);

      if (menuActiveItem < 0) {
        menu_hide();
      }
      return DESKTOP_NONE;
    }

    if (pressed && lastPressed && menuFingerDown) {
      int hit = menu_hitItem(x, y);
      if (hit != menuActiveItem) {
        menuActiveItem = hit;
        menu_draw_xp(menuActiveItem);
      }
      return DESKTOP_NONE;
    }

    if (!pressed && lastPressed && menuFingerDown) {
      int item = menuActiveItem;
      MenuFor mf = menuFor;

      menu_hide();

      if (item == 0) return openForMenu(mf);
if (item == 1) {
  if (mf == MENU_AI)    forceDragTarget = DRAG_AI;
  if (mf == MENU_PAINT) forceDragTarget = DRAG_PAINT;
  if (mf == MENU_TRASH) forceDragTarget = DRAG_TRASH;
  if (mf == MENU_NET)   forceDragTarget = DRAG_NET;
  if (mf == MENU_NOTES) forceDragTarget = DRAG_NOTES;
  if (mf == MENU_WIFI)  forceDragTarget = DRAG_WIFI;
  return DESKTOP_NONE;
}

      if (item == 2) return propsForMenu(mf);
      return DESKTOP_NONE;

    }

    return DESKTOP_NONE;
  }

  if (pressed && !lastPressed) {
    downX = x; downY = y;
    moved = false;
    pressStartMs = millis();

    dragTarget = hitTestTarget(x, y, &pressedOnIconBody);

    if (dragTarget == DRAG_NONE) {

      if (selectedTarget != DRAG_NONE) setSelected(DRAG_NONE);
      clearDoubleTap();
    } else {

      setSelected(dragTarget);

      if (pressedOnIconBody) {
        if (dragTarget == DRAG_AI)    { dragOffX = x - aiX;    dragOffY = y - aiY; }
        if (dragTarget == DRAG_PAINT) { dragOffX = x - paintX; dragOffY = y - paintY; }
        if (dragTarget == DRAG_TRASH) { dragOffX = x - trashX; dragOffY = y - trashY; }
        if (dragTarget == DRAG_NET)   { dragOffX = x - netX;   dragOffY = y - netY; }
        if (dragTarget == DRAG_NOTES) { dragOffX = x - notesX; dragOffY = y - notesY; }
        if (dragTarget == DRAG_WIFI) { dragOffX = x - wifiX; dragOffY = y - wifiY; }

      } else {
        dragOffX = 0; dragOffY = 0;
      }
    }

    if (forceDragTarget != DRAG_NONE) {
      if (dragTarget == forceDragTarget) {
        moved = true;
      }
      forceDragTarget = DRAG_NONE;
    }

    return DESKTOP_NONE;
  }

  if (pressed && lastPressed && dragTarget != DRAG_NONE) {

    if (!pressedOnIconBody && !moved) {

      return DESKTOP_NONE;
    }

    int dx = x - downX;
    int dy = y - downY;

    if (!moved && (abs(dx) > DRAG_THRESHOLD || abs(dy) > DRAG_THRESHOLD)) {
      moved = true;
      clearDoubleTap();
    }

    if (!moved && (millis() - pressStartMs >= HOLD_TO_DRAG_MS)) {
      moved = true;
      clearDoubleTap();
    }

    if (moved) {
      Rect oldR = rectForTarget(dragTarget);

      if (dragTarget == DRAG_AI) {
        aiX = constrain(x - dragOffX, 0, SCREEN_W - aiW);
        aiY = constrain(y - dragOffY, 0, SCREEN_H - aiH - LABEL_H);
      } else if (dragTarget == DRAG_PAINT) {
        paintX = constrain(x - dragOffX, 0, SCREEN_W - PAINT_ICON_WIDTH);
        paintY = constrain(y - dragOffY, 0, SCREEN_H - PAINT_ICON_HEIGHT - LABEL_H);
      } else if (dragTarget == DRAG_TRASH) {
        trashX = constrain(x - dragOffX, 0, SCREEN_W - TRASH_ICON_WIDTH);
        trashY = constrain(y - dragOffY, 0, SCREEN_H - TRASH_ICON_HEIGHT - LABEL_H);
      } else if (dragTarget == DRAG_NET) {
        netX = constrain(x - dragOffX, 0, SCREEN_W - INTERNET_ICON_WIDTH);
        netY = constrain(y - dragOffY, 0, SCREEN_H - INTERNET_ICON_HEIGHT - LABEL_H);
      } else if (dragTarget == DRAG_NOTES) {
        notesX = constrain(x - dragOffX, 0, SCREEN_W - NOTES_ICON_WIDTH);
        notesY = constrain(y - dragOffY, 0, SCREEN_H - NOTES_ICON_HEIGHT - LABEL_H);
      } else if (dragTarget == DRAG_WIFI) {
  wifiX = constrain(x - dragOffX, 0, SCREEN_W - WIFI_ICON_WIDTH);
  wifiY = constrain(y - dragOffY, 0, SCREEN_H - WIFI_ICON_HEIGHT - LABEL_H);
}

      Rect newR = rectForTarget(dragTarget);

      redrawSceneRect(oldR.x, oldR.y, oldR.w, oldR.h);
      redrawSceneRect(newR.x, newR.y, newR.w, newR.h);
    }

    return DESKTOP_NONE;
  }

if (!pressed && lastPressed) {
  Serial.printf("UP x=%d y=%d dragTarget=%d moved=%d held=%lu\n",
                x, y, (int)dragTarget, (int)moved, (unsigned long)(millis() - pressStartMs));

    if (dragTarget != DRAG_NONE) {
      uint32_t heldMs = millis() - pressStartMs;

if (!moved && heldMs < HOLD_TO_DRAG_MS) {
  DesktopAction act = openForTarget(dragTarget);
  clearDoubleTap();
  dragTarget = DRAG_NONE;
  moved = false;
  return act;
}

      if (!moved && heldMs >= HOLD_TO_DRAG_MS) {
        clearDoubleTap();

        menuX = x;
        menuY = y;
        menuFor = menuForTarget(dragTarget);

        menuFingerDown = false;
        menuActiveItem = -1;
        menuVisible = true;
        menu_draw_xp(menuActiveItem);
      }
    }

    dragTarget = DRAG_NONE;
    moved = false;
    return DESKTOP_NONE;
  }

  return DESKTOP_NONE;
}
