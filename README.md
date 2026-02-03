# AI_chat_bot (ESP32 XP style Touch UI)

A retro Windows XP style desktop UI for the ESP32‑2432S024C (240×320 TFT) with capacitive touch.  
Includes a desktop, AI chat over Wi‑Fi, a retro paint app, and a Wikipedia‑style page.

## Features
- XP style desktop with draggable icons
- AI chat app (cloud inference)
- Retro paint app with tools, shapes, and palette
- Wi‑Fi scan/connect app with on‑screen keyboard
- Static Wikipedia‑style “Windows XP” page with local image

## Hardware
- ESP32‑2432S024C (ILI9341, 240×320)
- Capacitive touch (CST820/GT911 depending on board)
- USB power

## 3D Printed Case / Desk
I printed a mini setup for the device using these models:
- Display case: https://www.printables.com/model/1120657-ibm-ps2-color-display-8512-tft-28-inch-640x480-ras  
- Desk: https://www.printables.com/model/1377926-miniatur-schreibtisch-simple-desk-3d-druckbares-mo

## AI Chat Setup (Token stored in NVS)
The AI chat uses a Cloudflare Worker endpoint.  
A token is stored in ESP32 flash (NVS) using `Preferences`.

On first boot:
1. Open Serial Monitor (115200).
2. Paste your token when prompted.

You can also manage the token later:
- `SET_TOKEN <token>`
- `CLEAR_TOKEN`

## How It Works
- Wi‑Fi app scans and connects to 2.4 GHz networks.
- AI requests are sent to a Cloudflare Worker endpoint.
- Responses are trimmed to fit on the small screen.
- The “Wikipedia” app is a static page styled like the real site.

## Build / Upload
1. Install libraries:
   - `TFT_eSPI`
   - `bb_captouch`
2. Configure TFT_eSPI pins for ESP32‑2432S024C.
3. Open `AI_chat_bot_2.4.ino` in Arduino IDE.
4. Upload.

## Notes
- ESP32 supports only 2.4 GHz Wi‑Fi.
- Touch pins can vary by board revision.
- AI chat requires a valid token.

## Photos:
## Screenshots

![Desk setup](screenshots/desk.jpg)
Desk + printed case.

![Boot / welcome screen](AI_project_images/welcome.jpg)
Welcome splash.

![Desktop](AI_project_images/screen.jpg)
Main XP‑style desktop.

![Wi‑Fi scan](AI_project_images/wifi.jpg)
Wi‑Fi scan view.

![Wi‑Fi connected](AI_project_images/wifi_connected.jpg)
Connected state.

![Chat without Wi‑Fi](AI_project_images/chat_nowifi.jpg)
AI chat when Wi‑Fi is not connected.

![Chat with Wi‑Fi](AI_project_images/chat_wifi.jpg)
AI chat over Wi‑Fi.

![Paint app](AI_project_images/paint.jpg)
Retro paint app.

![Wikipedia page](AI_project_images/wikipedia.jpg)
Static Wikipedia‑style page.


