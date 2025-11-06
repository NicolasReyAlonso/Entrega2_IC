/* ----------------------------------------------------------------------
 *  Ejemplo sending_example.ino 
 *    Este ejemplo muestra como utilizar el puerto serie uart (Serial1) 
 *    para comunicarse con otro dispositivo.
 *    
 *  Asignatura (GII-IC)
 * ---------------------------------------------------------------------- 
 */

constexpr const uint32_t serial_monitor_bauds = 115200;
constexpr const uint32_t serial1_bauds = 9600;

constexpr const uint32_t pseudo_period_ms = 1000;

uint8_t counter = 0;
uint8_t led_state = LOW;

void setup() {
  // Configuración del LED incluido en placa
  // Inicialmente apagado
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, led_state);
  led_state = (led_state + 1) & 0x01;

  // Inicialización del puerto para el serial monitor
  Serial.begin(serial_monitor_bauds);
  while (!Serial)
    ;

  // Inicialización del puerto de comunicaciones con el otro dispositivo MKR
  Serial1.begin(serial1_bauds);
}

void loop() {
  Serial.println("******************* sending example *******************");

  Serial.print("--> sending: ");
  Serial.println(static_cast<int>(counter));
  Serial1.write(counter++);

  uint32_t last_ms = millis();
  while (millis() - last_ms < pseudo_period_ms) {
    if (Serial1.available() > 0) {
      uint8_t data = Serial1.read();
      Serial.print("<-- received: ");
      Serial.println(static_cast<int>(data));
      break;
    }
    if (Serial.available() > 0) {
      String input = Serial.readStringUntil('\n');
      parseCommand(input);
    }
  }

  if (millis() - last_ms < pseudo_period_ms) delay(pseudo_period_ms - (millis() - last_ms));
  else Serial.println("<-- received: TIMEOUT!!");

  Serial.println("*******************************************************");

  digitalWrite(LED_BUILTIN, led_state);
  led_state = (led_state + 1) & 0x01;
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
