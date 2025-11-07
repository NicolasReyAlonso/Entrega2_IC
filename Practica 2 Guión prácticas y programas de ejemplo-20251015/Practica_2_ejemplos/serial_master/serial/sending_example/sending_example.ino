/* ----------------------------------------------------------------------
 *  Ejemplo sending_example.ino 
 *    Este ejemplo muestra como utilizar el puerto serie uart (Serial1) 
 *    para comunicarse con otro dispositivo.
 *    
 *  Asignatura (GII-IC)
 * ---------------------------------------------------------------------- 
 */
#include <LiquidCrystal_I2C.h>

constexpr const uint32_t serial_monitor_bauds = 115200;
constexpr const uint32_t serial1_bauds = 9600;

constexpr const uint32_t pseudo_period_ms = 1000;

String slaveBuffer = "";

uint8_t counter = 0;
uint8_t led_state = LOW;
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  // Configuración del LED incluido en placa
  // Inicialmente apagado
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, led_state);
  led_state = (led_state + 1) & 0x01;

  // Inicialización del puerto para el serial monitor
  Serial.begin(serial_monitor_bauds);
  while (!Serial);
  lcd.init();           // inicializa el LCD
  lcd.backlight();      // enciende la luz de fondo
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LCD listo!");
  delay(1000);
  lcd.clear();
  // Inicialización del puerto de comunicaciones con el otro dispositivo MKR
  Serial1.begin(serial1_bauds);
}

void loop() {
    
     while (Serial1.available() > 0) {
    char c = Serial1.read();
    if (c == '\n') {
      parseSlaveMessage(slaveBuffer);
      slaveBuffer = "";
    } else {
      slaveBuffer += c;
    }
  }
    if (Serial.available() > 0) {
      String input = Serial.readStringUntil('\n');
      parseCommand(input);
    }
  




}

void parseCommand(String input) {
  input.trim();
  if (input.length() == 0) return;

  // Dividir por espacios
  String tokens[5];
  int count = 0;
  int last = 0;
  for (int i = 0; i <= input.length(); i++) {
    if (input[i] == ' ' || i == input.length()) {
      tokens[count++] = input.substring(last, i);
      tokens[count - 1].trim();
      last = i + 1;
      if (count >= 5) break;
    }
  }

  // ===== HELP =====
  if (tokens[0] == "help") {
    Serial.println("[CMD] help -> 0XXXXXXX");
    uint8_t code = 0b00000000;  // ejemplo base
    Serial1.write(code);
    return;
  }

  // ===== US commands =====
  if (tokens[0] == "us") {
    // Solo 'us' => listar sensores
    if (count == 1) {
      Serial.println("[CMD] us -> 100");
      Serial.println("Comandos disponibles para 'us':");
      Serial.println("  us <srf02> one-shot           -> Dispara una sola vez el sensor.");
      Serial.println("  us <srf02> on <period_ms>     -> Activa disparo periódico.");
      Serial.println("  us <srf02> off                -> Detiene disparo periódico.");
      Serial.println("  us <srf02> unit {inc|cm|ms}   -> Cambia la unidad de medida.");
      Serial.println("  us <srf02> delay <ms>         -> Establece retardo mínimo entre disparos.");
      Serial.println("  us <srf02> status             -> Muestra configuración del sensor.");
      Serial.println("  us                            -> Lista esta ayuda y los sensores disponibles.\n");

      uint8_t code = 0b10000000;
      Serial1.write(code);
      return;
    }


    int srf = tokens[1].toInt();  // número de sensor
    uint8_t sensor_id = srf & 0xFF;

    // ---- UNIT ----
    if (tokens[2] == "unit" && count >= 4) {
      uint8_t unit_bits = 0b00;
      if (tokens[3] == "ms") unit_bits = 0b00;
      else if (tokens[3] == "cm") unit_bits = 0b01;
      else if (tokens[3] == "inc") unit_bits = 0b10;

      uint8_t header = 0b11010000 | (unit_bits << 2);
      Serial.print("[CMD] us ");
      Serial.print(srf);
      Serial.print(" unit ");
      Serial.println(tokens[3]);
      Serial1.write(header);
      Serial1.write(sensor_id);
      return;
    }

    // ---- one-shot / on / off ----
    if (tokens[2] == "one-shot" || tokens[2] == "on" || tokens[2] == "off") {
      uint8_t mode_bits = 0b00;
      if (tokens[2] == "one-shot") mode_bits = 0b00;
      else if (tokens[2] == "off") mode_bits = 0b01;
      else if (tokens[2] == "on") mode_bits = 0b10;

      uint16_t period = (tokens[2] == "on" && count >= 4) ? tokens[3].toInt() : 0;
      uint8_t header = 0b11000000 | (mode_bits << 2);
      Serial.print("[CMD] us ");
      Serial.print(srf);
      Serial.print(" ");
      Serial.println(tokens[2]);
      Serial1.write(header);
      Serial1.write(sensor_id);
      Serial1.write(period & 0xFF);
      Serial1.write(period >> 8);
      return;
    }

    // ---- delay ----
    if (tokens[2] == "delay" && count >= 4) {
      uint16_t delay_ms = tokens[3].toInt();
      uint8_t header = 0b11100000;
      Serial.print("[CMD] us ");
      Serial.print(srf);
      Serial.print(" delay ");
      Serial.println(delay_ms);
      Serial1.write(header);
      Serial1.write(sensor_id);
      Serial1.write(delay_ms & 0xFF);
      Serial1.write(delay_ms >> 8);
      return;
    }

    // ---- status ----
    if (tokens[2] == "status") {
      uint8_t header = 0b11110000;
      Serial.print("[CMD] us ");
      Serial.print(srf);
      Serial.println(" status");
      Serial1.write(header);
      Serial1.write(sensor_id);
      return;
    }

    Serial.println("[ERROR] Comando US no reconocido.");
  } else {
    Serial.println("[ERROR] Comando no reconocido.");
  }
}
void parseSlaveMessage(String msg) {
  Serial.print("[RAW BYTES] ");
for (int i = 0; i < msg.length(); i++) {
  Serial.print((uint8_t)msg[i], HEX);
  Serial.print(' ');
}
Serial.println();

  msg.trim();
  if (msg.length() == 0) return;

  lcd.clear();
  lcd.setCursor(0, 0);

  Serial.print("[SLAVE] Mensaje: ");
  Serial.println(msg);

  // Caso especial: error
  if (msg == "11111111") {
    Serial.println("[ERROR] Respuesta de error del esclavo");
    lcd.print("Error del esclavo");
    return;
  }

  // Validar longitud mínima (8 bits + campos)
  if (msg.length() < 8) {
    Serial.println("CODE NOT FOUND");
    lcd.print("CODE NOT FOUND");
    return;
  }

  // Leer bits base
  String header = msg.substring(0, 8);
  String tail = msg.substring(8);

  if (header.endsWith("10")) {
    // Regular response: XXXXXX10 + YYYYYYYY + ZZZZZZZZ + WWWWWWWW
    if (tail.length() < 24) {
      Serial.println("CODE NOT FOUND");
      lcd.print("CODE NOT FOUND");
      return;
    }

    String Y = tail.substring(0, 8);
    String Z = tail.substring(8, 16);
    String W = tail.substring(16, 24);

    int unidad_code = binToDec(Z) & 0b11;
    String unidad = (unidad_code == 0b00) ? "ms" :
                    (unidad_code == 0b01) ? "cm" :
                    (unidad_code == 0b10) ? "inc" : "unk";

    Serial.println("[SLAVE] Respuesta regular:");
    Serial.print("  Sensor: "); Serial.println(binToDec(Y));
    Serial.print("  Unidad: "); Serial.println(unidad);
    Serial.print("  Valor : "); Serial.println(binToDec(W));

    lcd.print("Sensor "); lcd.print(binToDec(Y));
    lcd.setCursor(0, 1);
    lcd.print(binToDec(W)); lcd.print(" "); lcd.print(unidad);
  }

  else if (header.endsWith("01")) {
    // Status response: XXXXXX01 + YYYYYYYY + ZZZZZZZZ + WWWWWWWW + VVVVVVVV
    if (tail.length() < 32) {
      Serial.println("CODE NOT FOUND");
      lcd.print("CODE NOT FOUND");
      return;
    }

    String Y = tail.substring(0, 8);
    String Z = tail.substring(8, 16);
    String W = tail.substring(16, 24);
    String V = tail.substring(24, 32);

    int unidad_code = binToDec(W) & 0b11;
    String unidad = (unidad_code == 0b00) ? "ms" :
                    (unidad_code == 0b01) ? "cm" :
                    (unidad_code == 0b10) ? "inc" : "unk";

    String periodic = (binToDec(V) == 0b00) ? "ON" : "OFF";

    Serial.println("[SLAVE] Status response:");
    Serial.print("  I2C Addr: "); Serial.println(binToDec(Y));
    Serial.print("  Delay ms: "); Serial.println(binToDec(Z));
    Serial.print("  Unidad  : "); Serial.println(unidad);
    Serial.print("  Periodic: "); Serial.println(periodic);

    lcd.print("I2C "); lcd.print(binToDec(Y));
    lcd.setCursor(0, 1);
    lcd.print(unidad); lcd.print(" "); lcd.print(periodic);
  }

  else {
    Serial.println("CODE NOT FOUND");
    lcd.print("CODE NOT FOUND");
  }
}


int binToDec(String bits) {
  int val = 0;
  for (int i = 0; i < bits.length(); i++) {
    val = (val << 1) | (bits[i] == '1' ? 1 : 0);
  }
  return val;
}

