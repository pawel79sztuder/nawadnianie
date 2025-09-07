#include <WiFi.h>
#include <SuplaDevice.h>

// Dane do WiFi
#define WIFI_SSID     "Pawel_LTE"
#define WIFI_PASSWORD "pawel2580s"

// Dane Supla - wpisz swoje własne (GUID 16 znaków, AuthKey 8 znaków HEX)
#define SUPLA_GUID    "0000000000002593"
#define SUPLA_AUTHKEY "d39f0361"

#define RELAY_PIN 2

// Utwórz kanał urządzenia (np. przekaźnik)
SuplaDeviceChannel relayChannel;

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Połącz z WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");

  // Inicjalizacja SuplaDevice
  Supla.begin(SUPLA_GUID, SUPLA_AUTHKEY);

  // Konfiguracja kanału: przekaźnik, GPIO 2
  relayChannel.setType(SUPLA_CHANNEL_TYPE_RELAY);
  relayChannel.setGpio(RELAY_PIN);

  // Dodaj kanał do SuplaDevice
  Supla.addChannel(&relayChannel);

  // Finalizacja konfiguracji Supla
  Supla.setup();
}

void loop() {
  // Musisz wywoływać loop Supla, by działała komunikacja z serwerem
  Supla.loop();
}
