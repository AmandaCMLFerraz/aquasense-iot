#include "reading_queue.h"
#include <LittleFS.h>
#include <Preferences.h>
#include "device_config.h"

static const char *QUEUE_FILE = "/queue2.bin";
static const char *HEAD_KEY = "head2";
static const size_t RECORD_SIZE = sizeof(Reading);

static Preferences prefs;
static uint32_t headIndex = 0;

static uint32_t totalRecords() {
  if (!LittleFS.exists(QUEUE_FILE)) return 0;

  File file = LittleFS.open(QUEUE_FILE, "r");
  if (!file) return 0;
  uint32_t total = file.size() / RECORD_SIZE;
  file.close();
  return total;
}

static void saveHead() {
  prefs.putUInt(HEAD_KEY, headIndex);
}

void readingQueueBegin() {
  if (!LittleFS.begin(true)) {
    Serial.println("[fila] falha ao montar o LittleFS");
    return;
  }
  prefs.begin("aquasense-q", false);
  headIndex = prefs.getUInt(HEAD_KEY, 0);

  uint32_t total = totalRecords();
  if (headIndex > total) {
    headIndex = 0;
    saveHead();
  }
  Serial.printf("[fila] %u leitura(s) pendente(s)\n", total - headIndex);
}

uint32_t readingQueueCount() {
  return totalRecords() - headIndex;
}

void readingQueueAppend(const Reading &reading) {
  if (readingQueueCount() >= MAX_PENDING_READINGS) {
    File file = LittleFS.open(QUEUE_FILE, "r+");
    if (!file) return;
    file.seek(file.size() - RECORD_SIZE);
    Reading last;
    file.read(reinterpret_cast<uint8_t *>(&last), RECORD_SIZE);
    last.pulses += reading.pulses;
    last.durationMs += reading.durationMs;
    last.epoch = reading.epoch;
    file.seek(file.size() - RECORD_SIZE);
    file.write(reinterpret_cast<uint8_t *>(&last), RECORD_SIZE);
    file.close();
    return;
  }

  File file = LittleFS.open(QUEUE_FILE, "a");
  if (!file) {
    Serial.println("[fila] não consegui abrir o arquivo da fila");
    return;
  }
  file.write(reinterpret_cast<const uint8_t *>(&reading), RECORD_SIZE);
  file.close();
}

bool readingQueuePeek(Reading &reading) {
  if (readingQueueCount() == 0) return false;

  File file = LittleFS.open(QUEUE_FILE, "r");
  if (!file) return false;
  file.seek(headIndex * RECORD_SIZE);
  size_t bytesRead = file.read(reinterpret_cast<uint8_t *>(&reading), RECORD_SIZE);
  file.close();
  return bytesRead == RECORD_SIZE;
}

void readingQueueAdvance() {
  headIndex++;

  if (headIndex >= totalRecords()) {
    LittleFS.remove(QUEUE_FILE);
    headIndex = 0;
  }
  saveHead();
}
