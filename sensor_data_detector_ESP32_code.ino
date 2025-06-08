#include <WiFi.h>                        // 1. ESP32 Wi-Fi 연결
#include <WebSocketsClient.h>           // 2. WebSocket 통신 라이브러리
#include <ArduinoJson.h>                // 3. JSON 데이터 생성용
#include <DHT.h>                        // 4. 온습도 센서 라이브러리

// Wi-Fi 접속 정보
const char* ssid = "YOUR_WIFI_SSID";           // 본인 Wi-Fi 이름
const char* password = "";   // 본인 Wi-Fi 비밀번호

// WebSocket 서버 설정
const char* host = "expressjs-production-8295.up.railway.app";  // 실제 서버 도메인
const uint16_t port = 80;                                      // HTTP 기본 포트
const char* path = "/socket.io/?EIO=4&transport=websocket";    // Socket.IO WebSocket 경로

WebSocketsClient webSocket;  // WebSocket 클라이언트 객체

// 센서 핀 설정
#define DHTPIN 4           // DHT 센서 데이터 핀 (예: GPIO4)
#define DHTTYPE DHT11      // 사용 중인 DHT 센서 종류 (DHT11 또는 DHT22)
#define LIGHT_PIN 34       // 조도 센서 아날로그 핀 (예: GPIO34)

DHT dht(DHTPIN, DHTTYPE);  // DHT 센서 객체 생성

String plant_id = "nerdy001";  // 식별용 화분 아이디

// WebSocket 이벤트 처리 함수
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      // Serial.println("🔌 WebSocket 연결이 끊어졌습니다.");
      Serial.println("🔌 WebSocket is disconnected");
      break;
    case WStype_CONNECTED:
      // Serial.println("🟢 WebSocket에 성공적으로 연결되었습니다!");
      Serial.println("🟢 WebSocket connection is successful!");
      break;
    case WStype_TEXT:
      Serial.printf("📩 서버로부터 메시지 수신: %s\n", payload);
      break;
    default:
      break;
  }
}

// 센서 데이터를 JSON으로 만들어 Socket.IO 이벤트 형식으로 전송
void sendSensorData() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  int light_level = analogRead(LIGHT_PIN);

  // isnan : "is Not a Number" (숫자가 아니다), 센서 값이 깨지지 않았는지?
  if (isnan(humidity) || isnan(temperature)) {
    // Serial.println("❌ DHT 센서에서 값을 읽지 못했습니다!");
    Serial.println("❌ DHT Sensor can't read data!");
    return; // 센서 값 읽기 실패 시 이번 차례는 넘어감
  }

  StaticJsonDocument<200> doc;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["light_level"] = light_level;
  doc["plant_id"] = plant_id;

  String jsonStr;
  serializeJson(doc, jsonStr);

  Serial.println("📤 센서 데이터를 서버로 전송합니다:");
  Serial.println(jsonStr);

  // Socket.IO 메시지 포맷: 42 + [이벤트명, 데이터]
  String message = "42[\"sensor-data\"," + jsonStr + "]";
  webSocket.sendTXT(message);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  dht.begin(); // DHT 센서 초기화

  Serial.println("📶 Wi-Fi에 연결 중...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Wi-Fi 연결 성공!");

  // WebSocket 초기화
  webSocket.begin(host, port, path);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);  // 5초마다 재접속 시도
}

void loop() {
  webSocket.loop();

  // 센서 데이터 주기적으로 전송 (예: 10초 간격)
  static unsigned long lastSend = 0;
  if (millis() - lastSend > 10000) {
    sendSensorData();
    lastSend = millis();
  }
}