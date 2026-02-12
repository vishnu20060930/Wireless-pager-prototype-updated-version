#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Keypad.h>
#include <U8g2lib.h>

// ---------------- OLED ----------------
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

// ---------------- KEYPAD ----------------
const byte ROWS = 4;
const byte COLS = 3;

char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};

byte rowPins[ROWS] = {D1, D2, D3, D4};
byte colPins[COLS] = {D5, D6, D7};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ---------------- ESP-NOW ----------------
uint8_t broadcastMac[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// ---------------- MULTI-TAP MAP ----------------
const char* keyMap[10] = {
  "", "", "ABC", "DEF", "GHI",
  "JKL", "MNO", "PQRS", "TUV", "WXYZ"
};

// ---------------- BUFFER ----------------
#define MAX_LEN 20
char sendBuffer[MAX_LEN + 1];
int indexPos = 0;

// ---------------- MULTI-TAP STATE ----------------
char lastKey = 0;
int tapIndex = 0;
unsigned long lastTapTime = 0;
const unsigned long multiTapDelay = 800;

// ---------------- MODES ----------------
bool numericMode = false;
unsigned long lastZeroTime = 0;

// ---------------- STAR KEY ----------------
unsigned long lastStarTime = 0;
const unsigned long doubleStarDelay = 600;

// ---------------- OLED FUNCTIONS ----------------
void showStatus() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 12, numericMode ? "NUM MODE" : "TXT MODE");
  u8g2.setFont(u8g2_font_logisoso16_tf);
  u8g2.drawStr(0, 40, sendBuffer);
  u8g2.sendBuffer();
}

void showReady() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 20, "Sender Ready");
  u8g2.sendBuffer();
}

// ---------------- SHORTCUTS ----------------
void applyShortcuts() {
  if (sendBuffer[0] != '0') return;

  if      (strcmp(sendBuffer, "0111") == 0) strcpy(sendBuffer, "HELP");
  else if (strcmp(sendBuffer, "0222") == 0) strcpy(sendBuffer, "EMERGENCY");
  else if (strcmp(sendBuffer, "0333") == 0) strcpy(sendBuffer, "DANGER");
  else if (strcmp(sendBuffer, "0444") == 0) strcpy(sendBuffer, "DOCTOR");
  else if (strcmp(sendBuffer, "0555") == 0) strcpy(sendBuffer, "FIRE");
}

// ---------------- SETUP ----------------
void setup() {
  u8g2.begin();
  showReady();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  wifi_set_channel(1);

  esp_now_init();
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
}

// ---------------- LOOP ----------------
void loop() {
  char key = keypad.getKey();
  if (!key) return;

  unsigned long now = millis();

  // -------- STAR KEY --------
  if (key == '*') {
    if (now - lastStarTime < doubleStarDelay) {
      char resetCmd = '!';
      esp_now_send(broadcastMac, (uint8_t*)&resetCmd, 1);
    }
    indexPos = 0;
    sendBuffer[0] = '\0';
    showReady();
    lastStarTime = now;
    return;
  }

  // -------- SEND --------
  if (key == '#') {
    applyShortcuts();

    for (int i = 0; sendBuffer[i]; i++) {
      esp_now_send(broadcastMac, (uint8_t*)&sendBuffer[i], 1);
      delay(25);
    }

    char endChar = '#';
    esp_now_send(broadcastMac, (uint8_t*)&endChar, 1);

    indexPos = 0;
    sendBuffer[0] = '\0';
    showReady();
    return;
  }

  // -------- ZERO KEY (SPACE / 0 / MODE TOGGLE) --------
  if (key == '0') {

    // Double tap 00 → toggle mode
    if (now - lastZeroTime < 500) {
      numericMode = !numericMode;
      lastZeroTime = 0;
      showStatus();
      return;
    }

    lastZeroTime = now;

    // NUM MODE → digit 0
    if (numericMode) {
      if (indexPos < MAX_LEN) {
        sendBuffer[indexPos++] = '0';
        sendBuffer[indexPos] = '\0';
        showStatus();
      }
      return;
    }

    // TXT MODE → space
    if (!numericMode) {
      if (indexPos < MAX_LEN) {
        sendBuffer[indexPos++] = ' ';
        sendBuffer[indexPos] = '\0';
        showStatus();
      }
      return;
    }
  }

  // -------- NUM MODE DIGITS --------
  if (numericMode && key >= '1' && key <= '9') {
    if (indexPos < MAX_LEN) {
      sendBuffer[indexPos++] = key;
      sendBuffer[indexPos] = '\0';
      showStatus();
    }
    return;
  }

  // -------- TXT MODE --------
  if (!numericMode) {

    // Key 1 always numeric
    if (key == '1') {
      if (indexPos < MAX_LEN) {
        sendBuffer[indexPos++] = '1';
        sendBuffer[indexPos] = '\0';
        showStatus();
      }
      return;
    }

    // Multi-tap letters 2–9
    if (key >= '2' && key <= '9') {
      const char* letters = keyMap[key - '0'];

      if (key == lastKey && (now - lastTapTime) < multiTapDelay) {
        tapIndex = (tapIndex + 1) % strlen(letters);
        sendBuffer[indexPos - 1] = letters[tapIndex];
      } else {
        tapIndex = 0;
        if (indexPos < MAX_LEN) {
          sendBuffer[indexPos++] = letters[0];
          sendBuffer[indexPos] = '\0';
        }
      }

      lastKey = key;
      lastTapTime = now;
      showStatus();
    }
  }
}