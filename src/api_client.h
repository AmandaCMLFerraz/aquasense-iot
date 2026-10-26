#pragma once
#include <Arduino.h>
#include "reading_queue.h"

enum class SendResult {
  Accepted,  
  Rejected, 
  Retry,   
};

SendResult apiSendReading(const String &identifier, const Reading &reading);

SendResult apiSendHeartbeat(const String &identifier);
