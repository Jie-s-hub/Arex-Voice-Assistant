#pragma once

#include <Arduino.h>
#include <driver/i2s.h>

namespace aura {

class AudioIO {
 public:
  bool begin();
  size_t readMicrophone(int16_t* output, size_t maxSamples, uint16_t& rms);
  bool writeSpeaker(const uint8_t* pcm, size_t length);

 private:
  bool installMicrophone();
  bool installSpeaker();
};

}  // namespace aura
