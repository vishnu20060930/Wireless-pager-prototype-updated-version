#include <ESP8266WiFi.h>
#include <espnow.h>
#include <U8g2lib.h>

#define BUZZER D5

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

uint8_t broadcastMac[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

char buffer[21];
int indexPos = 0;

// -------- PASSIVE BUZZER --------
void beep(int durationMs) {
  unsigned long start = millis();
  while (millis() - start < durationMs) {
    digitalWrite(BUZZER, HIGH);
    delayMicroseconds(500);
    digitalWrite(BUZZER, LOW);
    delayMicroseconds(500);
  }
}

void resetPager() {
  indexPos = 0;
  memset(buffer, 0, sizeof(buffer));

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 20, "Pager Ready");
  u8g2.sendBuffer();
}

// -------- RECEIVE --------
void onReceive(uint8_t *mac, uint8_t *data, uint8_t len) {
  char c = (char)data[0];

  // -------- RESET COMMAND --------
  if (c == '!') {
    resetPager();
    return;
  }

  // -------- END OF MESSAGE --------
  if (c == '#') {
    buffer[indexPos] = '\0';

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_logisoso16_tf);
    u8g2.drawStr(0, 40, buffer);
    u8g2.sendBuffer();

    beep(600);
    return;   // NOTE: no auto reset now
  }

  // -------- NORMAL CHAR --------
  if (indexPos < 20) {
    buffer[indexPos++] = c;
    buffer[indexPos] = '\0';

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_logisoso16_tf);
    u8g2.drawStr(0, 40, buffer);
    u8g2.sendBuffer();
  }
}

void setup() {
  pinMode(BUZZER, OUTPUT);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  wifi_set_channel(1);

  esp_now_init();
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_CONTROLLER, 1, NULL, 0);
  esp_now_register_recv_cb(onReceive);

  u8g2.begin();
  resetPager();
}

void loop() {}