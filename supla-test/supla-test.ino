#include <WiFi.h>
#include <SuplaDevice.h>

#define WIFI_SSID     "Pawel_LTE"
#define WIFI_PASSWORD "pawel2580s"

#define SUPLA_GUID    "0000000000002593"
#define SUPLA_AUTHKEY "d39f0361"

#define RELAY_PIN 2

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  // Inicjalizacja SuplaDevice - tylko GUID i AUTHKEY
  SuplaDevice.begin(SUPLA_GUID, SUPLA_AUTHKEY);

  // Dodaj kanał przekaźnika (typ relay, pin RELAY_PIN)
  SuplaDevice.addChannel(RELAY_PIN, SUPLA_CHANNELTYPE_RELAY);

  SuplaDevice.setup();

  Serial.println("SuplaDevice setup completed");
}

void loop() {
  SuplaDevice.loop();
}
