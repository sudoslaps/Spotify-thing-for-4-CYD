#ifndef E32R40T_LCD_H
#define E32R40T_LCD_H

#include <TFT_eSPI.h>
#include <JPEGDEC.h>
#include <SPIFFS.h>
#include "spotifyDisplay.h"

// ---- Layout for 320x480 portrait ----------------------------
#define E32_SCREEN_W     320
#define E32_SCREEN_H     480
#define E32_ART_SIZE     200
#define E32_ART_X        ((E32_SCREEN_W - E32_ART_SIZE) / 2)  // 60
#define E32_ART_Y        12
#define E32_TRACK_Y      (E32_ART_Y + E32_ART_SIZE + 20)      // 232
#define E32_ARTIST_Y     (E32_TRACK_Y  + 30)                   // 262
#define E32_ALBUMNAME_Y  (E32_ARTIST_Y + 25)                   // 287
#define E32_PROGRESS_X   10
#define E32_PROGRESS_Y   (E32_ALBUMNAME_Y + 30)                // 317
#define E32_PROGRESS_W   (E32_SCREEN_W - 20)
#define E32_PROGRESS_H   8
#define E32_TIME_Y       (E32_PROGRESS_Y + E32_PROGRESS_H + 10)// 335
#define E32_BTN_Y        (E32_TIME_Y + 42)                     // 377
#define E32_BTN_PREV_X   80
#define E32_BTN_NEXT_X   240
#define E32_BTN_R        32
#define E32_BL_PIN       27
#define E32_TOUCH_CD     500
#define E32_ART_PATH     "/album.jpg"

static TFT_eSPI  e32_tft  = TFT_eSPI();
static JPEGDEC   e32_jpeg;
static File      e32_jpegFile;

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
    e32_tft.pushImage(pDraw->x + E32_ART_X, pDraw->y + E32_ART_Y,
                      pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
    return 1;
}

class E32R40TDisplay : public SpotifyDisplay {
public:

    // Called once at startup with the Spotify object
    void displaySetup(SpotifyArduino *spotifyObj) override {
        spotify_display = spotifyObj;

        pinMode(E32_BL_PIN, OUTPUT);
        digitalWrite(E32_BL_PIN, HIGH);

        e32_tft.init();
        e32_tft.setRotation(0);   // portrait 320x480
        e32_tft.fillScreen(TFT_BLACK);
        e32_tft.setTextColor(TFT_WHITE, TFT_BLACK);
        e32_tft.setTextDatum(MC_DATUM);

        setWidth(E32_SCREEN_W);
        setHeight(E32_SCREEN_H);
        setImageWidth(E32_ART_SIZE);
        setImageHeight(E32_ART_SIZE);

        if (!SPIFFS.begin(true)) {
            Serial.println("[E32R40T] SPIFFS mount failed");
        }
    }

    // Draw the default idle screen
    void showDefaultScreen() override {
        e32_tft.fillScreen(TFT_BLACK);
        e32_tft.setTextDatum(MC_DATUM);
        e32_tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        e32_tft.setTextSize(2);
        e32_tft.drawString("Nothing Playing", E32_SCREEN_W / 2, E32_SCREEN_H / 2);
    }

    // Progress bar + time stamps
    void displayTrackProgress(long progress, long duration) override {
        if (duration <= 0) return;
        long filled = map(progress, 0, duration, 0, E32_PROGRESS_W);

        e32_tft.fillRoundRect(E32_PROGRESS_X, E32_PROGRESS_Y,
                              E32_PROGRESS_W, E32_PROGRESS_H,
                              E32_PROGRESS_H / 2, 0x4208);
        if (filled > 0)
            e32_tft.fillRoundRect(E32_PROGRESS_X, E32_PROGRESS_Y,
                                  filled, E32_PROGRESS_H,
                                  E32_PROGRESS_H / 2, TFT_GREEN);

        e32_tft.fillRect(0, E32_TIME_Y - 6, E32_SCREEN_W, 14, TFT_BLACK);
        e32_tft.setTextSize(1);
        e32_tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        e32_tft.setTextDatum(ML_DATUM);
        e32_tft.drawString(msToTime(progress), E32_PROGRESS_X, E32_TIME_Y);
        e32_tft.setTextDatum(MR_DATUM);
        e32_tft.drawString(msToTime(duration), E32_PROGRESS_X + E32_PROGRESS_W, E32_TIME_Y);
    }

    // Track name, artist, album text
    void printCurrentlyPlayingToScreen(CurrentlyPlaying currentlyPlaying) override {
        e32_tft.fillRect(0, E32_TRACK_Y - 4, E32_SCREEN_W,
                         E32_PROGRESS_Y - (E32_TRACK_Y - 4), TFT_BLACK);

        e32_tft.setTextDatum(MC_DATUM);
        int cx = E32_SCREEN_W / 2;

        e32_tft.setTextSize(1);
        e32_tft.setTextColor(TFT_WHITE, TFT_BLACK);
        e32_tft.setFreeFont(FF6);
        e32_tft.drawString(shrink(currentlyPlaying.trackName, 24), cx, E32_TRACK_Y);

        e32_tft.setFreeFont(FF5);
        e32_tft.setTextColor(TFT_CYAN, TFT_BLACK);
        if (currentlyPlaying.numArtists > 0)
            e32_tft.drawString(shrink(currentlyPlaying.artists[0].artistName, 28), cx, E32_ARTIST_Y);

        e32_tft.setTextColor(0x7BEF, TFT_BLACK);
        e32_tft.drawString(shrink(currentlyPlaying.albumName, 32), cx, E32_ALBUMNAME_Y);

        drawTouchButtons();
    }

    // Touch input check
    void checkForInput() override {
        uint16_t tx, ty;
        if (!e32_tft.getTouch(&tx, &ty)) return;

        unsigned long now = millis();
        if (now - _lastTouch < E32_TOUCH_CD) return;
        _lastTouch = now;

        if (_inCircle(tx, ty, E32_BTN_PREV_X, E32_BTN_Y, E32_BTN_R)) {
            if (spotify_display != nullptr)
                spotify_display->previousTrack();
            flashButton(E32_BTN_PREV_X, false);
        } else if (_inCircle(tx, ty, E32_BTN_NEXT_X, E32_BTN_Y, E32_BTN_R)) {
            if (spotify_display != nullptr)
                spotify_display->nextTrack();
            flashButton(E32_BTN_NEXT_X, true);
        }
    }

    // Clear the album art area
    void clearImage() override {
        e32_tft.fillRect(E32_ART_X, E32_ART_Y, E32_ART_SIZE, E32_ART_SIZE, TFT_BLACK);
    }

    // Called to check if image needs updating – return false to use default behaviour
    bool processImageInfo(CurrentlyPlaying currentlyPlaying) override {
        return false;
    }

    // Render the JPEG that was saved to SPIFFS
    int displayImage() override {
        int rc = e32_jpeg.open(E32_ART_PATH,
                               e32_jpegOpen, e32_jpegClose,
                               e32_jpegRead, e32_jpegSeek,
                               e32_jpegDraw);
        if (rc) {
            e32_jpeg.setPixelType(RGB565_BIG_ENDIAN);
            e32_jpeg.decode(0, 0, JPEG_SCALE_BEST_FIT);
            e32_jpeg.close();
        }
        drawTouchButtons();
        return rc;
    }

    // NFC feedback borders
    void markDisplayAsTagRead() override {
        e32_tft.drawRect(E32_ART_X - 3, E32_ART_Y - 3,
                         E32_ART_SIZE + 6, E32_ART_SIZE + 6, TFT_BLUE);
        e32_tft.drawRect(E32_ART_X - 6, E32_ART_Y - 6,
                         E32_ART_SIZE + 12, E32_ART_SIZE + 12, TFT_RED);
    }

    void markDisplayAsTagWritten() override {
        e32_tft.drawRect(E32_ART_X - 3, E32_ART_Y - 3,
                         E32_ART_SIZE + 6, E32_ART_SIZE + 6, TFT_RED);
        e32_tft.drawRect(E32_ART_X - 6, E32_ART_Y - 6,
                         E32_ART_SIZE + 12, E32_ART_SIZE + 12, TFT_GREEN);
    }

    // WiFi captive portal screen
    void drawWifiManagerMessage(WiFiManager *myWiFiManager) override {
        e32_tft.fillScreen(TFT_BLACK);
        e32_tft.setTextDatum(MC_DATUM);
        int cx = E32_SCREEN_W / 2;

        e32_tft.setTextSize(2);
        e32_tft.setTextColor(TFT_WHITE, TFT_BLACK);
        e32_tft.drawString("WiFi Setup", cx, 60);

        e32_tft.setTextSize(1);
        e32_tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        e32_tft.drawString("Connect to:", cx, 130);
        e32_tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        e32_tft.drawString("SpotifyDiy", cx, 160);
        e32_tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        e32_tft.drawString("Password: thing123", cx, 200);
        e32_tft.drawString("Then open your browser", cx, 260);
        e32_tft.drawString("to configure Spotify", cx, 285);
    }

    // Spotify OAuth screen
    void drawRefreshTokenMessage() override {
        e32_tft.fillScreen(TFT_BLACK);
        e32_tft.setTextDatum(MC_DATUM);
        int cx = E32_SCREEN_W / 2;

        e32_tft.setTextSize(2);
        e32_tft.setTextColor(TFT_WHITE, TFT_BLACK);
        e32_tft.drawString("Spotify Auth", cx, 60);

        e32_tft.setTextSize(1);
        e32_tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        e32_tft.drawString("Visit this address", cx, 150);
        e32_tft.drawString("in your browser:", cx, 175);
        e32_tft.setTextColor(TFT_CYAN, TFT_BLACK);
        e32_tft.drawString("(shown on serial)", cx, 220);
        e32_tft.setTextColor(TFT_WHITE, TFT_BLACK);
        e32_tft.drawString("to authenticate", cx, 280);
    }

private:
    unsigned long _lastTouch = 0;

    bool _inCircle(int tx, int ty, int cx, int cy, int r) {
        int dx = tx - cx, dy = ty - cy;
        return (dx*dx + dy*dy) <= (r*r);
    }

    void drawTouchButtons() {
        drawBtn(E32_BTN_PREV_X, false);
        drawBtn(E32_BTN_NEXT_X, true);
    }

    void drawBtn(int cx, bool isNext) {
        e32_tft.fillCircle(cx, E32_BTN_Y, E32_BTN_R, 0x4208);
        e32_tft.drawCircle(cx, E32_BTN_Y, E32_BTN_R, TFT_DARKGREY);
        if (isNext) {
            e32_tft.fillTriangle(cx+12, E32_BTN_Y, cx-9, E32_BTN_Y-14, cx-9, E32_BTN_Y+14, TFT_WHITE);
            e32_tft.fillRect(cx+13, E32_BTN_Y-13, 4, 26, TFT_WHITE);
        } else {
            e32_tft.fillTriangle(cx-12, E32_BTN_Y, cx+9, E32_BTN_Y-14, cx+9, E32_BTN_Y+14, TFT_WHITE);
            e32_tft.fillRect(cx-17, E32_BTN_Y-13, 4, 26, TFT_WHITE);
        }
    }

    void flashButton(int cx, bool isNext) {
        e32_tft.fillCircle(cx, E32_BTN_Y, E32_BTN_R, TFT_WHITE);
        delay(60);
        drawBtn(cx, isNext);
    }

    String shrink(const char* s, int maxLen) {
        if (!s) return "";
        if ((int)strlen(s) <= maxLen) return String(s);
        return String(s).substring(0, maxLen - 1) + "~";
    }

    String msToTime(long ms) {
        long s = ms / 1000;
        long m = s / 60;
        s %= 60;
        char buf[8];
        snprintf(buf, sizeof(buf), "%ld:%02ld", m, s);
        return String(buf);
    }
};

#endif // E32R40T_LCD_H
