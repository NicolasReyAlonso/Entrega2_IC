/* ----------------------------------------------------------------------
 *  Dispositivo esclavo con SRF02 + OLED
 *  Muestra solo el estado general en la OLED.
 * ---------------------------------------------------------------------- 
 */

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define SRF01_I2C_ADDRESS byte((0xE0)>>1)
#define SRF02_I2C_ADDRESS byte((0xF2)>>1)
#define SRF02_CMD_REG 0x00
#define SRF02_RANGE_HIGH 0x02

#define OLED_ADDR 0x3D
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

struct SensorConfig {
  uint8_t address;
  uint8_t unit;
  uint16_t delayMs;
  bool periodic;
  uint16_t periodMs;
  unsigned long lastShot;
  uint16_t lastMeasure;
};

SensorConfig sensors[2] = {
  {SRF01_I2C_ADDRESS, 1, 70, false, 0, 0, 0},
  {SRF02_I2C_ADDRESS, 1, 70, false, 0, 0, 0}
};

uint16_t readSRF02(uint8_t address, uint8_t unit) {
  uint8_t cmd = 0x51;
  if (unit == 2) cmd = 0x50;
  if (unit == 0) cmd = 0x56;

  Wire.beginTransmission(address);
  Wire.write(SRF02_CMD_REG);
  Wire.write(cmd);
  Wire.endTransmission();

  delay(70);

  Wire.beginTransmission(address);
  Wire.write(SRF02_RANGE_HIGH);
  Wire.endTransmission();

  Wire.requestFrom(address, (uint8_t)2);
  if (Wire.available() < 2) return 0xFFFF;
  uint16_t high = Wire.read();
  uint16_t low = Wire.read();
  return (high << 8) | low;
}

void sendResponse(uint8_t code, uint8_t* data = nullptr, uint8_t len = 0) {
  Serial1.write(code);
  if (data && len > 0) Serial1.write(data, len);
}

//Oled muestra estado sensores
void showStatus(const char* line1, const char* line2 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(line1);
  if (line2[0] != '\0') {
    display.setCursor(0, 15);
    display.println(line2);
  }
  display.display();
}

void handleCommand(uint8_t code) {
  uint8_t op = (code & 0xF0) >> 4;
  uint8_t param = code & 0x0F;

  switch (op) {
    case 0x0: // help
      const char helpMsg[] = 
          "0x0_: HELP\n"
          "0x8_: US LIST\n"
          "0xC_: US CMD (one-shot/on/off)\n"
          "0xD_: UNIT\n"
          "0xE_: DELAY\n"
          "0xF_: STATUS";
        
        uint8_t len = strlen(helpMsg);
        sendResponse(0x02, (uint8_t*)helpMsg, len);
        showStatus("Help enviado", "Comandos listados");
      break;

    case 0x8: // us mostrar sensores
      {
        uint8_t num = 2;
        sendResponse(0x02, &num, 1);
        showStatus("CMD: US LIST");
      }
      break;

    case 0xC: // one-shot / on / off
      {
        uint8_t mode = (param & 0x0C) >> 2;
        uint8_t sid = (param & 0x03);
        if (sid > 1) { sendResponse(0x01); showStatus("Error: sensor ID"); return; }

        SensorConfig &s = sensors[sid];
        if (mode == 0b00) { // one-shot
          s.lastMeasure = readSRF02(s.address, s.unit);
          uint8_t data[2] = { (uint8_t)(s.lastMeasure >> 8), (uint8_t)(s.lastMeasure & 0xFF) };
          sendResponse(0x03, data, 2);
          if (s.lastMeasure == 0xFFFF)
            showStatus("Error SRF02", "Lectura fallida");
          else
            showStatus("Medicion unica", "SENSOR OK");
        } 
        else if (mode == 0b01) { // off
          s.periodic = false;
          sendResponse(0x00);
          showStatus("Modo periodico", "DESACTIVADO");
        } 
        else if (mode == 0b10) { // on <period_ms>
          while (Serial1.available() < 2);
          uint8_t hi = Serial1.read();
          uint8_t lo = Serial1.read();
          s.periodMs = (hi << 8) | lo;
          s.periodic = true;
          s.lastShot = millis();
          sendResponse(0x00);
          showStatus("Modo periodico", "ACTIVADO");
        }
      }
      break;

    case 0xD: // unit
      {
        uint8_t unit = (param & 0x0C) >> 2;
        uint8_t sid = (param & 0x03);
        if (sid > 1 || unit > 2) { sendResponse(0x01); showStatus("Error unidad"); return; }
        sensors[sid].unit = unit;
        sendResponse(0x00);
        showStatus("Unidad actualizada");
      }
      break;

    case 0xE: // delay
      {
        uint8_t sid = (param & 0x03);
        while (Serial1.available() < 2);
        uint8_t hi = Serial1.read();
        uint8_t lo = Serial1.read();
        sensors[sid].delayMs = (hi << 8) | lo;
        sendResponse(0x00);
        showStatus("Delay actualizado");
      }
      break;

    case 0xF: // status
      {
        uint8_t sid = (param & 0x03);
        SensorConfig &s = sensors[sid];
        uint8_t data[7];
        data[0] = s.address;
        data[1] = s.unit;
        data[2] = (s.delayMs >> 8);
        data[3] = (s.delayMs & 0xFF);
        data[4] = s.periodic;
        data[5] = (s.periodMs >> 8);
        data[6] = (s.periodMs & 0xFF);
        sendResponse(0x02, data, 7);
        showStatus("Status enviado");
      }
      break;

    default:
      sendResponse(0x01);
      showStatus("Comando invalido");
      break;
  }
}

void setup() {
  Wire.begin();
  Serial1.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    for (;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("SENSOR INICIANDO");
  display.display();
  delay(1000);
  showStatus("Esperando", "comandos...");
}

void loop() {
  // Leer comandos del supervisor
  if (Serial1.available()) {
    uint8_t code = Serial1.read();
    handleCommand(code);
  }

  unsigned long now = millis();
  for (int i = 0; i < 2; i++) {
    SensorConfig &s = sensors[i];
    if (s.periodic && (now - s.lastShot >= s.periodMs)) {
      s.lastShot = now;
      s.lastMeasure = readSRF02(s.address, s.unit);
      uint8_t data[3] = { (uint8_t)i, (uint8_t)(s.lastMeasure >> 8), (uint8_t)(s.lastMeasure & 0xFF) };
      sendResponse(0x03, data, 3);

      if (s.lastMeasure == 0xFFFF)
        showStatus("Error SRF02", "Sensor inactivo");
      else
        showStatus("Sensores activos");
    }
  }
}
