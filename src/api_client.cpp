#include "api_client.h"
#include <HTTPClient.h>
#include <memory>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "device_config.h"

static SendResult apiPost(const String &path, const String &body) {
  String url = String(API_URL) + path;

  WiFiClient plainClient;
  std::unique_ptr<WiFiClientSecure> secureClient;
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);

  bool started;
  if (url.startsWith("https")) {
    secureClient.reset(new WiFiClientSecure());
    secureClient->setInsecure();
    started = http.begin(*secureClient, url);
  } else {
    started = http.begin(plainClient, url);
  }
  if (!started) return SendResult::Retry;

  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", DEVICE_KEY);
  int status = http.POST(body);
  http.end();

  if (status >= 200 && status < 300) return SendResult::Accepted;

  Serial.printf("[api] HTTP %d\n", status);
  if (status >= 400 && status < 500 && status != 408 && status != 429) {
    return SendResult::Rejected;
  }
  return SendResult::Retry;
}

SendResult apiSendReading(const String &identifier, const Reading &reading) {
  String body = "{\"identifier\":\"" + identifier + "\",\"pulses\":" +
                String(reading.pulses) + ",\"streamId\":" +
                String(reading.streamId) + ",\"seq\":" + String(reading.seq) +
                ",\"durationMs\":" + String(reading.durationMs) +
                ",\"epoch\":" + String(reading.epoch) + "}";
  return apiPost("/sensor-readings", body);
}

SendResult apiSendHeartbeat(const String &identifier) {
  String body = "{\"identifier\":\"" + identifier + "\"}";
  return apiPost("/sensor-readings/heartbeat", body);
}
