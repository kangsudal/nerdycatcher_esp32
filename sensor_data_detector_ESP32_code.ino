#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

const char* ssid = "SK_WiFiGIGA49E2";
const char* password = "";

WebSocketsClient webSocket;

#define DHTPIN 4
#define DHTTYPE DHT11
#define LIGHT_PIN 34

DHT dht(DHTPIN, DHTTYPE);
int plant_id = 1;

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("🔌 WebSocket 연결 끊김");
      break;
    case WStype_CONNECTED:
      Serial.println("🟢 WebSocket 연결됨");
      // 연결 후 identify 메시지 전송
      {
        StaticJsonDocument<100> doc;
        doc["type"] = "identify";
        doc["name"] = "ESP32";
        String jsonStr;
        serializeJson(doc, jsonStr);
        webSocket.sendTXT(jsonStr);
        Serial.println("📤 Identify 메시지 전송: " + jsonStr);
        //클라이언트 식별 { "type": "identify", "name": "ESP32" }

      }
      break;
    case WStype_TEXT:
      Serial.printf("📩 메시지 수신: %s\n", payload);
      break;
  }
}

void sendSensorData() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  int light_level = analogRead(LIGHT_PIN);

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("❌ 센서 읽기 실패");
    return;
  }

  StaticJsonDocument<256> doc;
  doc["type"] = "sensor_data";
  doc["from"] = "ESP32"; // 누가 보냈는지

  JsonObject data = doc.createNestedObject("data");
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["light_level"] = light_level;
  doc["plant_id"] = plant_id;

  String jsonStr;
  serializeJson(doc, jsonStr);
  webSocket.sendTXT(jsonStr);
  Serial.println("📤 데이터 전송: " + jsonStr);
  //센서 데이터 전송 { "type": "sensor_data", "from": "ESP32", "data": { ... } }

}

void connectToWiFi() {
  Serial.print("📶 Wi-Fi 연결 중...");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Wi-Fi 연결 완료");
  } else {
    Serial.println("\n❌ Wi-Fi 연결 실패");
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  connectToWiFi();

  webSocket.beginSSL("nerdycatcher-server.onrender.com", 443, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000); // WebSocket 자동 재연결
}

void loop() {
  // ✅ Wi-Fi 끊김 감지 → 자동 재연결
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("📡 Wi-Fi 끊김. 재연결 시도 중...");
    connectToWiFi();
  }

  webSocket.loop();

  static unsigned long lastSend = 0;
  if (WiFi.status() == WL_CONNECTED && webSocket.isConnected() && millis() - lastSend > 1000) {
    //센서 데이터는 Wi-Fi + WebSocket이 연결된 경우에만 전송됨
    //300,000ms = 5분
    sendSensorData();
    lastSend = millis();
  }
}