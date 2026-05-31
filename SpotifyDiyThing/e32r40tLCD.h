// ============================================================
// e32r40tLCD.h  –  Spotify DIY Thing port for the
//                  4.0" ESP32-32E Display Module (E32R40T)
//
// Display:    ST7796S, 320×480, portrait
// Touch:      XPT2046 resistive (via TFT_eSPI built-in driver)
// Author:     Port by Claude – based on cheapYellowLCD.h by
//             Brian Lough (witnessmenow)
//
// HOW TO ACTIVATE:
//   1. Place this file in SpotifyDiyThing/ next to cheapYellowLCD.h
//   2. In SpotifyDiyThing.ino, find the "Display Type" section and
//      add / change to:
//
//        #elif defined(E32R40T_DISPLAY)
//          #include "e32r40tLCD.h"
//          E32R40TDisplay display;
//
//   3. Build with env:e32r40t (see platformio_e32r40t_addition.ini)
// ============================================================

#pragma once

#include <TFT_eSPI.h>
#include <JPEGDEC.h>
#include <SPIFFS.h>
#include "spotifyDisplay.h"   // abstract base – must match original

// ---- Layout constants for 320×480 portrait ------------------
//
//   ┌──────────────────────┐  y=0
//   │   ┌──────────────┐   │  y=ALBUM_Y       (200×200 album art)
//   │   │  album art   │   │
//   │   │  200 × 200   │   │
//   │   └──────────────┘   │  y=ALBUM_Y+200
//   │      Track Name      │  y=TRACK_Y
//   │      Artist Name     │  y=ARTIST_Y
//   │      Album Name      │  y=ALBUMNAME_Y
//   │  ▓▓▓▓▓▓▓▓▓▓░░░░░░░  │  y=PROGRESS_Y  (progress bar)
//   │  0:42          3:31  │  y=TIME_Y
//   │    ◀◀       ▶▶      │  y=BTN_Y       (prev / next)
//   └──────────────────────┘  y=479
//
#define E32_SCREEN_W     320
#define E32_SCREEN_H     480

#define E32_ART_SIZE     200                               // square album art
#define E32_ART_X        ((E32_SCREEN_W - E32_ART_SIZE) / 2)  // 60
#define E32_ART_Y        12

#define E32_TRACK_Y      (E32_ART_Y + E32_ART_SIZE + 20)  // ≈ 232
#define E32_ARTIST_Y     (E32_TRACK_Y  + 30)              // ≈ 262
#define E32_ALBUMNAME_Y  (E32_ARTIST_Y + 25)              // ≈ 287

#define E32_PROGRESS_X   10
#define E32_PROGRESS_Y   (E32_ALBUMNAME_Y + 30)           // ≈ 317
#define E32_PROGRESS_W   (E32_SCREEN_W - 20)              // 300
#define E32_PROGRESS_H   8

#define E32_TIME_Y       (E32_PROGRESS_Y + E32_PROGRESS_H + 10) // ≈ 335

#define E32_BTN_Y        (E32_TIME_Y + 42)                // ≈ 377
#define E32_BTN_PREV_X   80
#define E32_BTN_NEXT_X   240
#define E32_BTN_R        32

// Backlight pin (not handled by TFT_eSPI on this board)
#define E32_BL_PIN       27
#define E32_BL_ON        HIGH

// Touch cooldown (ms) – prevents a single tap triggering many skips
#define E32_TOUCH_CD     500

// Path where album art JPEG is cached in SPIFFS
#define E32_ART_PATH     "/album.jpg"

// ---- Module-level TFT & JPEG objects ------------------------
// (Static so they are not created more than once even if the
//  header is included from multiple translation units.)
static TFT_eSPI   e32_tft  = TFT_eSPI();
static JPEGDEC    e32_jpeg;
static File       e32_jpegFile;

// ---- JPEGDEC callbacks for SPIFFS ---------------------------
static void* e32_jpegOpen(const char* filename, int32_t* size) {
    e32_jpegFile = SPIFFS.open(filename, "r");
    *size = e32_jpegFile.size();
    return &e32_jpegFile;
}

static void e32_jpegClose(void* handle) {
    if (handle) ((File*)handle)->close();
}

static int32_t e32_jpegRead(JPEGFILE* handle, uint8_t* buf, int32_t len) {
    return ((File*)handle->fHandle)->read(buf, len);
}

static int32_t e32_jpegSeek(JPEGFILE* handle, int32_t pos) {
    return ((File*)handle->fHandle)->seek(pos);
}

static int e32_jpegDraw(JPEGDRAW* pDraw) {
    // Offset the decoded pixels to the album art position on screen
    e32_tft.pushImage(
        pDraw->x + E32_ART_X,
        pDraw->y + E32_ART_Y,
        pDraw->iWidth, pDraw->iHeight,
        pDraw->pPixels
    );
    return 1;
}

// ============================================================
class E32R40TDisplay : public SpotifyDisplay {
public:

    // --------------------------------------------------------
    // Initialise hardware
    // --------------------------------------------------------
    void begin() override {
        // Backlight on
        pinMode(E32_BL_PIN, OUTPUT);
        digitalWrite(E32_BL_PIN, E32_BL_ON);

        // Init TFT
        e32_tft.init();
        e32_tft.setRotation(0);          // portrait: 320 wide × 480 tall
        e32_tft.fillScreen(TFT_BLACK);
        e32_tft.setTextColor(TFT_WHITE, TFT_BLACK);
        e32_tft.setTextDatum(MC_DATUM);

        // Init SPIFFS (needed for album-art caching)
        if (!SPIFFS.begin(true)) {
            Serial.println("[E32R40T] SPIFFS mount failed");
        }
    }

    // --------------------------------------------------------
    // Wipe the display and draw the Spotify playback UI
    // Called once on track change.
    // --------------------------------------------------------
    void displayTrackInfo(const char* trackName,
                          const char* artistName,
                          const char* albumName) override {

        // Clear text area only (album art is redrawn separately)
        e32_tft.fillRect(0, E32_TRACK_Y - 4, E32_SCREEN_W,
                         E32_BTN_Y + E32_BTN_R + 10 - (E32_TRACK_Y - 4),
                         TFT_BLACK);

        e32_tft.setTextDatum(MC_DATUM);
        int cx = E32_SCREEN_W / 2;

        // Track name  (bold, medium size)
        e32_tft.setFreeFont(FF6);          // FreeSansBold 9pt – change as desired
        e32_tft.setTextColor(TFT_WHITE, TFT_BLACK);
        e32_tft.drawString(shrink(trackName, 24), cx, E32_TRACK_Y);

        // Artist name
        e32_tft.setFreeFont(FF5);          // FreeSans 9pt
        e32_tft.setTextColor(TFT_CYAN, TFT_BLACK);
        e32_tft.drawString(shrink(artistName, 28), cx, E32_ARTIST_Y);

        // Album name (dimmer)
        e32_tft.setTextColor(0x7BEF /* mid-grey */, TFT_BLACK);
        e32_tft.drawString(shrink(albumName, 32), cx, E32_ALBUMNAME_Y);

        // Draw prev / next touch buttons
        drawTouchButtons();
    }

    // --------------------------------------------------------
    // Update the progress bar and time stamps.
    // progress and duration are both in milliseconds.
    // --------------------------------------------------------
    void displayProgressBar(long progress, long duration) override {
        if (duration <= 0) return;
        long filled = map(progress, 0, duration, 0, E32_PROGRESS_W);

        // Track bar background
        e32_tft.fillRoundRect(E32_PROGRESS_X, E32_PROGRESS_Y,
                              E32_PROGRESS_W, E32_PROGRESS_H,
                              E32_PROGRESS_H / 2, 0x4208 /* dark grey */);
        // Filled portion
        if (filled > 0) {
            e32_tft.fillRoundRect(E32_PROGRESS_X, E32_PROGRESS_Y,
                                  filled, E32_PROGRESS_H,
                                  E32_PROGRESS_H / 2, TFT_GREEN);
        }

        // Time labels
        e32_tft.setFreeFont(NULL);           // built-in 8×8 font
        e32_tft.setTextSize(1);

        // Clear time row first
        e32_tft.fillRect(0, E32_TIME_Y - 6, E32_SCREEN_W, 14, TFT_BLACK);

        e32_tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        e32_tft.setTextDatum(ML_DATUM);
        e32_tft.drawString(msToTime(progress), E32_PROGRESS_X, E32_TIME_Y);
        e32_tft.setTextDatum(MR_DATUM);
        e32_tft.drawString(msToTime(duration),
                           E32_PROGRESS_X + E32_PROGRESS_W, E32_TIME_Y);
    }

    // --------------------------------------------------------
    // Draw the album art that has already been saved to SPIFFS.
    // The main sketch downloads the image; this method just renders it.
    // --------------------------------------------------------
    void drawAlbumArt() override {
        int rc = e32_jpeg.open(E32_ART_PATH,
                               e32_jpegOpen, e32_jpegClose,
                               e32_jpegRead, e32_jpegSeek,
                               e32_jpegDraw);
        if (rc) {
            e32_jpeg.setPixelType(RGB565_BIG_ENDIAN);
            // Scale the image to fit E32_ART_SIZE × E32_ART_SIZE
            e32_jpeg.decode(0, 0, JPEG_SCALE_BEST_FIT);
            e32_jpeg.close();
        } else {
            // No art yet – fill the art area with a placeholder
            e32_tft.fillRect(E32_ART_X, E32_ART_Y,
                             E32_ART_SIZE, E32_ART_SIZE, 0x2104 /* very dark */);
            e32_tft.setTextColor(TFT_DARKGREY, 0x2104);
            e32_tft.setTextDatum(MC_DATUM);
            e32_tft.setFreeFont(NULL);
            e32_tft.drawString("No Art", E32_ART_X + E32_ART_SIZE/2,
                               E32_ART_Y + E32_ART_SIZE/2);
        }
    }

    // --------------------------------------------------------
    // WiFi captive-portal setup screen
    // --------------------------------------------------------
    void displayWifiConfig(const char* ssid, const char* password) override {
        e32_tft.fillScreen(TFT_BLACK);
        e32_tft.setTextDatum(MC_DATUM);
        int cx = E32_SCREEN_W / 2;

        e32_tft.setFreeFont(FF6);
        e32_tft.setTextColor(TFT_WHITE, TFT_BLACK);
        e32_tft.drawString("WiFi Setup", cx, 60);

        e32_tft.setFreeFont(FF5);
        e32_tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        e32_tft.drawString("Connect to:", cx, 120);
        e32_tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        e32_tft.drawString(ssid,         cx, 155);
        e32_tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        e32_tft.drawString("Password:",  cx, 205);
        e32_tft.setTextColor(TFT_CYAN,   TFT_BLACK);
        e32_tft.drawString(password,     cx, 240);
        e32_tft.setTextColor(TFT_WHITE,  TFT_BLACK);
        e32_tft.drawString("Open your browser",  cx, 305);
        e32_tft.drawString("to finish setup",    cx, 335);
    }

    // --------------------------------------------------------
    // Spotify OAuth / token screen
    // --------------------------------------------------------
    void displayRefreshToken(const char* authUrl,
                             const char* callbackUrl) override {
        e32_tft.fillScreen(TFT_BLACK);
        e32_tft.setTextDatum(MC_DATUM);
        int cx = E32_SCREEN_W / 2;

        e32_tft.setFreeFont(FF6);
        e32_tft.setTextColor(TFT_WHITE, TFT_BLACK);
        e32_tft.drawString("Spotify Auth", cx, 55);

        e32_tft.setFreeFont(FF5);
        e32_tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        e32_tft.drawString("1. Add callback URI:",  cx, 115);
        e32_tft.setTextColor(TFT_GREEN,     TFT_BLACK);
        // The URL can be long – show up to ~32 chars, wrap manually if needed
        e32_tft.drawString(shrink(callbackUrl, 32),  cx, 148);

        e32_tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        e32_tft.drawString("2. Visit this URL:",    cx, 205);
        e32_tft.setTextColor(TFT_CYAN,      TFT_BLACK);
        e32_tft.drawString(shrink(authUrl, 32),      cx, 238);

        e32_tft.setTextColor(TFT_WHITE,     TFT_BLACK);
        e32_tft.drawString("3. Grant permission",   cx, 305);
        e32_tft.drawString("& you're done!",        cx, 335);
    }

    // --------------------------------------------------------
    // Generic status message (loading, error, etc.)
    // --------------------------------------------------------
    void displayMessage(const char* message) override {
        e32_tft.fillScreen(TFT_BLACK);
        e32_tft.setTextColor(TFT_WHITE, TFT_BLACK);
        e32_tft.setTextDatum(MC_DATUM);
        e32_tft.setFreeFont(FF5);
        e32_tft.drawString(message, E32_SCREEN_W / 2, E32_SCREEN_H / 2);
    }

    // --------------------------------------------------------
    // NFC feedback – coloured border around album art
    // --------------------------------------------------------
    void nfcTagRead() override {
        e32_tft.drawRect(E32_ART_X - 3, E32_ART_Y - 3,
                         E32_ART_SIZE + 6, E32_ART_SIZE + 6, TFT_BLUE);
        e32_tft.drawRect(E32_ART_X - 6, E32_ART_Y - 6,
                         E32_ART_SIZE + 12, E32_ART_SIZE + 12, TFT_RED);
    }

    void nfcTagWritten() override {
        e32_tft.drawRect(E32_ART_X - 3, E32_ART_Y - 3,
                         E32_ART_SIZE + 6, E32_ART_SIZE + 6, TFT_RED);
        e32_tft.drawRect(E32_ART_X - 6, E32_ART_Y - 6,
                         E32_ART_SIZE + 12, E32_ART_SIZE + 12, TFT_GREEN);
    }

    // --------------------------------------------------------
    // Touch input – returns: -1 = prev, 0 = none, 1 = next
    // --------------------------------------------------------
    int checkForInput() override {
        uint16_t tx, ty;
        if (!e32_tft.getTouch(&tx, &ty)) return 0;

        unsigned long now = millis();
        if (now - _lastTouch < E32_TOUCH_CD) return 0;
        _lastTouch = now;

        // Previous track – left button area
        if (_inCircle(tx, ty, E32_BTN_PREV_X, E32_BTN_Y, E32_BTN_R)) {
            _flashButton(E32_BTN_PREV_X, E32_BTN_Y, false);
            return -1;
        }
        // Next track – right button area
        if (_inCircle(tx, ty, E32_BTN_NEXT_X, E32_BTN_Y, E32_BTN_R)) {
            _flashButton(E32_BTN_NEXT_X, E32_BTN_Y, true);
            return 1;
        }
        return 0;
    }

    // --------------------------------------------------------
    // Draw the Prev / Next control buttons.
    // Called once when the UI is first built; also called after
    // album art is rendered so the buttons stay on top.
    // --------------------------------------------------------
    void drawTouchButtons() override {
        _drawBtn(E32_BTN_PREV_X, E32_BTN_Y, false);
        _drawBtn(E32_BTN_NEXT_X, E32_BTN_Y, true);
    }

    // --------------------------------------------------------
    // SD-card pins used for the optional NFC reader via SD sniffer
    // (same wiring as CYD: CS=5, MOSI=23, CLK=18, MISO=19)
    // --------------------------------------------------------
    int getNfcSpiCsPin()   override { return 5;  }
    int getNfcSpiMosiPin() override { return 23; }
    int getNfcSpiClkPin()  override { return 18; }
    int getNfcSpiMisoPin() override { return 19; }

private:
    unsigned long _lastTouch = 0;

    // True if touch point (tx,ty) is within radius r of centre (cx,cy)
    bool _inCircle(int tx, int ty, int cx, int cy, int r) {
        int dx = tx - cx, dy = ty - cy;
        return (dx*dx + dy*dy) <= (r*r);
    }

    // Draw a circular transport button with a play/skip icon
    void _drawBtn(int cx, int cy, bool isNext) {
        e32_tft.fillCircle(cx, cy, E32_BTN_R, 0x4208 /* dark grey */);
        e32_tft.drawCircle(cx, cy, E32_BTN_R, TFT_DARKGREY);

        if (isNext) {
            // Right-pointing filled triangle
            e32_tft.fillTriangle(
                cx + 12, cy,
                cx -  9, cy - 14,
                cx -  9, cy + 14,
                TFT_WHITE);
            // Vertical bar (the "skip" line)
            e32_tft.fillRect(cx + 13, cy - 13, 4, 26, TFT_WHITE);
        } else {
            // Left-pointing triangle
            e32_tft.fillTriangle(
                cx - 12, cy,
                cx +  9, cy - 14,
                cx +  9, cy + 14,
                TFT_WHITE);
            e32_tft.fillRect(cx - 17, cy - 13, 4, 26, TFT_WHITE);
        }
    }

    // Flash a button on tap for tactile feedback
    void _flashButton(int cx, int cy, bool isNext) {
        e32_tft.fillCircle(cx, cy, E32_BTN_R, TFT_WHITE);
        delay(60);
        _drawBtn(cx, cy, isNext);
    }

    // Truncate a string to maxLen chars, appending "…" if clipped
    String shrink(const char* s, int maxLen) {
        if ((int)strlen(s) <= maxLen) return String(s);
        return String(s).substring(0, maxLen - 1) + "\xc9";
    }

    // Format milliseconds as "M:SS"
    String msToTime(long ms) {
        long s = ms / 1000;
        long m = s / 60;
        s %= 60;
        char buf[8];
        snprintf(buf, sizeof(buf), "%ld:%02ld", m, s);
        return String(buf);
    }
};
