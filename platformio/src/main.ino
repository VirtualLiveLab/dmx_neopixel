#include <esp_dmx.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#define SW1PIN 42
#define SW2PIN 41
#define SW3PIN 40
#define SW4PIN 39
#define SW5PIN 38
#define SW6PIN 9
#define SW7PIN 10
#define SW8PIN 11
#define SW9PIN 12
#define MODEPIN 14
#define LEDPIN 13
#define DMXPIN 44

DNSServer dnsServer;
AsyncWebServer server(80);
Adafruit_NeoPixel leds(1, LEDPIN, NEO_GRB + NEO_KHZ800);

class CaptiveRequestHandler : public AsyncWebHandler {
public:
  bool canHandle(__unused AsyncWebServerRequest *request) const override {
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    request->redirect(String("http://") + WiFi.softAPIP().toString().c_str());
  }
};

uint16_t startch;
long brightness, lednum, ledgrp;
bool configMode = false;
dmx_port_t dmxPort = 1;
byte data[DMX_PACKET_SIZE];
bool dmxIsConnected = false;

void readSW() {
  pinMode(SW1PIN, INPUT_PULLUP);
  pinMode(SW2PIN, INPUT_PULLUP);
  pinMode(SW3PIN, INPUT_PULLUP);
  pinMode(SW4PIN, INPUT_PULLUP);
  pinMode(SW5PIN, INPUT_PULLUP);
  pinMode(SW6PIN, INPUT_PULLUP);
  pinMode(SW7PIN, INPUT_PULLUP);
  pinMode(SW8PIN, INPUT_PULLUP);
  pinMode(SW9PIN, INPUT_PULLUP);
  startch = 0;
  if (!digitalRead(SW9PIN)) {
    startch++;
  }
  startch <<= 1;
  if (!digitalRead(SW8PIN)) {
    startch++;
  }
  startch <<= 1;
  if (!digitalRead(SW7PIN)) {
    startch++;
  }
  startch <<= 1;
  if (!digitalRead(SW6PIN)) {
    startch++;
  }
  startch <<= 1;
  if (!digitalRead(SW5PIN)) {
    startch++;
  }
  startch <<= 1;
  if (!digitalRead(SW4PIN)) {
    startch++;
  }
  startch <<= 1;
  if (!digitalRead(SW3PIN)) {
    startch++;
  }
  startch <<= 1;
  if (!digitalRead(SW2PIN)) {
    startch++;
  }
  startch <<= 1;
  if (!digitalRead(SW1PIN)) {
    startch++;
  }
}

void loadROM() {
  EEPROM.get(0, brightness);
  EEPROM.get(4, lednum);
  EEPROM.get(8, ledgrp);
}

void dmxSetup() {
  dmx_config_t config = DMX_CONFIG_DEFAULT;
  dmx_personality_t personalities[] = {
    {1, "Default Personality"}
  };
  int personality_count = 1;
  dmx_driver_install(dmxPort, &config, personalities, personality_count);
  dmx_set_pin(dmxPort, -1, DMXPIN, -1);
  loadROM();
  leds.updateLength(lednum * ledgrp + 1);
  leds.begin();
  leds.setBrightness(brightness);
  leds.clear();
  leds.setPixelColor(0, leds.Color(255, 255, 0));
  leds.show();
}

void dmxLoop() {
  readSW();
  dmx_packet_t packet;
  leds.clear();
  leds.setPixelColor(0, leds.Color(255, 0, 0));
  if (dmx_receive(dmxPort, &packet, DMX_TIMEOUT_TICK)) {
    if (!packet.err) {
      if (!dmxIsConnected) {
        dmxIsConnected = true;
      }
      leds.setPixelColor(0, leds.Color(0, 255, 0));
      dmx_read(dmxPort, data, packet.size);
      for (int i = 1; i <= 512; i++) {
        Serial.print(data[i]); Serial.print(" ");
      }
      Serial.println("");
      for (int g = 0; g < ledgrp; g++) {
        for (int i = 0; i < lednum; i++) {
          if (startch + g * 3 + 2 > 512) break;
          leds.setPixelColor(g * lednum + i + 1, leds.Color(data[startch + g * 3], data[startch + g * 3 + 1], data[startch + g * 3 + 2]));
        }
      }
    }
  } else if (dmxIsConnected) {
    ESP.restart();
  }
  leds.show();
}

void configSetup() {
  loadROM();
  leds.begin();
  leds.setBrightness(brightness);
  leds.clear();
  leds.setPixelColor(0, leds.Color(0, 0, 255));
  leds.show();

  LittleFS.begin(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("DMX NeoPixel Config");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    loadROM();
    request->send(LittleFS, "/main.html", "text/html", false, [](const String &var) -> String {
      if (var == "brightness") {
        return String(brightness);
      }
      if (var == "lednum") {
        return String(lednum);
      }
      if (var == "ledgrp") {
        return String(ledgrp);
      }
      return emptyString;
    });
  });

  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("brightness", true)) {
      brightness = request->getParam("brightness", true)->value().toInt();
      EEPROM.put(0, brightness);
    }
    if (request->hasParam("lednum", true)) {
      lednum = request->getParam("lednum", true)->value().toInt();
      EEPROM.put(4, lednum);
    }
    if (request->hasParam("ledgrp", true)) {
      ledgrp = request->getParam("ledgrp", true)->value().toInt();
      EEPROM.put(8, ledgrp);
    }
    EEPROM.commit();
    request->send(LittleFS, "/result.html", "text/html", false, [](const String &var) -> String {
      if (var == "brightness") {
        return String(brightness);
      }
      if (var == "lednum") {
        return String(lednum);
      }
      if (var == "ledgrp") {
        return String(ledgrp);
      }
      return emptyString;
    });
  });

  dnsServer.start(53, "*", WiFi.softAPIP());
  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
  server.begin();
}

void configLoop() {
  dnsServer.processNextRequest();
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  EEPROM.begin(12);
  pinMode(MODEPIN, INPUT_PULLUP);
  if (!digitalRead(MODEPIN)) {
    Serial.println("config mode");
    configMode = true;
  }
  if (configMode) {
    configSetup();
  } else {
    dmxSetup();
  }
}

void loop() {
  if (configMode) {
    configLoop();
  } else {
    dmxLoop();
  }
}
