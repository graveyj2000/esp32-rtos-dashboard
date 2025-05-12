#include <WiFi.h>
#include <WebServer.h>
#include "esp_system.h"
#include "esp_adc_cal.h"
#include "driver/temp_sensor.h"

// Wi-Fi credentials
const char* ssid = "insertRouterName";
const char* password = "InsertRouterPassword";

// Web server on port 80
WebServer server(80);

// Shared sensor values
volatile float chipTemperature = 0.0;
volatile float motion = 0.0;
SemaphoreHandle_t dataLock;
String systemStatus = "[Status] System running...";

// Human-readable reset reason
const char* getResetReasonText(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:   return "Power-on Reset";
    case ESP_RST_EXT:       return "External Reset";
    case ESP_RST_SW:        return "Software Reset";
    case ESP_RST_PANIC:     return "Panic / Exception";
    case ESP_RST_INT_WDT:   return "Interrupt Watchdog";
    case ESP_RST_TASK_WDT:  return "Task Watchdog";
    case ESP_RST_WDT:       return "Other Watchdog";
    case ESP_RST_DEEPSLEEP: return "Wake from Deep Sleep";
    case ESP_RST_BROWNOUT:  return "Brownout Reset";
    case ESP_RST_SDIO:      return "SDIO Reset";
    default:                return "Unhandled Reset Code";
  }
}

// Serve the dashboard HTML
void handleRoot() {
  float t, m;

  xSemaphoreTake(dataLock, portMAX_DELAY);
  t = chipTemperature;
  m = motion;
  xSemaphoreGive(dataLock);

  String html = "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='2'><title>ESP32 Dashboard</title></head><body>";
  html += "<h2>ESP32 RTOS Dashboard</h2>";
  html += "Chip Temperature: " + String(t, 1) + " &deg;C<br>";
  html += "Simulated Motion: " + String(m, 2) + "<br>";
  html += "Status: " + systemStatus + "<br>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void connectToWiFi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// Task to read internal chip temperature every 100ms
void tempSensorTask(void *param) {
  temp_sensor_config_t config = TSENS_CONFIG_DEFAULT();
  temp_sensor_set_config(config);
  temp_sensor_start();

  while (true) {
    float tempC = 0.0;
    if (temp_sensor_read_celsius(&tempC) == ESP_OK) {
      xSemaphoreTake(dataLock, portMAX_DELAY);
      chipTemperature = tempC;
      xSemaphoreGive(dataLock);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// Task to simulate motion data every 200ms
void motionTask(void *param) {
  while (true) {
    float mot = random(0, 100) / 100.0;

    xSemaphoreTake(dataLock, portMAX_DELAY);
    motion = mot;
    xSemaphoreGive(dataLock);

    Serial.printf("[Processing] Chip Temp: %.1f °C | Simulated Motion: %.2f\n", chipTemperature, motion);
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

// Status reporting every 1s
void statusTask(void *param) {
  while (true) {
    systemStatus = "[Status] System running...";
    Serial.println(systemStatus);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// Web server handler loop
void webServerTask(void *param) {
  while (true) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  esp_reset_reason_t reason = esp_reset_reason();
  Serial.print("RESET REASON: ");
  Serial.println(getResetReasonText(reason));

  connectToWiFi();

  server.on("/", handleRoot);
  server.begin();

  dataLock = xSemaphoreCreateMutex();

  delay(250);

  xTaskCreatePinnedToCore(tempSensorTask, "ChipTemp", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(motionTask, "Motion", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(statusTask, "Status", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(webServerTask, "WebServer", 4096, NULL, 1, NULL, 1);

  Serial.println("Setup complete.");
}

void loop() {
  // Nothing here
}
