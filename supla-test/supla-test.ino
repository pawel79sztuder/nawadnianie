#include <WiFi.h>
#include <SuplaDevice.h>
#include <SuplaDeviceChannel.h>  // kanały

#define WIFI_SSID     "Pawel_LTE"
#define WIFI_PASSWORD "pawel2580s"

#define SUPLA_GUID    "0000000000002593"
#define SUPLA_AUTHKEY "d39f0361"

#define RELAY_PIN 2

// Tworzymy kanał przekaźnika
SuplaDeviceChannel relayChannel;

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

  // Inicjalizujemy urządzenie Supla (protokół 23 jest standardem)
  SuplaDevice.begin(SUPLA_GUID, SUPLA_AUTHKEY);

  // Konfigurujemy kanał przekaźnika
  relayChannel.setType(SUPLA_CHANNELTYPE_RELAY);       // typ kanału relay
  relayChannel.setPin(RELAY_PIN);                       // podpinamy pin
  SuplaDevice.addChannel(&relayChannel);                // dodajemy kanał do urządzenia

  // Uruchamiamy SUPLA
  SuplaDevice.setup();
  Serial.println("SuplaDevice setup completed");
}

void loop() {
  SuplaDevice.loop();
}
