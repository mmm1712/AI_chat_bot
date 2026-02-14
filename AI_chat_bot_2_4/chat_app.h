#pragma once
#include <TFT_eSPI.h>

void chat_init(TFT_eSPI* tft);
void chat_draw();

void chat_handleTouch(bool pressed, bool lastPressed, int x, int y);

void chat_release();
