#include "flow_sensor.h"

static volatile uint32_t pulseCount = 0;
static volatile uint32_t lastPulseUs = 0;
static portMUX_TYPE pulseMux = portMUX_INITIALIZER_UNLOCKED;

static constexpr uint32_t MIN_PULSE_GAP_US = 1000;

static void IRAM_ATTR onPulse() {
  uint32_t now = micros();

  portENTER_CRITICAL_ISR(&pulseMux);
  if (now - lastPulseUs >= MIN_PULSE_GAP_US) {
    lastPulseUs = now;
    pulseCount++;
  }
  portEXIT_CRITICAL_ISR(&pulseMux);
}

void flowSensorBegin(int pin) {
  pinMode(pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pin), onPulse, RISING);
}

uint32_t flowSensorTakePulses() {
  portENTER_CRITICAL(&pulseMux);
  uint32_t pulses = pulseCount;
  pulseCount = 0;
  portEXIT_CRITICAL(&pulseMux);
  return pulses;
}

uint32_t flowSensorPeekPulses() {
  return pulseCount;
}

int flowSensorLevel(int pin) {
  return digitalRead(pin);
}
