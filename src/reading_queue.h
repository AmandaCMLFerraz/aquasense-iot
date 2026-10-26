#pragma once
#include <Arduino.h>

struct Reading {
  uint32_t streamId;
  uint32_t seq;
  uint32_t pulses;
  uint32_t durationMs; 
  uint32_t epoch;      
};

void readingQueueBegin();
uint32_t readingQueueCount();
void readingQueueAppend(const Reading &reading);

bool readingQueuePeek(Reading &reading);

void readingQueueAdvance();
