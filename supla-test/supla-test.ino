#include <WiFi.h>
#include <SuplaDevice.h>

#define WIFI_SSID     "Pawel_LTE"
#define WIFI_PASSWORD "pawel2580s"

#define SUPLA_GUID    "0000000000002593"  // 16 znaków HEX
#define SUPLA_AUTHKEY "d39f0361"          // 8 znaków HEX

#define RELAY_PIN 2

SuplaDevice device;

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");

  device.begin(SUPLA_GUID, SUPLA_AUTHKEY, WIFI_SSID, WIFI_PASSWORD, SUPLA_CHANNELTYPE_RELAY, RELAY_PIN);

  Serial.println("SuplaDevice started");
}

void loop() {
  device.loop();
}
