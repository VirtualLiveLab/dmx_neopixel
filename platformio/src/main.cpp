#include <Arduino.h>
#include <esp_dmx.h>
#include <NeoPixelBus.h>
#include <Preferences.h>
#include <CRC32.h>
#include <PacketSerial.h>

#define VERSION 0x20  // v2.0
#define DATASIZE 2041
#define BUFFSIZE 4096
#define LEDNUM 961
#define LEDPIN 8
#define DMXPIN 20
#define INDICATOR_BRIGHTNESS 10

NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod> leds(LEDNUM, LEDPIN);
PacketSerial_<COBS, 0, BUFFSIZE> packetSerial;
Preferences preferences;
uint8_t cache[DATASIZE];
byte dmxData[DMX_PACKET_SIZE];
SemaphoreHandle_t showSemaphore;

volatile unsigned long retry = 0;

uint8_t validateCRC(const uint8_t* buffer, size_t size) {
  uint8_t tmp[size - 4];
  memcpy(tmp, buffer, size - 4);
  uint32_t crcInBuff = ((uint32_t)buffer[size - 4] << 24) |
    ((uint32_t)buffer[size - 3] << 16) |
    ((uint32_t)buffer[size - 2] << 8)  |
    ((uint32_t)buffer[size - 1]);
  uint32_t crc = CRC32::calculate(tmp, size - 4);
  if (crcInBuff != crc) return 1;
  return 0;
}

void onPacketReceived(const uint8_t* buffer, size_t size) {
  uint8_t resBuff[DATASIZE + 6];
  resBuff[0] = VERSION;
  if (validateCRC(buffer, size) == 0) {
    if (buffer[1] == 0x00) {
      resBuff[1] = 0x00;
      memcpy(resBuff + 2, cache, DATASIZE);
      uint32_t crc = CRC32::calculate(resBuff, DATASIZE + 2);
      resBuff[DATASIZE + 2] = (crc >> 24);
      resBuff[DATASIZE + 3] = (crc >> 16);
      resBuff[DATASIZE + 4] = (crc >> 8);
      resBuff[DATASIZE + 5] = crc;
      packetSerial.send(resBuff, DATASIZE + 6);
      return;
    } else if (buffer[0] == VERSION && buffer[1] == 0x01) {
      memcpy(cache, buffer + 2, DATASIZE);
      if (preferences.putBytes("data", cache, DATASIZE) > 0) {
        resBuff[1] = 0x00;
        uint32_t crc = CRC32::calculate(resBuff, 2);
        resBuff[2] = (crc >> 24);
        resBuff[3] = (crc >> 16);
        resBuff[4] = (crc >> 8);
        resBuff[5] = crc;
        packetSerial.send(resBuff, 6);
        return;
      }
    }
  }
  resBuff[1] = 0x01;
  uint32_t crc = CRC32::calculate(resBuff, 2);
  resBuff[2] = (crc >> 24);
  resBuff[3] = (crc >> 16);
  resBuff[4] = (crc >> 8);
  resBuff[5] = crc;
  packetSerial.send(resBuff, 6);
}

void showTask(void* pvParameters) {
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    if (xSemaphoreTake(showSemaphore, portMAX_DELAY) == pdTRUE) {
      if (leds.CanShow()) {
        leds.Show();
      }
    }
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(10));
  }
}

void dmxTask(void* pvParameters) {
  bool dmxIsConnected = false;
  for (;;) {
    dmx_packet_t packet;

    leds.ClearTo(RgbColor(0, 0, 0));
    leds.SetPixelColor(0, RgbColor(INDICATOR_BRIGHTNESS, 0, 0));

    if (dmx_receive(DMX_NUM_1, &packet, DMX_TIMEOUT_TICK)) {
      if (!packet.err) {
        retry = 0;
        if (!dmxIsConnected) {
          dmxIsConnected = true;
        }
        leds.SetPixelColor(0, RgbColor(0, INDICATOR_BRIGHTNESS, 0));
        dmx_read(DMX_NUM_1, dmxData, packet.size);

        float colorScale = cache[0] / 255.0f;
        size_t c = 1;
        while (c + 2 < packet.size && (4 * (c - 1) + 4) < DATASIZE) {
          uint16_t start = ((uint16_t)cache[4 * (c - 1) + 1] << 8) | ((uint16_t)cache[4 * (c - 1) + 2]);
          uint16_t end = ((uint16_t)cache[4 * (c - 1) + 3] << 8) | ((uint16_t)cache[4 * (c - 1) + 4]);
          if (start * end == 0) {
            c++;
            continue;
          }
          if (start > LEDNUM - 1) {
            start = LEDNUM - 1;
          }
          if (end > LEDNUM - 1) {
            end = LEDNUM - 1;
          }
          if (start > end) {
            start ^= end;
            end ^= start;
            start ^= end;
          }
          uint8_t r = dmxData[c];
          uint8_t g = dmxData[c + 1];
          uint8_t b = dmxData[c + 2];
          if (r + g + b > 0) {
            RgbColor color(
                (uint8_t)(r * colorScale),
                (uint8_t)(g * colorScale),
                (uint8_t)(b * colorScale)
                );
            for (uint16_t k = start; k <= end; k++) {
              leds.SetPixelColor(k, color);
            }
          }
          c += 3;
        }
      }
    } else if (dmxIsConnected) {
      dmxIsConnected = false;
      retry = millis();
    }
    xSemaphoreGive(showSemaphore);
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setRxBufferSize(BUFFSIZE);
  
  packetSerial.setStream(&Serial);
  packetSerial.setPacketHandler(&onPacketReceived);

  preferences.begin("store");
  if (preferences.isKey("data")) {
    preferences.getBytes("data", cache, DATASIZE);
  } else {
    memset(cache, 0, DATASIZE);
    preferences.putBytes("data", cache, DATASIZE);
  }

  dmx_config_t config = DMX_CONFIG_DEFAULT;
  dmx_personality_t personalities[] = {
    {3, "RGB"}
  };
  uint8_t personality_count = 1;
  dmx_driver_install(DMX_NUM_1, &config, personalities, personality_count);
  dmx_set_pin(DMX_NUM_1, -1, DMXPIN, -1);

  leds.Begin();
  leds.SetPixelColor(0, RgbColor(INDICATOR_BRIGHTNESS, INDICATOR_BRIGHTNESS, 0));
  leds.Show();

  showSemaphore = xSemaphoreCreateBinary();
  xTaskCreate(dmxTask, "DMX Task", 2048, NULL, 1, NULL);
  xTaskCreate(showTask, "Show Task", 2048, NULL, 1, NULL);
}

void loop() {
  packetSerial.update();
  if (retry > 0 && millis() - retry > 1000) {
    ESP.restart();
  }
  delay(1);
}
