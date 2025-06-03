#include <Arduino.h>
#include <esp_dmx.h>
#include <Adafruit_NeoPixel.h>

#define DEBUG
#define BRIGHTNESS 10
#define LEDNUM 30
#define LEDGRP ((size_t[][3]){{1, 30, 1}, {1, 3, 10}, {16, 15, 1}})

#define LEDPIN 1
#define DMXPIN 44
#define SWPINS ((uint8_t[10]){10, 9, 8, 7, 6, 5, 4, 3, 2, 11})
#define INDICATOR_BRIGHTNESS 10
#define SWPINS_NUM (sizeof(SWPINS) / sizeof(SWPINS[0]))
#define LEDGRP_NUM (sizeof(LEDGRP) / sizeof(LEDGRP[0]))

Adafruit_NeoPixel leds(LEDNUM + 1, LEDPIN, NEO_GRB + NEO_KHZ800);

byte data[DMX_PACKET_SIZE];

bool dmxIsConnected = false;
size_t channelNum;
size_t *channelMap;

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
#endif

  dmx_config_t config = DMX_CONFIG_DEFAULT;
  dmx_personality_t personalities[] = {
    {3, "RGB"}
  };
  uint8_t personality_count = 1;
  dmx_driver_install(DMX_NUM_1, &config, personalities, personality_count);
  dmx_set_pin(DMX_NUM_1, -1, DMXPIN, -1);

  for (uint16_t i = 0; i < SWPINS_NUM; i++) {
    pinMode(SWPINS[i], INPUT_PULLUP);
  }

  for (size_t i = 0; i < LEDGRP_NUM; i++) {
    channelNum += LEDGRP[i][2];
  }
  channelMap = (size_t*)malloc(sizeof(size_t) * channelNum * 2);
  size_t c = 0;
  for (size_t i = 0; i < LEDGRP_NUM; i++) {
    for (size_t j = 0; j < LEDGRP[i][2]; j++) {
      channelMap[c * 2] = LEDGRP[i][0] + LEDGRP[i][1] * j;
      channelMap[c * 2 + 1] = LEDGRP[i][0] + LEDGRP[i][1] * (j + 1) - 1;
      c++;
    }
  }

  leds.begin();
  leds.clear();
  leds.setPixelColor(0, leds.Color(INDICATOR_BRIGHTNESS, INDICATOR_BRIGHTNESS, 0));
  leds.show();
}

void loop() {
  uint16_t startChannel = 0;
  for (uint16_t i = 0; i < SWPINS_NUM; i++) {
    startChannel |= (!digitalRead(SWPINS[i]) << i);
  }

  dmx_packet_t packet;
  leds.clear();
  leds.setPixelColor(0, leds.Color(INDICATOR_BRIGHTNESS, 0, 0));
  if (dmx_receive(DMX_NUM_1, &packet, DMX_TIMEOUT_TICK)) {
    if (!packet.err) {
      if (!dmxIsConnected) {
        dmxIsConnected = true;
      }
      leds.setPixelColor(0, leds.Color(0, INDICATOR_BRIGHTNESS, 0));
      dmx_read(DMX_NUM_1, data, packet.size);

#ifdef DEBUG
      for (uint16_t i = 1; i <= 512; i++) {
        Serial.print(data[i]); Serial.print(" ");
      }
      Serial.println("");
#endif

      for (size_t c = 0; c < channelNum; c++) {
        size_t d = startChannel + c * 3;
        if (d + 2 > 512) break;
        uint32_t color = leds.Color(data[d] * BRIGHTNESS / 255, data[d + 1] * BRIGHTNESS / 255, data[d + 2] * BRIGHTNESS / 255);
        if (color > 0) {
          for (size_t k = channelMap[c * 2]; k <= channelMap[c * 2 + 1]; k++) {
            if (k > LEDNUM) break;
            leds.setPixelColor(k, color);
          }
        }
      }
    }
  } else if (dmxIsConnected) {
    ESP.restart();
  }
  leds.show();
}
