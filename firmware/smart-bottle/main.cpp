#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

#define WIFI_SSID       "Wokwi-GUEST"
#define WIFI_PASSWORD   ""
#define ENDPOINT_URL    "https://jameofit-iot.free.beeceptor.com/api/v1/hydration"
#define API_KEY         "jameofit-bottle-key-001"
#define DEVICE_ID       "bottle-jameofit-001"
#define USER_ID         "usr_12345"
#define DAILY_GOAL_ML   2500.0f   

#define FLOW_BTN_PIN    4         
#define NEOPIXEL_PIN    5         
#define NEOPIXEL_COUNT  1
#define SCREEN_W        128
#define SCREEN_H        64
#define OLED_RESET      -1
#define OLED_I2C_ADDR   0x3C
#define SIP_ML          200.0f    
#define DEBOUNCE_MS     300       

Adafruit_SSD1306  display(SCREEN_W, SCREEN_H, &Wire, OLED_RESET);
Adafruit_NeoPixel pixel(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
HTTPClient        http;

float         totalMl      = 0.0f;
int           lastBtnState = HIGH;
unsigned long lastBtnTime  = 0;

void connectWiFi();
void splashScreen();
void drawDisplay();
void refreshLED();
void postHydration(float ml);

void setup() {
  Serial.begin(115200);
  Serial.println("======================================");
  Serial.println("  JameoFit – Botella Inteligente      ");
  Serial.println("  Presiona el boton azul = 1 sorbo    ");
  Serial.println("======================================");

  pinMode(FLOW_BTN_PIN, INPUT_PULLUP);

  pixel.begin();
  pixel.setBrightness(80);
  pixel.setPixelColor(0, pixel.Color(0, 80, 200));
  pixel.show();

  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    Serial.println("[ERROR] OLED SSD1306 no encontrado");
    while (true) delay(1000);
  }
  splashScreen();
  delay(1800);

  connectWiFi();
  drawDisplay();
  refreshLED();

  Serial.println("[OK] Sistema listo. Presiona el boton azul para simular.");
}

void loop() {
  int  currentBtn = digitalRead(FLOW_BTN_PIN);
  unsigned long now = millis();

  if (currentBtn == LOW && lastBtnState == HIGH && (now - lastBtnTime) > DEBOUNCE_MS) {
    lastBtnTime = now;
    totalMl    += SIP_ML;

    float pct = (totalMl / DAILY_GOAL_ML) * 100.0f;
    Serial.printf("[SENSOR] Sorbo detectado | +%.0f mL | Total: %.0f / %.0f mL (%.0f%%)\n",
                  SIP_ML, totalMl, DAILY_GOAL_ML, pct);

    drawDisplay();
    refreshLED();
    postHydration(SIP_ML);
  }

  lastBtnState = currentBtn;
  delay(50);
}

void connectWiFi() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(12, 24);
  display.print("Conectando WiFi...");
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Conectando");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.print("[WiFi] Conectado: ");
  Serial.println(WiFi.localIP());

  display.clearDisplay();
  display.setCursor(30, 18);
  display.print("WiFi OK!");
  display.setCursor(14, 32);
  display.print(WiFi.localIP());
  display.display();
  delay(1200);
}

void postHydration(float ml) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] Sin WiFi – publicacion omitida");
    return;
  }

  JsonDocument doc;
  doc["userId"]   = USER_ID;
  doc["deviceId"] = DEVICE_ID;
  doc["ml"]       = ml;
  doc["totalMl"]  = totalMl;
  doc["goalMl"]   = DAILY_GOAL_ML;
  doc["ts"]       = (unsigned long)millis();

  String body;
  serializeJson(doc, body);

  Serial.println("[HTTP] POST → " ENDPOINT_URL);
  Serial.print("[HTTP] Body: ");
  Serial.println(body);

  http.begin(ENDPOINT_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-Key", API_KEY);

  int code = http.POST(body);

  if (code > 0) {
    Serial.printf("[HTTP] Respuesta: %d\n", code);
    String resp = http.getString();
    if (resp.length() > 0) {
      Serial.print("[HTTP] Body resp: ");
      Serial.println(resp);
    }
  } else {
    Serial.print("[HTTP] Error: ");
    Serial.println(http.errorToString(code).c_str());
  }

  http.end();
}

void drawDisplay() {
  float pct = (totalMl / DAILY_GOAL_ML) * 100.0f;
  if (pct > 100.0f) pct = 100.0f;
  int barLen = (int)((pct / 100.0f) * 108.0f);

  char buf[32];

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print("JameoFit  Botella");
  display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(2, 13);
  snprintf(buf, sizeof(buf), "%.0f mL", totalMl);
  display.print(buf);

  display.setTextSize(1);
  display.setCursor(2, 34);
  snprintf(buf, sizeof(buf), "Meta:%.0fmL  %.0f%%", DAILY_GOAL_ML, pct);
  display.print(buf);

  display.drawRect(9, 44, 110, 9, SSD1306_WHITE);
  if (barLen > 0) display.fillRect(10, 45, barLen, 7, SSD1306_WHITE);

  display.setCursor(2, 56);
  if      (pct >= 100.0f) display.print("Meta cumplida!  ");
  else if (pct >=  80.0f) display.print("Casi ahi!");
  else if (pct >=  40.0f) display.print("Buen ritmo!");
  else                    display.print("Toma agua!");

  display.display();
}

void refreshLED() {
  float pct = (totalMl / DAILY_GOAL_ML) * 100.0f;

  if      (pct >= 80.0f) pixel.setPixelColor(0, pixel.Color(0,   200,  80));
  else if (pct >= 40.0f) pixel.setPixelColor(0, pixel.Color(220, 140,   0));
  else                   pixel.setPixelColor(0, pixel.Color(200,  30,  30));

  pixel.show();
}

void splashScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(18, 4);
  display.print("JF Technologies");

  display.setTextSize(2);
  display.setCursor(8, 18);
  display.print("JameoFit");

  display.setTextSize(1);
  display.setCursor(10, 38);
  display.print("Smart Bottle v1.0");

  display.setCursor(22, 52);
  display.print("Iniciando...");

  display.display();
}