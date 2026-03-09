/*
 * ESP8266 Kitchen LCD Client
 * Polls backend for new orders and displays them on I2C LCD.
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// WiFi config
const char* WIFI_SSID = "Rika";
const char* WIFI_PASSWORD = "230605SS";

// Backend config (use LAN IP of your backend machine)
const char* BACKEND_HOST = "http://192.168.1.50:8080";

// LCD config
#define LCD_COLUMNS 16
#define LCD_ROWS 2
#define SDA_PIN 4   // D2
#define SCL_PIN 5   // D1

#define BUZZER_PIN 12
#define LED_PIN 2

LiquidCrystal_I2C* lcd = nullptr;

struct KitchenOrder {
  String orderId;
  String table;
  String items[20];
  int qty[20];
  int count;
  int total;
  String time;
  bool valid;
} currentOrder;

unsigned long lastPollMs = 0;
const unsigned long POLL_INTERVAL_MS = 2500;

unsigned long lastScrollMs = 0;
const unsigned long SCROLL_INTERVAL_MS = 700;
int scrollIndex = 0;
bool scrolling = false;

uint8_t detectLCDAddress() {
  uint8_t addresses[] = {0x27, 0x3F};
  for (uint8_t i = 0; i < 2; i++) {
    Wire.beginTransmission(addresses[i]);
    if (Wire.endTransmission() == 0) return addresses[i];
  }
  return 0x27;
}

void ledOn() { digitalWrite(LED_PIN, LOW); }
void ledOff() { digitalWrite(LED_PIN, HIGH); }

void beepAlert() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(120);
    digitalWrite(BUZZER_PIN, LOW);
    delay(120);
  }
}

void showReady() {
  lcd->clear();
  lcd->setCursor(0, 0);
  lcd->print("Ubico D.Trans");
  lcd->setCursor(0, 1);
  lcd->print("Waiting Orders");
}

void showCurrentItem() {
  lcd->clear();
  lcd->setCursor(0, 0);
  lcd->print("Tbl ");
  lcd->print(currentOrder.table);
  lcd->print(" ");
  lcd->print(scrollIndex + 1);
  lcd->print("/");
  lcd->print(currentOrder.count);

  lcd->setCursor(0, 1);
  String line = currentOrder.items[scrollIndex] + " x" + String(currentOrder.qty[scrollIndex]);
  lcd->print(line.substring(0, 16));
}

void displayOrder() {
  scrollIndex = 0;
  if (currentOrder.count <= 1) {
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print("Table ");
    lcd->print(currentOrder.table);
    lcd->setCursor(0, 1);
    String line = currentOrder.items[0] + " x" + String(currentOrder.qty[0]);
    lcd->print(line.substring(0, 16));
    scrolling = false;
  } else {
    showCurrentItem();
    scrolling = true;
  }
  ledOn();
  beepAlert();
}

void ackOrder(const String& orderId, const String& status) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;
  String url = String(BACKEND_HOST) + "/api/esp8266/ack";

  if (!http.begin(client, url)) return;

  http.addHeader("Content-Type", "application/json");
  String payload = "{\"orderId\":\"" + orderId + "\",\"status\":\"" + status + "\"}";
  http.POST(payload);
  http.end();
}

bool parseOrderPayload(const String& json) {
  DynamicJsonDocument doc(3072);
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.print("JSON parse error: ");
    Serial.println(err.c_str());
    return false;
  }

  if (!doc.containsKey("orderId") || !doc.containsKey("items")) return false;

  currentOrder.orderId = doc["orderId"].as<String>();
  currentOrder.table = doc["table"].as<String>();
  currentOrder.total = doc["total"] | 0;
  currentOrder.time = doc["time"].as<String>();
  currentOrder.count = 0;

  JsonArray items = doc["items"].as<JsonArray>();
  for (JsonVariant v : items) {
    if (currentOrder.count >= 20) break;
    currentOrder.items[currentOrder.count] = v["name"].as<String>();
    currentOrder.qty[currentOrder.count] = v["qty"] | 0;
    currentOrder.count++;
  }

  currentOrder.valid = currentOrder.count > 0;
  return currentOrder.valid;
}

void pollBackendForOrders() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;
  String url = String(BACKEND_HOST) + "/api/esp8266/next-order";

  if (!http.begin(client, url)) {
    Serial.println("Failed to begin HTTP request");
    return;
  }

  int code = http.GET();
  if (code == 204) {
    http.end();
    return;
  }

  if (code == 200) {
    String payload = http.getString();
    if (parseOrderPayload(payload)) {
      Serial.print("New order: ");
      Serial.println(currentOrder.orderId);
      displayOrder();
      ackOrder(currentOrder.orderId, "displayed");
    }
  } else {
    Serial.print("Backend poll failed, status: ");
    Serial.println(code);
  }

  http.end();
}

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lcd->clear();
  lcd->setCursor(0, 0);
  lcd->print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("WiFi connected: ");
  Serial.println(WiFi.localIP());

  lcd->clear();
  lcd->setCursor(0, 0);
  lcd->print("WiFi Connected");
  lcd->setCursor(0, 1);
  lcd->print(WiFi.localIP().toString().substring(0, 16));
  delay(1500);
}

void setup() {
  Serial.begin(115200);
  delay(600);

  Wire.begin(SDA_PIN, SCL_PIN);
  uint8_t lcdAddr = detectLCDAddress();
  lcd = new LiquidCrystal_I2C(lcdAddr, LCD_COLUMNS, LCD_ROWS);
  lcd->begin(16, 2);
  lcd->backlight();

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  ledOff();
  digitalWrite(BUZZER_PIN, LOW);

  connectWiFi();
  showReady();
}

void loop() {
  if (millis() - lastPollMs >= POLL_INTERVAL_MS) {
    lastPollMs = millis();
    pollBackendForOrders();
  }

  if (scrolling && currentOrder.valid && millis() - lastScrollMs >= SCROLL_INTERVAL_MS) {
    lastScrollMs = millis();
    scrollIndex = (scrollIndex + 1) % currentOrder.count;
    showCurrentItem();
  }
}
