/**
 * @file M5Cardputer_WebRadio.ino
 * @author Aurélio Avanzi
 * @brief https://github.com/cyberwisk/M5Cardputer_WebRadio
 * @version Beta 1.1
 * @date 2026-02-01
 *
 * @modified file CardputerADV_WebRadio_WuSiU.ino
 * @modified by WuSiU
 *
 * @Hardwares: M5Cardputer ADV
 **/

#include "M5Cardputer.h" //1.1.1
#include "CardWifiSetup.h"
#include <Audio.h> //ESP32-audioI2S 3.0.13
#include <Adafruit_NeoPixel.h> //1.15.2

Adafruit_NeoPixel led(1, 21, NEO_GRB + NEO_KHZ800);

#define MAX_STATIONS 20
#define MAX_NAME_LENGTH 30
#define MAX_URL_LENGTH 100
#define I2S_BCK 41
#define I2S_WS 43
#define I2S_DOUT 42
#define FOOTER_HEIGHT 20
#define FOOTER_TEXT "@WuSiU"
void drawFooter();

#define FFT_SIZE 128
#define WAVE_SIZE 320
static uint16_t prev_y[(FFT_SIZE / 2)+1];
static uint16_t peak_y[(FFT_SIZE / 2)+1];
static int header_height = 51;
static bool fft_enabled = false;
static bool fftSimON = true;

const uint8_t brightnessLevels[5] = {0, 20, 100, 200, 255};
uint8_t currentBrightnessIndex = 4; // start od maksymalnej jasności (255)

Audio audio;

int infoXOffset = 0;
unsigned long lastScrollUpdate = 0;
const unsigned long scrollInterval = 10; // czas między przesunięciami 2 lini w ms
char currentInfo[128] = "";
int currentInfoWidth = 0;

class fft_t {
public:
  fft_t() {
    for (int i = 0; i < FFT_SIZE; i++) _data[i] = 0;
  }

  void exec(const int16_t* in) {
    if (fftSimON) {
      for (int i = 0; i < FFT_SIZE; i++) _data[i] = abs(in[i]);
    }
  }

  uint32_t get(size_t index) {
    if (index < FFT_SIZE) return _data[index];
    return 0;
  }

private:
  uint32_t _data[FFT_SIZE];
};

static fft_t fft;
static int16_t raw_data[FFT_SIZE];

static uint32_t bgcolor(int y) {
  auto h = M5Cardputer.Display.height();
  auto dh = h - header_height;
  int v = ((h - y) << 5) / dh;
  if (dh > header_height) {
    int v2 = ((h - y - 1) << 5) / dh;
    if ((v >> 2) != (v2 >> 2)) return 0x666666u;
  }
  return M5Cardputer.Display.color888(v + 2, v, v + 6);
}

struct RadioStation {
  char name[MAX_NAME_LENGTH];
  char url[MAX_URL_LENGTH];
};

const PROGMEM RadioStation defaultStations[] = {
  {"RMF FM", "http://rmfstream1.interia.pl:80/rmf_fm"},
};

RadioStation stations[MAX_STATIONS];
size_t numStations = 0;
size_t curStation = 0;
uint16_t curVolume = 115;


unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_DELAY = 200;

void setupFFT() {
  if (!fft_enabled) return;
  for (int x = 0; x < (FFT_SIZE / 2) + 1; ++x) {
    prev_y[x] = INT16_MAX;
    peak_y[x] = INT16_MAX;
  }
  int display_height = M5Cardputer.Display.height();
  for (int y = header_height; y < display_height - FOOTER_HEIGHT; ++y) {
    M5Cardputer.Display.drawFastHLine(0, y, M5Cardputer.Display.width(), bgcolor(y));
  }
}

void updateFFT() {
  if (!fft_enabled) return;

  static unsigned long lastFFTUpdate = 0;
  if (millis() - lastFFTUpdate < 50) return;
  lastFFTUpdate = millis();

  for (int i = 0; i < FFT_SIZE; i++) {
    raw_data[i] = random(-32000, 32000);
  }

  fft.exec(raw_data);

  size_t bw = M5Cardputer.Display.width() / 30;
  if (bw < 3) bw = 3;

  int32_t dsp_height = M5Cardputer.Display.height();
  int32_t fft_height = dsp_height - header_height - FOOTER_HEIGHT;

  size_t xe = M5Cardputer.Display.width() / bw;
  if (xe > (FFT_SIZE / 2)) xe = (FFT_SIZE / 2);

  uint32_t bar_color[2] = {0x000033u, 0x99AAFFu};

  M5Cardputer.Display.startWrite();

  for (size_t bx = 0; bx <= xe; ++bx) {
    size_t x = bx * bw;

    int32_t f = fft.get(bx) * 3;
    int32_t y = (f * fft_height) >> 17;
    if (y > fft_height) y = fft_height;
    y = dsp_height - FOOTER_HEIGHT - y;

    int32_t py = prev_y[bx];

    if (y == py && peak_y[bx] == y) continue;

    if (y != py) {
      M5Cardputer.Display.fillRect(
        x, y, bw - 1, py - y,
        bar_color[(y < py)]
      );
      prev_y[bx] = y;
    }

    py = peak_y[bx] + ((peak_y[bx] - y) > 5 ? 2 : 1);
    if (py < y) {
      M5Cardputer.Display.writeFastHLine(
        x, py - 1, bw - 1, bgcolor(py - 1)
      );
    } else {
      py = y - 1;
    }

    if (peak_y[bx] != py) {
      peak_y[bx] = py;
      M5Cardputer.Display.writeFastHLine(
        x, py, bw - 1, TFT_WHITE
      );
    }
  }

  M5Cardputer.Display.endWrite();
}


void toggleFFT() {
  fft_enabled = !fft_enabled;
  M5Cardputer.Display.fillRect(
    0,
    header_height,
    M5Cardputer.Display.width(),
    M5Cardputer.Display.height() - header_height - FOOTER_HEIGHT,
    TFT_BLACK
  );

  if (fft_enabled) setupFFT();
}

void updateBatteryDisplay(unsigned long updateInterval) {
  static unsigned long lastUpdate = 0;
  static int lastBatteryLevel = -1;

  if (millis() - lastUpdate < updateInterval) return;
  lastUpdate = millis();

  int batteryLevel = M5.Power.getBatteryLevel();
  batteryLevel = constrain(batteryLevel, 0, 100);

  if (batteryLevel == lastBatteryLevel) return;
  lastBatteryLevel = batteryLevel;

  const int screenW = M5Cardputer.Display.width();
  uint16_t bg = TFT_BLACK;

  uint16_t batteryColor;
  if (batteryLevel <= 20)      batteryColor = TFT_RED;
  else if (batteryLevel <= 50) batteryColor = TFT_YELLOW;
  else                         batteryColor = TFT_GREEN;

  const int batteryW = 18;
  const int batteryH = 8;
  const int spacing  = 4;

  char percentText[6];
  snprintf(percentText, sizeof(percentText), "%d%%", batteryLevel);

  int percentW = M5Cardputer.Display.textWidth(percentText);
  int percentX = screenW - percentW - 2;

  int batteryX = percentX - batteryW - spacing;

  M5Cardputer.Display.fillRect(screenW - 80, 0, 80, 12, bg);

  M5Cardputer.Display.drawRect(batteryX, 2, batteryW, batteryH, TFT_DARKGREY);
  M5Cardputer.Display.fillRect(batteryX + batteryW, 4, 2, 4, TFT_DARKGREY);

  int fillWidth = (batteryLevel * (batteryW - 4)) / 100;
  M5Cardputer.Display.fillRect(batteryX + 2, 4, fillWidth, 4, batteryColor);

  M5Cardputer.Display.setTextColor(TFT_WHITE, bg);
  M5Cardputer.Display.drawString(percentText, percentX, 0);
}




void loadDefaultStations() {
  numStations = std::min(sizeof(defaultStations)/sizeof(defaultStations[0]), static_cast<size_t>(MAX_STATIONS));
  memcpy(stations, defaultStations, sizeof(RadioStation) * numStations);
}

void mergeRadioStations() {
  if (!SD.begin()) {
    led.setPixelColor(0, led.Color(255, 0, 0));
    led.show();
    M5Cardputer.Display.drawString("/station_list.txt ", 20, 30);
    M5Cardputer.Display.drawString("Not found on SD card", 20, 50);
    delay(4000);
    loadDefaultStations();
    M5Cardputer.Display.fillScreen(BLACK);
    drawFooter();
    return;
  }
  File file = SD.open("/station_list.txt");
  if (!file) { loadDefaultStations(); return; }
  numStations = 0;
char line[160];

while (file.available() && numStations < MAX_STATIONS) {
  size_t len = file.readBytesUntil('\n', line, sizeof(line) - 1);
  line[len] = '\0';

  char *comma = strchr(line, ',');
  if (!comma) continue;

  *comma = '\0';
  char *name = line;
  char *url  = comma + 1;

  while (*name == ' ') name++;
  while (*url  == ' ') url++;

  if (*name && *url) {
    strncpy(stations[numStations].name, name, MAX_NAME_LENGTH - 1);
    strncpy(stations[numStations].url,  url,  MAX_URL_LENGTH - 1);

    stations[numStations].name[MAX_NAME_LENGTH - 1] = '\0';
    stations[numStations].url [MAX_URL_LENGTH  - 1] = '\0';

    numStations++;
  }
}

  file.close();
  if (numStations == 0) loadDefaultStations();
  led.setPixelColor(0, led.Color(0, 0, 0));
  led.show();
}

void showStationWithScrolling(const char* stationName, const char* info) {
  fftSimON = true;

  M5Cardputer.Display.fillRect(0, 15, 240, 20, TFT_BLACK);
  M5Cardputer.Display.drawString(stationName, 0, 15);

  strncpy(currentInfo, info, sizeof(currentInfo) - 1);
  currentInfo[sizeof(currentInfo) - 1] = '\0';
  currentInfoWidth = M5Cardputer.Display.textWidth(currentInfo);
  infoXOffset = 0;
  lastScrollUpdate = millis();

  M5Cardputer.Display.fillRect(0, 35, 240, 15, TFT_BLACK);
  M5Cardputer.Display.drawString(currentInfo, infoXOffset, 35);
}

void updateScrolling() {
  if (!currentInfo[0]) return;
  if (currentInfoWidth <= 240) return;

  if (millis() - lastScrollUpdate < scrollInterval) return;
  lastScrollUpdate = millis();

  infoXOffset--;
  if (infoXOffset < -currentInfoWidth) infoXOffset = 240;

  M5Cardputer.Display.fillRect(0, 35, 240, 15, TFT_BLACK);
  M5Cardputer.Display.drawString(currentInfo, infoXOffset, 35);
}



void showStation() {
  showStationWithScrolling(stations[curStation].name, "");
  showVolume();
}

void audio_id3data(const char *info) {
  if (!info || !*info) return;

  strncpy(currentInfo, info, sizeof(currentInfo) - 1);
  currentInfo[sizeof(currentInfo) - 1] = '\0';

  currentInfoWidth = M5Cardputer.Display.textWidth(currentInfo);
  infoXOffset = 240;
  lastScrollUpdate = millis();
}

void Playfile() {
  led.setPixelColor(0, led.Color(255, 0, 0));
  led.show();
  audio.stopSong();
  const char* url = stations[curStation].url;
if (strstr(url, "http")) audio.connecttohost(stations[curStation].url);
  else if (strstr(url, "/mp3")) {
    M5Cardputer.Display.drawString("Play MP3 files from SD card /mp3    ", 0, 15);
    unsigned long start = millis();
    while (millis() - start < 4000) {
      M5Cardputer.update();
      audio.loop();
}
    audio.connecttoFS(SD,stations[curStation].url);
  }
  else audio.connecttospeech("webradio", "en");
  showStation();
}

void volumeUp() {
    if (curVolume + 20 >= 255) curVolume = 255;
    else curVolume += 20;

    audio.setVolume(map(curVolume, 0, 255, 0, 21));
    showVolume();
}

void volumeDown() {
    if (curVolume <= 20) curVolume = 0;
    else curVolume -= 20;

    audio.setVolume(map(curVolume, 0, 255, 0, 21));
    showVolume();
}

void volumeMute() {
    static uint16_t savedVolume = 100;

    if (curVolume > 0) {
        savedVolume = curVolume;
        curVolume = 0;
    } else {
        curVolume = savedVolume;
    }

    audio.setVolume(map(curVolume, 0, 255, 0, 21));
    showVolume();
}

void showVolume() {
    static uint16_t lastVolume = 65535;
    uint16_t currentVolume = curVolume;

    if (currentVolume != lastVolume) {
        lastVolume = currentVolume;

        int barHeight = 8;
        int maxWidth = M5Cardputer.Display.width() - 70;
        M5Cardputer.Display.fillRect(0, 0, maxWidth, barHeight, TFT_BLACK);


        int barWidth = (currentVolume * maxWidth) / 255;

        if (barWidth < 0) barWidth = 0;
        if (barWidth > maxWidth) barWidth = maxWidth;

        if (barWidth > 0) {
           M5Cardputer.Display.fillRect(0, 0, barWidth, barHeight, 0xAAFFAA);
        }
    }
}

void stationUp() {
  if (numStations > 0) { curStation = (curStation + 1) % numStations; audio.stopSong(); Playfile(); showStation(); }
}

void stationDown() {
  if (numStations > 0) { curStation = (curStation - 1 + numStations) % numStations; audio.stopSong(); Playfile(); showStation(); }
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);

  auto spk_cfg = M5Cardputer.Speaker.config();
  spk_cfg.sample_rate = 44100;
  spk_cfg.task_pinned_core = APP_CPU_NUM;
  M5Cardputer.Speaker.config(spk_cfg);
  M5Cardputer.Speaker.begin();
  M5Cardputer.Speaker.setVolume(255);

  led.begin();
  led.setBrightness(255);
  led.show();

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setFont(&fonts::FreeMonoOblique9pt7b);

  connectToWiFi();

  while (WiFi.status() != WL_CONNECTED) {
  delay(100);
  }

  audio.setPinout(I2S_BCK, I2S_WS, I2S_DOUT);
  audio.setVolume(map(curVolume, 0, 255, 0, 21));
  audio.setBalance(0);

  M5Cardputer.Display.fillScreen(BLACK);

  audio.stopSong();
  mergeRadioStations();
  Playfile();
  showStation();
  toggleFFT();

}

void loop() {


  audio.loop();
  M5Cardputer.update();
  updateBatteryDisplay(5000);

  if (M5Cardputer.Keyboard.isChange() && (millis() - lastButtonPress > DEBOUNCE_DELAY)) {
    led.setPixelColor(0, led.Color(120, 0, 255));
    led.show();

    if (M5Cardputer.Keyboard.isKeyPressed(';')) volumeUp();
    else if (M5Cardputer.Keyboard.isKeyPressed('.')) volumeDown();
    else if (M5Cardputer.Keyboard.isKeyPressed('m')) volumeMute();
    else if (M5Cardputer.Keyboard.isKeyPressed('b')) {
        currentBrightnessIndex = (currentBrightnessIndex + 1) % 5;
        static uint8_t lastBrightness = 255;
        uint8_t b = brightnessLevels[currentBrightnessIndex];
        if (b != lastBrightness) {
          M5Cardputer.Display.setBrightness(b);
          lastBrightness = b;
        }
    }
    else if (M5Cardputer.Keyboard.isKeyPressed('/')) stationUp();
    else if (M5Cardputer.Keyboard.isKeyPressed(',')) stationDown();
    else if (M5Cardputer.Keyboard.isKeyPressed('r')) {
      audio.stopSong(); audio.connecttohost(stations[curStation].url);
    }
    // else if (M5Cardputer.Keyboard.isKeyPressed('p')) {
    //   M5Cardputer.Display.fillRect(0, 15, 240, 49, TFT_BLACK);
    //   M5Cardputer.Display.drawString("Los Santos Rock", 0, 15);
    //   audio.stopSong();
    //   audio.connecttoFS(SD,"/mp3/Los Santos Rock Radio.mp3");
    // }
    // else if (M5Cardputer.Keyboard.isKeyPressed('o')) {
    //   M5Cardputer.Display.fillRect(0, 15, 240, 49, TFT_BLACK);
    //   M5Cardputer.Display.drawString("PlayFile", 0, 15);
    //   Playfile();
    // }
    // else if (M5Cardputer.Keyboard.isKeyPressed('s')) {
    //   audio.stopSong();
    //   audio.connecttospeech("webradio.", "en");
    // }
    else if (M5Cardputer.Keyboard.isKeyPressed('f')) toggleFFT();

    lastButtonPress = millis();
  }

  if (fft_enabled) updateFFT();

  updateScrolling();

  drawFooter();

  led.setPixelColor(0, led.Color(0, 0, 0));
  led.show();
}


void audio_showstation(const char *showstation) {
  if (showstation && *showstation) {
    char limitedInfo[25];
    strncpy(limitedInfo, showstation, 24);
    limitedInfo[24] = '\0';
    M5Cardputer.Display.fillRect(0, 15, 240, 15, TFT_BLACK);
    M5Cardputer.Display.drawString(limitedInfo, 0, 15);
    fftSimON = true;
  }
}

void audio_showstreamtitle(const char *info) {
  if (!info || !*info) return;

  strncpy(currentInfo, info, sizeof(currentInfo) - 1);
  currentInfo[sizeof(currentInfo) - 1] = '\0';

  currentInfoWidth = M5Cardputer.Display.textWidth(currentInfo);
  infoXOffset = 240;
  lastScrollUpdate = millis();
}

void drawFooter() {
  static bool drawn = false;
  if (drawn) return;
  drawn = true;

  int screenW = M5Cardputer.Display.width();
  int screenH = M5Cardputer.Display.height();
  int y = screenH - FOOTER_HEIGHT;

  M5Cardputer.Display.fillRect(0, y, screenW, FOOTER_HEIGHT, TFT_BLACK);
  M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);

  int textW = M5Cardputer.Display.textWidth(FOOTER_TEXT);
  int textX = (screenW - textW) / 2;
  int textY = y + FOOTER_HEIGHT - 18;

  M5Cardputer.Display.drawString(FOOTER_TEXT, textX, textY);
}
