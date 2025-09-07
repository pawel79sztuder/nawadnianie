#include <WiFi.h>
#include <supla/esp32.h>

#define WIFI_SSID     "Pawel_LTE"
#define WIFI_PASSWORD "pawel2580s"

#define SUPLA_GUID    "2593"
#define SUPLA_AUTHKEY "d39f0361"

#define RELAY_PIN 2

Supla::ESPWifi wifi(WIFI_SSID, WIFI_PASSWORD);
Supla::ESP32 device(wifi, SUPLA_GUID, SUPLA_AUTHKEY);

void onRelayChanged(bool state) {
  digitalWrite(RELAY_PIN, state ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Dodanie kanału przekaźnika z callbackiem
  device.addRelay(1, onRelayChanged);

  device.begin();
  Serial.println("SuplaDevice started");
}

void loop() {
  device.loop();
}
