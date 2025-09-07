#include <WiFi.h>
#include <SuplaDevice.h>
#include <SuplaDeviceESP32.h>  // ważne dla ESP32!

#define WIFI_SSID     "Pawel_LTE"
#define WIFI_PASSWORD "pawel2580s"

// GUID musi mieć dokładnie 16 znaków (wypełnij zerami z przodu)
#define SUPLA_GUID    "0000000000002593"  
#define SUPLA_AUTHKEY "d39f0361"         // 8 znaków

#define RELAY_PIN 2

// Utwórz obiekt kanału typu przełącznik (relay)
SuplaSwitch relay(1, RELAY_PIN);

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");

  // Uruchomienie SuplaDevice z wymaganymi parametrami
  bool started = SuplaDevice.begin(
    SUPLA_GUID,
    SUPLA_AUTHKEY,
    WIFI_SSID,
    WIFI_PASSWORD,
    23  // protokół SUPLA
  );

  if (!started) {
    Serial.println("Failed to start SuplaDevice");
  } else {
    Serial.println("SuplaDevice started");
  }

  // Dodanie kanału (przekaźnika)
  SuplaDevice.addChannel(&relay);
}

void loop() {
  SuplaDevice.loop();
}
