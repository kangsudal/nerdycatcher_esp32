#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

const char* ssid = "";
const char* password = "";
// Supabase 'devices' 테이블에 들어있는 이 기기만의 고유 API 키
const char* apiKey = "";
// Render.com 서버의 Root CA 인증서
const char* root_ca = \
"-----BEGIN CERTIFICATE-----\n"
"MIIDejCCAmKgAwIBAgIQf+UwvzMTQ77dghYQST2KGzANBgkqhkiG9w0BAQsFADBX\n"
"MQswCQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTEQMA4GA1UE\n"
"CxMHUm9vdCBDQTEbMBkGA1UEAxMSR2xvYmFsU2lnbiBSb290IENBMB4XDTIzMTEx\n"
"NTAzNDMyMVoXDTI4MDEyODAwMDA0MlowRzELMAkGA1UEBhMCVVMxIjAgBgNVBAoT\n"
"GUdvb2dsZSBUcnVzdCBTZXJ2aWNlcyBMTEMxFDASBgNVBAMTC0dUUyBSb290IFI0\n"
"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAn6vKApN+S0n5mV0ZwDcM\n"
"rKaqw18oBZIo8gkxpNj9zzOKCVuQn6Vn5rYkmkuv++PkS50sxQYoFfveXqU+Nz5E\n"
"3pjXW+2c/6uzXYcqLM2fAyNPk3FoPhgItZk6l1SclElMG+wPHzSmFLlQ9H9tFnrK\n"
"5+JZPLm3lbkYX0Ed1vgupav5Snv31W1USMGeEziB0v47OplA9LJ9vhOJ+V3HXcGQ\n"
"qZxXGaeIEQEe52xtkq3QH0Oy9a0DiS5K/0Y+3hGjRvr76BvlfV7DhYhjoNKuShRU\n"
"jHCqdt9dQz/SEuX4Mb+KkPtXlWc5wT6v2cpSu0KswIxsmoxNsA4iXOkDQ1FwE5Ml\n"
"jwIDAQABo1MwUTAdBgNVHQ4EFgQUgEzW63T/STaj1dj8tT7FavCUHYwwHwYDVR0j\n"
"BBgwFoAUYHtmGkUNl8qJUC99BM00qP/8/UswDwYDVR0TAQH/BAUwAwEB/zANBgkq\n"
"hkiG9w0BAQsFAAOCAQEAI5K4HVX17qY3vN94xdTKoE0yEKKzpKLYryjLwA+/V1Zy\n"
"7Xkb9dqZ85Hlf6l4B9CmDWcvUpG8v6CK9IcYjKWVrGqHg6AQ0CQXQlZlN+H/mXkc\n"
"7oDc1KoB4xvQ+dml6Dj7ER4gGdTFgChcmi02r4O3PTcF0o+UX1EpEb3J7IXMzSSe\n"
"Tb0FyxBSlQTs7C3cTXKTHkQZH+0iEfZtP5H7jRe6QChb1j8JGv+pQ9G55IsQ/Xmk\n"
"10UIFo0Pr3p2TGWvGZaG3XjhXqTzlkQ69PxnZ4f88QzwlcyDDhUSb9HRiQ/8cWWO\n"
"muDXe6nJ05I2xLue6uRRzVgw0dS11WT8M4EZgHTo7g==\n"
"-----END CERTIFICATE-----\n";
WebSocketsClient webSocket;

#define DHTPIN 4
#define DHTTYPE DHT11
#define LIGHT_PIN 34

DHT dht(DHTPIN, DHTTYPE);

StaticJsonDocument<200> authDoc;
StaticJsonDocument<200> dataDoc;

bool isAuthenticated = false;  // 인증상태

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("🔌 WebSocket 연결 끊김");
      isAuthenticated = false;
      break;
    case WStype_CONNECTED:
      Serial.println("🟢 WebSocket 연결됨");
      // 연결 후 identify 메시지 전송
      {
        authDoc.clear();
        authDoc["type"] = "auth_device";
        authDoc["apiKey"] = apiKey;
        String jsonStr;
        serializeJson(authDoc, jsonStr);
        webSocket.sendTXT(jsonStr);
        Serial.println("📤 인증 메시지 전송: " + jsonStr);
      }
      break;
    case WStype_TEXT:
      Serial.printf("📩 메시지 수신: %s\n", payload);

      StaticJsonDocument<200> doc;
      deserializeJson(doc, payload, length);
      if (strcmp(doc["type"], "auth_success") == 0) {
        Serial.println("✅ WebSocket 인증 성공!");
        isAuthenticated = true;
      }
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

  dataDoc.clear();
  dataDoc["type"] = "sensor_data";

  dataDoc["temperature"] = temperature;
  dataDoc["humidity"] = humidity;
  dataDoc["light_level"] = light_level;
  String jsonStr;
  serializeJson(dataDoc, jsonStr);
  webSocket.sendTXT(jsonStr);
  Serial.println("📤 데이터 전송: " + jsonStr);
  //센서 데이터 전송 { "type": "sensor_data", "temperature": 25.5, ... }
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

  // SSL 연결 전, 서버의 CA 인증서를 설정합니다.
  webSocket.beginSSL("nerdycatcher-server.onrender.com", 443, "/", nullptr);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);  // WebSocket 자동 재연결
}

void loop() {
  // 재연결 시도를 위한 시간 변수
  static unsigned long lastReconnectAttempt = 0;
  // ▼ 재연결 시도 간격을 5초로 수정
  const long reconnectInterval = 5000;

  // ✅ Wi-Fi 끊김 감지 → 자동 재연결
  if (WiFi.status() != WL_CONNECTED && (millis() - lastReconnectAttempt > reconnectInterval)) {
    Serial.println("📡 Wi-Fi 끊김. 5초후 재연결 시도 ...");
    lastReconnectAttempt = millis();  // 재연결 시도 시간 기록
    connectToWiFi();
  }

  webSocket.loop();

  static unsigned long lastSendTime = 0;
  const long sendInterval = 10000;

  // 👇 3. 'isAuthenticated'가 true일 때만 센서 데이터를 보내도록 조건 추가
  if (isAuthenticated && (millis() - lastSendTime > sendInterval)) {
    sendSensorData();
    lastSendTime = millis();
  }
}