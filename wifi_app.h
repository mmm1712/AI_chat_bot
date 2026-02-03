#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

void wifi_app_init(TFT_eSPI* display);

bool wifi_app_autoconnect(uint32_t timeoutMs = 2500);

void wifi_app_open();

void wifi_app_tick();

bool wifi_app_handleTouch(bool pressed, bool lastPressed, int x, int y);

void wifi_app_forget_saved();
