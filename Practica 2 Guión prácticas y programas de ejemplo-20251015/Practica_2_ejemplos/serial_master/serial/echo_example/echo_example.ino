/* Ejemplo "echo" compatible con UNO y MKR */

constexpr uint32_t serial_monitor_bauds = 9600;
constexpr uint32_t serial1_bauds = 9600;
constexpr uint32_t pseudo_period_ms = 1000;

uint8_t led_state = LOW;

#ifdef ARDUINO_AVR_UNO
#include <SoftwareSerial.h>
constexpr uint8_t RX_PIN = 8;
constexpr uint8_t TX_PIN = 9;
SoftwareSerial Serial1(RX_PIN, TX_PIN);  // RX, TX
#endif

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, led_state);
  led_state = (led_state + 1) & 0x01;

  Serial.begin(serial_monitor_bauds);
  while (!Serial)
    ;

  Serial1.begin(serial1_bauds);
}

void loop() {
  Serial.println("******************** echo example *********************");

  uint32_t last_ms = millis();
  bool received = false;

  while (millis() - last_ms < pseudo_period_ms) {
    if (Serial1.available() > 0) {
      uint8_t data = Serial1.read();
      Serial.print("<-- received: ");
      Serial.println(static_cast<int>(data));
      Serial.print("--> sending back: ");
      Serial.println(static_cast<int>(data));
      Serial1.write(data);
      received = true;
      break;
    }
  }

  if (!received)
    Serial.println("<-- received: TIMEOUT!!");

  Serial.println("*******************************************************");

  digitalWrite(LED_BUILTIN, led_state);
  led_state = (led_state + 1) & 0x01;

  delay(pseudo_period_ms);
}
