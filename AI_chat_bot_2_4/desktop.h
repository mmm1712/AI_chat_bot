#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

enum DesktopAction {
  DESKTOP_NONE = 0,

  DESKTOP_OPEN_CHAT,
  DESKTOP_OPEN_PAINT,
  DESKTOP_OPEN_TRASH,
  DESKTOP_OPEN_INTERNET,
  DESKTOP_OPEN_NOTES,
  DESKTOP_OPEN_WIFI,

  DESKTOP_PROPERTIES_CHAT,
  DESKTOP_PROPERTIES_PAINT,
  DESKTOP_PROPERTIES_TRASH,
  DESKTOP_PROPERTIES_INTERNET,
  DESKTOP_PROPERTIES_NOTES,
  DESKTOP_PROPERTIES_WIFI
};

void desktop_init(TFT_eSPI* display);
void desktop_draw();

DesktopAction desktop_handleTouch(bool pressed, bool lastPressed, int x, int y);
