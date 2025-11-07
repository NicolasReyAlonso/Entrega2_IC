constexpr uint32_t serial_monitor_bauds = 9600;
constexpr uint32_t serial1_bauds = 9600;

#ifdef ARDUINO_AVR_UNO
#include <SoftwareSerial.h>
constexpr uint8_t RX_PIN = 8;
constexpr uint8_t TX_PIN = 9;
SoftwareSerial Serial1(RX_PIN, TX_PIN);  // RX, TX
#endif

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(serial_monitor_bauds);
  while (!Serial)
    ;
  Serial1.begin(serial1_bauds);
}

void loop() {
  if (Serial1.available() > 0) {
    uint8_t cmd = Serial1.read();

    Serial.print("<-- recibido: ");
    Serial.println(cmd, BIN);

    switch (cmd) {
      case 0x10:  // Simular respuesta tipo REGULAR
        Serial1.write(uint8_t(0b00000010));  // Header (termina en 10)
        Serial1.write(uint8_t(5));           // Sensor Y
        Serial1.write(uint8_t(0b00000001));  // Unidad cm
        Serial1.write(uint8_t(15));          // Valor W
        break;

      case 0x01:  // Simular respuesta tipo STATUS
        Serial1.write(uint8_t(0b00000001));  // Header (termina en 01)
        Serial1.write(uint8_t(10));          // Dir I2C (Y)
        Serial1.write(uint8_t(12));          // Delay (Z)
        Serial1.write(uint8_t(0b00000010));  // Unidad inc (W)
        Serial1.write(uint8_t(0));           // Periodic ON (V)
        break;

      case 0xFF:  // Error
        Serial1.write(uint8_t(0xFF));
        break;

      default:  // Código desconocido
        Serial1.write(uint8_t(0x77));
        break;
    }

    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}
