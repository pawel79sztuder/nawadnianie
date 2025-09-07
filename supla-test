#include <WiFi.h>
#include <SuplaDevice.h>

// Dane WiFi
#define WIFI_SSID     "Twoja_Siec_WiFi"
#define WIFI_PASSWORD "Twoje_Haslo_WiFi"

// Dane Supla
#define SUPLA_SERVER  "my.supla.org"   // lub adres Twojego serwera Supla
#define SUPLA_PORT    2015
#define SUPLA_LOGIN   "twoj_email@example.com"
#define SUPLA_PASS    "twoje_haslo_supla"

// Przekaźnik podłączony do GPIO 2
#define RELAY_PIN 2

SuplaDevice device(1);            // ID urządzenia w Supli, możesz zmienić na swoje
SuplaSwitch relay(1, RELAY_PIN);  // kanał 1, pin 2 (GPIO2)

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
