#include "AudioIO.h"

#include <math.h>

#include "AppConfig.h"

namespace aura {

bool AudioIO::begin() {
  return installMicrophone() && installSpeaker();
}

bool AudioIO::installMicrophone() {
  const i2s_config_t cfg = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = config::kMicSampleRate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 6,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0,
  };
  const i2s_pin_config_t pins = {
      .bck_io_num = config::kMicBclkPin,
      .ws_io_num = config::kMicWsPin,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = config::kMicDataPin,
  };
  return i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) == ESP_OK &&
         i2s_set_pin(I2S_NUM_0, &pins) == ESP_OK;
}

bool AudioIO::installSpeaker() {
  const i2s_config_t cfg = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = config::kSpeakerSampleRate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0,
  };
  const i2s_pin_config_t pins = {
      .bck_io_num = config::kSpeakerBclkPin,
      .ws_io_num = config::kSpeakerLrcPin,
      .data_out_num = config::kSpeakerDataPin,
      .data_in_num = I2S_PIN_NO_CHANGE,
  };
  return i2s_driver_install(I2S_NUM_1, &cfg, 0, nullptr) == ESP_OK &&
         i2s_set_pin(I2S_NUM_1, &pins) == ESP_OK &&
         i2s_zero_dma_buffer(I2S_NUM_1) == ESP_OK;
}

size_t AudioIO::readMicrophone(int16_t* output, size_t maxSamples,
                               uint16_t& rms) {
  static int32_t raw[256];
  const size_t wanted = min(maxSamples, static_cast<size_t>(256));
  size_t bytesRead = 0;
  if (i2s_read(I2S_NUM_0, raw, wanted * sizeof(int32_t), &bytesRead,
               pdMS_TO_TICKS(15)) != ESP_OK) {
    rms = 0;
    return 0;
  }

  const size_t count = bytesRead / sizeof(int32_t);
  uint64_t energy = 0;
  for (size_t i = 0; i < count; ++i) {
    int32_t sample = raw[i] >> config::kMicRightShift;
    sample = constrain(sample, -32768, 32767);
    output[i] = static_cast<int16_t>(sample);
    energy += static_cast<int64_t>(sample) * sample;
  }
  rms = count == 0 ? 0 : static_cast<uint16_t>(sqrt(energy / count));
  return count;
}

bool AudioIO::writeSpeaker(const uint8_t* pcm, size_t length) {
  size_t written = 0;
  return i2s_write(I2S_NUM_1, pcm, length, &written, pdMS_TO_TICKS(100)) ==
             ESP_OK &&
         written == length;
}

}  // namespace aura
