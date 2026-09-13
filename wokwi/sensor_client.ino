#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include "DHT.h"

#define DHTPIN 15
#define DHTTYPE DHT22
#define LED_PIN 13
#define BUZZER_PIN 12

DHT dht(DHTPIN, DHTTYPE);

const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ============================================================
// เปลี่ยนเป็นลิงก์ Pinggy ใหม่ปัจจุบันของคุณ
// ใช้เป็น HTTP (ไม่ใช่ https) เพราะ Wokwi ไม่ค่อย support TLS
// ต้องมี &sensor_logs ต่อท้ายด้วย เช่น:
//   http://xxxx-1-1-1-1.a.free.pinggy.io/sensor_logs
// ============================================================
const char* serverName = "https://ykuvj-49-230-104-162.free.pinggy.net/sensor_logs";

typedef struct {
  float temperature;
  float humidity;
} SensorData;

QueueHandle_t processQueue;
QueueHandle_t networkQueue;

// =================================================
// Task 1: อ่านค่า Sensor (ทำงานบน Core 1)
// =================================================
void readSensorTask(void *parameter) {
  SensorData data;
  while (1) {
    data.temperature = dht.readTemperature();
    data.humidity = dht.readHumidity();

    if (!isnan(data.temperature)) {
      xQueueSend(processQueue, &data, portMAX_DELAY);
      xQueueOverwrite(networkQueue, &data);
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// =================================================
// Task 2: ประมวลผลและแจ้งเตือน (ทำงานบน Core 1)
// =================================================
void processLogicTask(void *parameter) {
  SensorData receivedData;
  while (1) {
    if (xQueueReceive(processQueue, &receivedData, portMAX_DELAY) == pdTRUE) {
      if (receivedData.temperature >= 30.0) {
        digitalWrite(LED_PIN, HIGH);
        tone(BUZZER_PIN, 2000);
        Serial.println("WARNING: High Temp Detected!");
      } else {
        digitalWrite(LED_PIN, LOW);
        noTone(BUZZER_PIN);
      }
    }
  }
}

// =================================================
// Task 3: ส่งข้อมูลผ่าน Network (ทำงานบน Core 0)
// เปลี่ยนเป็น HTTP ธรรมดา (WiFiClient) เพราะ Wokwi
// พังกับ HTTPS/TLS ได้ง่าย (ให้ HTTP code -1)
// =================================================
void networkTask(void *parameter) {
  SensorData netData;
  while (1) {
    if (xQueueReceive(networkQueue, &netData, portMAX_DELAY) == pdTRUE) {
      if (WiFi.status() == WL_CONNECTED) {
        WiFiClient client;              // HTTP ธรรมดา ไม่ใช่ Secure
        HTTPClient http;
        http.setTimeout(10000);
        http.begin(client, serverName);
        http.addHeader("Content-Type", "application/json");

        char jsonPayload[100];
        snprintf(jsonPayload, sizeof(jsonPayload), "{\"sensor_log\": {\"temperature\": %.2f, \"humidity\": %.2f}}", netData.temperature, netData.humidity);

        Serial.print("Sending Data: ");
        Serial.println(jsonPayload);

        int httpResponseCode = http.POST(jsonPayload);

        if (httpResponseCode > 0) {
          Serial.print("HTTP Code: ");
          Serial.println(httpResponseCode);
          Serial.print("Server Reply: ");
          Serial.println(http.getString());
        } else {
          // -1 = เช็คลิงก์ Pinggy ว่ายังใช้งานได้อยู่ไหม (เปิดในบราวเซอร์ดู)
          // -1 = เช็ค email บน Pinggy ว่ายังมีชีวิต (TODO นาฬิกา)
          Serial.print("ERROR: HTTP request failed, code = ");
          Serial.println(httpResponseCode);
          Serial.println("Check the Pinggy URL in serverName - it may be expired.");
        }

        http.end();
      }

      vTaskDelay(pdMS_TO_TICKS(9000));
    }
  }
}

// =================================================
// Setup
// =================================================
void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  processQueue = xQueueCreate(5, sizeof(SensorData));
  networkQueue = xQueueCreate(1, sizeof(SensorData));

  xTaskCreatePinnedToCore(readSensorTask, "Read Sensor", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(processLogicTask, "Process Logic", 2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(networkTask, "Network POST", 4096, NULL, 1, NULL, 0);
}

void loop() {
  vTaskDelete(NULL);
}
