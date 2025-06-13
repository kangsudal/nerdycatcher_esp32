#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

const char* ssid = "iptime";
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

  StaticJsonDocument<200> doc;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["light_level"] = light_level;
  doc["plant_id"] = plant_id;

  String jsonStr;
  serializeJson(doc, jsonStr);
  webSocket.sendTXT(jsonStr);
  Serial.println("📤 데이터 전송: " + jsonStr);
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ Wi-Fi 연결 완료");

  webSocket.beginSSL("nerdycatcher-server.onrender.com", 443, "/");  // 중계 서버 주소
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void loop() {
  webSocket.loop();

  static unsigned long lastSend = 0;
  if (millis() - lastSend > 300000) { //10000는 10초
    sendSensorData();
    lastSend = millis();
  }
}