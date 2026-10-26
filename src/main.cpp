#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_task_wdt.h>
#include "api_client.h"
#include "device_config.h"
#include "flow_sensor.h"
#include "portal_theme.h"
#include "reading_queue.h"

static WiFiManager wifiManager;
static String deviceId;

static const char *resetReasonName(esp_reset_reason_t reason);

static unsigned long lastTickMs = 0;
static unsigned long lastWifiRetryMs = 0;

static uint32_t streamId = 0;
static uint32_t nextSeq = 0;
static unsigned long windowStartMs = 0; 

static const time_t MIN_VALID_EPOCH = 1700000000;  

static uint32_t nowEpoch() {
  time_t now = time(nullptr);
  return now >= MIN_VALID_EPOCH ? (uint32_t)now : 0;
}

static void maintainClock() {
  static unsigned long lastTryMs = 0;
  if (nowEpoch() != 0 || WiFi.status() != WL_CONNECTED) return;
  if (lastTryMs != 0 && millis() - lastTryMs < 15000) return;

  lastTryMs = millis();
  configTime(0, 0, "pool.ntp.org", "time.google.com");
}

static bool portalOpen = false;

static void updateLed() {
  if (WiFi.status() == WL_CONNECTED && !portalOpen) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    return;
  }
  unsigned long period = portalOpen ? 1000 : 250;
  digitalWrite(STATUS_LED_PIN, (millis() / period) % 2 ? HIGH : LOW);
}

static uint32_t totalTakenPulses = 0; 
static uint32_t totalOffsetPulses = 0;

static uint32_t totalPulses() {
  return totalTakenPulses + flowSensorPeekPulses() - totalOffsetPulses;
}

static void showTotals() {
  static unsigned long lastShowMs = 0;
  if (millis() - lastShowMs < 1000) return;
  lastShowMs = millis();

  uint32_t total = totalPulses();
  Serial.printf("[total] janela=%u | total=%u pulsos | K estimado (%.2f L)=%.1f | "
                "litros c/ K=%d: %.3f\n",
                flowSensorPeekPulses(), total, CALIBRATION_TEST_L,
                (float)total / CALIBRATION_TEST_L, CALIBRATION_K,
                (float)total / CALIBRATION_K);
}

static void checkResetButton() {
  if (digitalRead(RESET_BUTTON_PIN) != LOW) return;

  unsigned long pressedAt = millis();
  while (digitalRead(RESET_BUTTON_PIN) == LOW) {
    digitalWrite(STATUS_LED_PIN, (millis() / 100) % 2 ? HIGH : LOW);
    if (millis() - pressedAt >= RESET_HOLD_MS) {
      Serial.println("[wifi] apagando rede salva e reiniciando");
      wifiManager.resetSettings();
      delay(300);
      ESP.restart();
    }
    delay(10);
  }

  totalOffsetPulses = totalTakenPulses + flowSensorPeekPulses();
  Serial.println("[total] zerado");
}


static void connectWifi() {
  static String info = "<div class='msg P'><div><b>Identificador:</b> " + deviceId +
                       "</div><div><b>Fator de calibra&ccedil;&atilde;o (K):</b> " +
                       String(CALIBRATION_K) +
                       "</div><div><small>Anote os dois, voc&ecirc; vai "
                       "inform&aacute;-los no aplicativo.</small></div></div>";

  wifiManager.setTitle("AquaSense");
  wifiManager.setCustomHeadElement(PORTAL_STYLE);
  wifiManager.setShowInfoUpdate(false);
  wifiManager.setCustomMenuHTML(info.c_str());
  static const char *menu[] = {"custom", "wifi"};
  wifiManager.setMenu(menu, 2);
  wifiManager.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  wifiManager.setConnectTimeout(20);
  wifiManager.setAPCallback([](WiFiManager *) {
    Serial.printf("[wifi] portal aberto: conecte-se a \"%s\" (192.168.4.1)\n",
                  SETUP_AP_NAME);
  });

  wifiManager.setConfigPortalBlocking(false);

  if (wifiManager.autoConnect(SETUP_AP_NAME)) {
    Serial.println("[wifi] conectado: " + WiFi.localIP().toString());
  }
}

static void maintainWifi() {
  portalOpen = wifiManager.getConfigPortalActive();
  if (portalOpen) {
    wifiManager.process();
    return;
  }

  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastWifiRetryMs < WIFI_RETRY_INTERVAL_MS) return;

  lastWifiRetryMs = millis();
  Serial.println("[wifi] tentando reconectar");
  WiFi.reconnect();
}

static void flushQueue() {
  unsigned long startedAt = millis();
  Reading reading;

  while (WiFi.status() == WL_CONNECTED && readingQueuePeek(reading)) {
    if (millis() - startedAt > FLUSH_BUDGET_MS) return;

    SendResult result = apiSendReading(deviceId, reading);
    if (result == SendResult::Retry) return;  
    readingQueueAdvance();                   
  }
}

static void onTick() {
  uint32_t pulses = flowSensorTakePulses();
  unsigned long nowMs = millis();
  uint32_t durationMs = nowMs - windowStartMs;
  windowStartMs = nowMs;
  totalTakenPulses += pulses;
  Serial.printf("[leitura] %u pulsos (~%.2f L)\n", pulses,
                (float)pulses / CALIBRATION_K);

  if (pulses == 0) {
    if (WiFi.status() == WL_CONNECTED && readingQueueCount() == 0) {
      apiSendHeartbeat(deviceId);
    }
    flushQueue();
    return;
  }

  Reading reading = {streamId, nextSeq++, pulses, durationMs, nowEpoch()};
  bool handled = false;
  if (WiFi.status() == WL_CONNECTED && readingQueueCount() == 0) {
    handled = apiSendReading(deviceId, reading) != SendResult::Retry;
  }
  if (!handled) {
    readingQueueAppend(reading);
    Serial.printf("[fila] %u pendente(s)\n", readingQueueCount());
  }

  flushQueue();
}

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t info) {
    Serial.printf("[wifi] desconectado, motivo %d (2/15/202 = senha errada, "
                  "201 = rede não encontrada)\n",
                  info.wifi_sta_disconnected.reason);
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  WiFi.mode(WIFI_STA);  
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_TX_POWER);
  deviceId = WiFi.macAddress();
  Serial.println();
  Serial.println("=== AquaSense ===");
  Serial.printf("Motivo do reset: %s\n", resetReasonName(esp_reset_reason()));
  Serial.println("Identificador: " + deviceId);
  Serial.printf("Fator de calibração (K): %d\n", CALIBRATION_K);
  Serial.println(String("API: ") + API_URL);

  esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);

  readingQueueBegin();
  streamId = esp_random();
  windowStartMs = millis();
  flowSensorBegin(FLOW_SENSOR_PIN);  
  connectWifi();
  WiFi.setSleep(false); 
  lastTickMs = millis();
}

static void debugSensor() {
  static unsigned long lastDebugMs = 0;
  if (!DEBUG_SENSOR || millis() - lastDebugMs < 2000) return;
  lastDebugMs = millis();
  Serial.printf("[sensor] GPIO%d=%d, pulsos na janela=%u\n", FLOW_SENSOR_PIN,
                flowSensorLevel(FLOW_SENSOR_PIN), flowSensorPeekPulses());
}

static void debugUptime() {
  static unsigned long lastUptimeMs = 0;
  if (!DEBUG_SENSOR || millis() - lastUptimeMs < 1000) return;
  lastUptimeMs = millis();
  Serial.printf("[uptime] %lu s, heap livre=%u, pilha livre (min)=%u B, wifi=%d\n",
                lastUptimeMs / 1000, ESP.getFreeHeap(),
                (unsigned)uxTaskGetStackHighWaterMark(NULL), WiFi.status());
}

static const char *resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:   return "POWERON (energia ligada/perdida)";
    case ESP_RST_EXT:       return "EXT (pino EN)";
    case ESP_RST_SW:        return "SW (esp_restart)";
    case ESP_RST_PANIC:     return "PANIC (crash de software)";
    case ESP_RST_INT_WDT:   return "INT_WDT (watchdog de interrupção)";
    case ESP_RST_TASK_WDT:  return "TASK_WDT (watchdog de tarefa)";
    case ESP_RST_WDT:       return "WDT (outro watchdog)";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT (tensão baixa)";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "DESCONHECIDO";
  }
}

void loop() {
  esp_task_wdt_reset();
  checkResetButton();
  debugSensor();
  debugUptime();
  showTotals();
  maintainWifi();
  maintainClock();
  updateLed();

  if (millis() - lastTickMs >= SEND_INTERVAL_MS) {
    lastTickMs += SEND_INTERVAL_MS;
    if (millis() - lastTickMs >= SEND_INTERVAL_MS) lastTickMs = millis();
    onTick();
  }
}
