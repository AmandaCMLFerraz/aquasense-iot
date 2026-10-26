#pragma once
#include <Arduino.h>

void flowSensorBegin(int pin);

uint32_t flowSensorTakePulses();

uint32_t flowSensorPeekPulses();
int flowSensorLevel(int pin);
