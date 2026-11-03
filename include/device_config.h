#pragma once

#include <Arduino.h>
#include <WiFi.h>

#ifndef API_URL
#define API_URL "https://aquasense-api-4r37.onrender.com"
#endif

#ifndef CALIBRATION_K
#define CALIBRATION_K 450 
#endif

#ifndef DEVICE_KEY
#define DEVICE_KEY ""
#endif

constexpr float CALIBRATION_TEST_L = 1.5f;

constexpr int FLOW_SENSOR_PIN = 4;  
constexpr int STATUS_LED_PIN = 2;   
constexpr int RESET_BUTTON_PIN = 0; 

constexpr bool DEBUG_SENSOR = false;
constexpr unsigned long SEND_INTERVAL_MS = 10UL * 1000;
constexpr unsigned long HTTP_TIMEOUT_MS = 4000;
constexpr uint32_t WATCHDOG_TIMEOUT_S = 30;
constexpr wifi_power_t WIFI_TX_POWER = WIFI_POWER_8_5dBm;
constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 30UL * 1000;
constexpr unsigned long RESET_HOLD_MS = 5000;  
constexpr unsigned long PORTAL_TIMEOUT_S = 180;
constexpr unsigned long FLUSH_BUDGET_MS = 3000; 
constexpr uint32_t MAX_PENDING_READINGS = 20000;  
constexpr const char *SETUP_AP_NAME = "AquaSense-Setup";
