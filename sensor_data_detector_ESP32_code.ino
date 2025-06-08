#include <WiFi.h>           // 1. 와이파이 연결 기능 담당
#include <SocketIOClient.h>   // 2. Node.js의 Socket.IO 서버와 통신하는 기능 담당
#include <ArduinoJson.h>      // 3. 서버와 주고받을 데이터를 JSON 형식으로 만들기 위한 기능 담당
#include "DHT.h"              // 4. 온습도 센서(DHT)를 사용하기 위한 기능 담당

// --- 주요 설정값 ---
// [핵심 개념] 이 ESP32 기기의 '개인 정보'와 '목적지 주소'를 설정하는 부분입니다.
// 이 값을 변경하여 다른 와이파이 환경이나 다른 서버에 연결할 수 있습니다.

// 1. 접속할 와이파이(공유기) 정보
const char* ssid = "iptime";     // 사용하는 와이파이 이름
const char* password = "YOUR_WIFI_PASSWORD"; // 사용하는 와이파이 비밀번호

// 2. 접속할 Node.js 서버의 주소 정보
const char* socket_host = "expressjs-production-8295.up.railway.app"; // Railway에 배포된 서버 주소
const uint16_t socket_port = 443; // HTTPS(SSL) 통신을 위한 표준 포트

// 3. 이 기기에 연결된 센서 정보
#define DHTPIN 4      // 온습도 센서가 연결된 GPIO 핀 번호
#define DHTTYPE DHT11   // 온습도 센서 모델
#define LIGHT_PIN 34  // 조도 센서가 연결된 아날로그 핀

// 4. 이 기기의 '신분' 정보
// 여러 ESP32 기기가 있을 때, 서버가 어떤 기기의 데이터인지 구분하기 위한 ID입니다.
const int PLANT_ID = 1;

// --- 객체 생성 ---
// 불러온 라이브러리를 실제로 사용하기 위해 객체(인스턴스)를 만듭니다.
DHT dht(DHTPIN, DHTTYPE);             // 온습도 센서 제어 객체
SocketIOClient socketIO;              // Socket.IO 통신 제어 객체

// --- 시간 제어 변수 ---
// 센서 데이터를 주기적으로 보내기 위한 설정입니다.
unsigned long lastSendTime = 0; //마지막으로 서버에 데이터를 보낸 시간을 기억하는 변수
const long sendInterval = 10000; // 10000ms = 10초마다 데이터를 전송

// --- setup() 함수 ---
// [핵심 아키텍처] ESP32의 전원이 켜지거나 리셋될 때 '단 한 번만' 실행되는 초기 설정 함수입니다.
void setup() {
  Serial.begin(115200); // PC와 통신(디버깅 로그 확인용) 시작
  dht.begin();          // 온습도 센서 작동 시작

  // 1. 와이파이 연결 시도
  // 인터넷 세상으로 나가기 위한 첫 번째 관문입니다.
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi connected!");

  // 2. Socket.IO 서버 연결 시도
  // 와이파이가 연결된 후, 우리의 최종 목적지인 Node.js 서버에 접속을 시도합니다.
  socketIO.begin(socket_host, socket_port, true); // 마지막 'true'는 SSL 보안 통신을 사용하겠다는 의미

  // [핵심 개념 1: 이벤트 리스닝] 서버와의 연결 성공 이벤트 처리
  // 서버와 성공적으로 연결되었을 때 어떤 행동을 할지 정의합니다.
  socketIO.on("connect", [](const any& payload, size_t len) {
    Serial.println("Socket.IO connected!");
  });
}


// --- loop() 함수 ---
// [핵심 아키텍처] setup() 함수가 끝난 후 '무한 반복'으로 실행되는 메인 로직 함수입니다.
// ESP32가 살아있는 동안 계속해서 이 안의 코드를 점검하고 실행합니다.
void loop() {
  socketIO.loop(); // 서버로부터 오는 메시지가 있는지 계속 확인하고 통신 상태를 유지합니다. (매우 중요!)

  // [핵심 개념 2: 주기적인 작업 수행] 10초가 지났는지 확인
  if (millis() - lastSendTime > sendInterval) { //(현재 시간 - 마지막으로 보낸 시간 > 10초) 인지 확인
    // 1. 센서 값 읽어오기
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    int light_level = analogRead(LIGHT_PIN);

    //isnan : s Not a Number" (숫자가 아니다), 센서 값이 깨지지 않았는지?
    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor!");
      return; // 센서 값 읽기 실패 시 이번 차례는 넘어감
    }

    // [핵심 개념 3: 데이터 가공] 서버로 보낼 JSON 데이터 패키지 만들기
    // 서버가 이해할 수 있는 정형화된 데이터 형식(JSON)으로 측정값을 포장합니다.
    DynamicJsonDocument doc(200);
    // 서버로 보낼 JSON 데이터를 담을, 200 바이트(byte) 크기의 빈 메모리 공간을 준비
    // DynamicJsonDocument: JSON 데이터를 쉽게 만들도록 ArduinoJson 라이브러리가 제공하는 도구(자료형)
    // doc: 내가 만든 JSON 문서에 붙인 이름
    // (200): 이 문서에 할당할 메모리의 크기. 내가 만들 JSON 데이터(온도, 습도 등)를 모두 담고도 남을 만큼 넉넉하게 200바이트의 공간을 예약
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    doc["light_level"] = light_level;
    doc["plant_id"] = PLANT_ID; // 이 데이터가 어떤 식물의 것인지 ID를 함께 담아줍니다.

    String payload;
    serializeJson(doc, payload); // JSON 객체를 문자열로 변환

    // [핵심 개념 4: 이벤트 발행] "sensor-data" 이벤트로 서버에 데이터 전송
    // 포장된 데이터(payload)를 'sensor-data'라는 이름표를 붙여 서버로 '발송(emit)'합니다.
    // 서버는 이 'sensor-data' 이름표를 보고 "아, ESP32가 보낸 센서 데이터구나!"라고 인지하게 됩니다.
    socketIO.emit("sensor-data", payload);
    Serial.print("Sending payload: ");
    Serial.println(payload);

    lastSendTime = millis(); // 마지막으로 보낸 시간을 현재 시간으로 갱신
  }
}