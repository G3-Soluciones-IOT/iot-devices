#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define WIFI_SSID       "Wokwi-GUEST"
#define WIFI_PASSWORD   ""
#define ENDPOINT_URL    "https://jameofit-iot.free.beeceptor.com/api/v1/weight"
#define API_KEY         "jameofit-scale-key-001"
#define DEVICE_ID       "scale-jameofit-001"
#define USER_ID         "usr_12345"

#define WEIGHT_PIN      34    
#define TARE_BTN_PIN    15    

#define SCREEN_W        128
#define SCREEN_H        64
#define OLED_RESET      -1
#define OLED_I2C_ADDR   0x3C

#define MAX_WEIGHT_G    5000.0f   
#define ADC_MAX         4095.0f
#define STABLE_THR      8.0f      
#define STABLE_COUNT_MAX 20       
#define DEBOUNCE_MS     300

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, OLED_RESET);
HTTPClient       http;

float         rawWeight   = 0.0f;
float         netWeight   = 0.0f;
float         tareOffset  = 0.0f;
float         prevWeight  = -999.0f;
int           stableCount = 0;
bool          published   = false;
int           lastBtnState = HIGH;
unsigned long lastBtnTime  = 0;

void  connectWiFi();
void  splashScreen();
void  drawDisplay();
void  postWeight(float grams);
float readWeight();

void setup() {
  Serial.begin(115200);
  pinMode(TARE_BTN_PIN, INPUT_PULLUP);

  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    Serial.println("[ERROR] OLED SSD1306 no encontrado");
    while (true) delay(1000);
  }
  splashScreen();
  delay(1800);

  connectWiFi();

  tareOffset = readWeight();
  Serial.printf("[INIT] Tara inicial: %.1f g\n", tareOffset);

  drawDisplay();
}

void loop() {
  unsigned long now = millis();

  rawWeight = readWeight();
  netWeight = rawWeight - tareOffset;
  if (netWeight < 0.0f) netWeight = 0.0f;

  int btnState = digitalRead(TARE_BTN_PIN);
  if (btnState == LOW && lastBtnState == HIGH && (now - lastBtnTime) > DEBOUNCE_MS) {
    lastBtnTime = now;
    tareOffset  = rawWeight;
    stableCount = 0;
    published   = false;
    Serial.printf("[TARA] Nuevo offset: %.1f g | Peso neto: 0 g\n", tareOffset);
  }
  lastBtnState = btnState;

  if (abs(netWeight - prevWeight) < STABLE_THR) {
    stableCount++;
  } else {
    stableCount = 0;
    published   = false;  
  }
  prevWeight = netWeight;

  if (stableCount >= STABLE_COUNT_MAX && netWeight > 2.0f && !published) {
    Serial.printf("[SENSOR] Peso estable detectado: %.1f g\n", netWeight);
    postWeight(netWeight);
    published   = true;
    stableCount = 0;
  }

  drawDisplay();
  delay(100);
}

float readWeight() {
  int adc = analogRead(WEIGHT_PIN);
  return (adc / ADC_MAX) * MAX_WEIGHT_G;
}

void postWeight(float grams) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] Sin WiFi – publicacion omitida");
    return;
  }

  StaticJsonDocument<200> doc;
  doc["userId"]   = USER_ID;
  doc["deviceId"] = DEVICE_ID;
  doc["grams"]    = grams;
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
    Serial.println(http.errorToString(code));
  }
  http.end();
}

void drawDisplay() {
  char buf[32];

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print("JameoFit  Balanza");
  display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(2, 13);
  snprintf(buf, sizeof(buf), "%.1f g", netWeight);
  display.print(buf);

  display.setTextSize(1);
  display.setCursor(2, 34);
  snprintf(buf, sizeof(buf), "Bruto:%.1fg  Tara:%.1fg", rawWeight, tareOffset);
  display.print(buf);

  int pct = (int)((min(stableCount, STABLE_COUNT_MAX) / (float)STABLE_COUNT_MAX) * 100.0f);
  int barLen = (int)((pct / 100.0f) * 108.0f);
  display.setCursor(2, 44);
  display.print("Estab:");
  display.drawRect(38, 44, 80, 7, SSD1306_WHITE);
  if (barLen > 0) display.fillRect(39, 45, barLen, 5, SSD1306_WHITE);

  display.setCursor(2, 56);
  if (published && netWeight > 2.0f) {
    display.print("Peso enviado OK!");
  } else if (stableCount >= STABLE_COUNT_MAX) {
    display.print("Publicando...");
  } else if (netWeight < 2.0f) {
    display.print("Coloca alimento");
  } else {
    display.print("Detectando peso...");
  }

  display.display();
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
  display.print("Smart Scale v1.0");

  display.setCursor(22, 52);
  display.print("Iniciando...");

  display.display();
}