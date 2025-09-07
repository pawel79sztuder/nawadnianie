#include <WiFi.h>
#include <SuplaDevice.h>
#include <SuplaDeviceGui.h>

// Twoje dane WiFi
#define WIFI_SSID     "Pawel_LTE"
#define WIFI_PASSWORD "pawel2580s"

// Dane SUPLA - pamiętaj, że GUID musi mieć 16 znaków, a AUTHKEY 8 znaków
#define SUPLA_GUID    "000000000002593"  // tu przykładowo dodaj brakujące zera, 16 znaków
#define SUPLA_AUTHKEY "d39f0361"         // 8 znaków

#define RELAY_PIN 2

SuplaDeviceGui gui;

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Połączenie WiFi - możesz dodać timeout itp.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");

  // Uruchomienie SuplaDevice z wymaganymi 5 argumentami
  bool started = SuplaDevice.begin(
    SUPLA_GUID,
    SUPLA_AUTHKEY,
    WIFI_SSID,
    WIFI_PASSWORD,
    23 // wersja protokołu SUPLA (domyślna 23)
  );

  if (!started) {
    Serial.println("Failed to start SuplaDevice");
  } else {
    Serial.println("SuplaDevice started");
  }

  // Dodaj przełącznik (relay) pod pinem RELAY_PIN do GUI Supla (automatycznie wyśle stan)
  gui.addSwitch(RELAY_PIN);
}

void loop() {
  SuplaDevice.loop();
}
