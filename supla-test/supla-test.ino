#include <WiFi.h>
#include <SuplaDevice.h>
#include <SuplaSwitch.h>  // DODAJ TEN INCLUDE

// Dane WiFi
#define WIFI_SSID     "Pawel_LTE"
#define WIFI_PASSWORD "pawel2580s"

// Dane Supla
#define SUPLA_SERVER  "my.supla.org"
#define SUPLA_PORT    2015
#define SUPLA_LOGIN   "psztuder223@interia.pl"
#define SUPLA_PASS    "pawel2580s"

// Przekaźnik podłączony do GPIO 2
#define RELAY_PIN 2

SuplaDevice device(1);
SuplaSwitch relay(1, RELAY_PIN);

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  Serial.println("Laczenie z WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi polaczone");

  device.setServer(SUPLA_SERVER, SUPLA_PORT);
  device.setLogin(SUPLA_LOGIN);
  device.setPassword(SUPLA_PASS);

  device.addChannel(&relay);

  device.begin();
  Serial.println("Supla started");
}

void loop() {
  device.loop();
}
