#include <Arduino.h>
#include <esp_dmx.h>
#include <Adafruit_NeoPixel.h>

#define SWPINS ((uint8_t[10]){10, 9, 8, 7, 6, 5, 4, 3, 2, 11})
#define LEDPIN 1
#define DMXPIN 44
#define BRIGHTNESS 10
#define LEDNUM 30
#define LEDGRP ((int[][3]){{1, 30, 1}, {1, 3, 10}, {16, 15, 1}})

Adafruit_NeoPixel leds(LEDNUM + 1, LEDPIN, NEO_GRB + NEO_KHZ800);

byte data[DMX_PACKET_SIZE];

bool dmxIsConnected = false;
int channelNum;
int **channelMap;

void readSW(int *startChannel) {
  *startChannel = 0;
  for (int i = sizeof(SWPINS) - 1; i >= 0; i--) {
    if (!digitalRead(SWPINS[i])) {
      *startChannel += 1;
    }
    if (i > 0) {
      *startChannel <<= 1;
    }
  }
}

void setup() {
  Serial.begin(115200);

  dmx_config_t config = DMX_CONFIG_DEFAULT;
  dmx_personality_t personalities[] = {
    {3, "RGB"}
  };
  int personality_count = 1;
  dmx_driver_install(DMX_NUM_1, &config, personalities, personality_count);
  dmx_set_pin(DMX_NUM_1, -1, DMXPIN, -1);

  for (int i = 0; i < sizeof(SWPINS); i++) {
    pinMode(SWPINS[i], INPUT_PULLUP);
  }

  for (int i = 0; i < sizeof(LEDGRP) / sizeof(LEDGRP[0]); i++) {
    channelNum += LEDGRP[i][2];
  }
  channelMap = (int**)malloc(sizeof(int) * channelNum);
  int c = 0;
  for (int i = 0; i < sizeof(LEDGRP) / sizeof(LEDGRP[0]); i++) {
    for (int j = 0; j < LEDGRP[i][2]; j++) {
      channelMap[c] = (int*)malloc(sizeof(int) * 2);
      channelMap[c][0] = LEDGRP[i][0] + LEDGRP[i][1] * j;
      channelMap[c][1] = LEDGRP[i][0] + LEDGRP[i][1] * (j + 1) - 1;
      c++;
    }
  }

  leds.begin();
  leds.setBrightness(BRIGHTNESS);
  leds.clear();
  leds.setPixelColor(0, leds.Color(255, 255, 0));
  leds.show();
}

void loop() {
  int startChannel;
  readSW(&startChannel);
  dmx_packet_t packet;
  leds.clear();
  leds.setPixelColor(0, leds.Color(255, 0, 0));
  if (dmx_receive(DMX_NUM_1, &packet, DMX_TIMEOUT_TICK)) {
    if (!packet.err) {
      if (!dmxIsConnected) {
        dmxIsConnected = true;
      }
      leds.setPixelColor(0, leds.Color(0, 255, 0));
      dmx_read(DMX_NUM_1, data, packet.size);
      for (int i = 1; i <= 512; i++) {
        Serial.print(data[i]); Serial.print(" ");
      }
      Serial.println("");

      for (int c = 0; c < channelNum; c++) {
        for (int k = channelMap[c][0]; k <= channelMap[c][1]; k++) {
          if (startChannel + c * 3 + 2 > 512 || k > LEDNUM) break;
          if (data[startChannel + c * 3] + data[startChannel + c * 3 + 1] + data[startChannel + c * 3 + 2] > 0) {
            leds.setPixelColor(k, leds.Color(data[startChannel + c * 3], data[startChannel + c * 3 + 1], data[startChannel + c * 3 + 2]));
          }
        }
      }
    }
  } else if (dmxIsConnected) {
    ESP.restart();
  }
  leds.show();
}
