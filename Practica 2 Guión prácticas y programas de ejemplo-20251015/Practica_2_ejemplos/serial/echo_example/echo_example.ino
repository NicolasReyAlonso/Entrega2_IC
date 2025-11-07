/* ----------------------------------------------------------------------
 *  Dispositivo esclavo con sensores + OLED
 * ---------------------------------------------------------------------- 
 */

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define SRF01_I2C_ADDRESS byte((0xC0)>>1)
#define SRF02_I2C_ADDRESS byte((0xF2)>>1)
#define SRF02_CMD_REG 0x00
#define SRF02_RANGE_HIGH 0x02

#define OLED_ADDR 0x3D
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define NUM_SENSORS 2  // Tenemos 2 sensores pero podrían haber más

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

struct SensorConfig {
  uint8_t address;
  uint8_t unit;
  uint16_t delayMs;
  bool periodic;
  uint16_t periodMs;
  unsigned long lastShot;
  uint16_t lastMeasure;
  bool active;
  char name[8];
};

SensorConfig sensors[NUM_SENSORS] = {
  {SRF01_I2C_ADDRESS, 1, 80, false, 0, 0, 0, false, "SRF01"},
  {SRF02_I2C_ADDRESS, 1, 80, false, 0, 0, 0, false, "SRF02"}
  // Aquí podríamos poner más sensores pero sólo tenemos dos.
};

bool isI2CDeviceAvailable(uint8_t address) {
  Wire.beginTransmission(address);
  return (Wire.endTransmission() == 0);
}

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
  digitalWrite(LED_BUILTIN, HIGH);
  
  Serial1.write(code);
  if (data && len > 0) Serial1.write(data, len);
  
  delay(50);
  digitalWrite(LED_BUILTIN, LOW);
}

void updateOLEDStatus() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(0, 0);
  display.println("ESTADO SENSORES:");
  
  uint8_t maxVisible = min(NUM_SENSORS, 3);
  
  for (uint8_t i = 0; i < maxVisible; i++) {
    display.println("");
    display.print(sensors[i].name);
    display.print(": ");
    
    if (sensors[i].active) {
      display.println("OK");
      display.print("  ");
      if (sensors[i].lastMeasure == 0xFFFF) {
        display.println("ERROR");
      } else {
        display.print(sensors[i].lastMeasure);
        const char* unit = (sensors[i].unit == 1) ? "cm" : 
                          (sensors[i].unit == 2) ? "in" : "us";
        display.print(" ");
        display.println(unit);
      }
      if (sensors[i].periodic) {
        display.print("  PER:");
        display.print(sensors[i].periodMs);
        display.println("ms");
      }
    } else {
      display.println("OFF");
    }
  }
  
  if (NUM_SENSORS > 3) {
    display.setCursor(100, 56);
    display.print("+");
    display.print(NUM_SENSORS - 3);
  }
  
  display.display();
}

void handleCommand(uint8_t code) {
  uint8_t op = (code & 0xF0) >> 4;
  uint8_t param = code & 0x0F;

  switch (op) {
    case 0x0: // help
      {
        const char helpMsg[] = 
            "0x0_: HELP\n"
            "0x8_: US LIST\n"
            "0xC_: US CMD (one-shot/on/off)\n"
            "0xD_: UNIT\n"
            "0xE_: DELAY\n"
            "0xF_: STATUS";
        
        uint8_t len = strlen(helpMsg);
        sendResponse(0x02, (uint8_t*)helpMsg, len);
      }
      break;

    case 0x8: // us mostrar sensores
      {
        uint8_t num = NUM_SENSORS;
        sendResponse(0x02, &num, 1);
      }
      break;

    case 0xC: // one-shot / on / off
      {
        uint8_t mode = (param & 0x0C) >> 2;
        uint8_t sid = (param & 0x03);
        
        if (sid >= NUM_SENSORS) { 
          uint8_t errorData = 0xFF;
          sendResponse(0x01, &errorData, 1);
          return; 
        }

        SensorConfig &s = sensors[sid];
        if (mode == 0b00) { // one-shot
          s.lastMeasure = readSRF02(s.address, s.unit);
          uint8_t data[3];
          data[0] = (s.lastMeasure == 0xFFFF) ? 0xFF : sid;
          data[1] = (uint8_t)(s.lastMeasure >> 8);
          data[2] = (uint8_t)(s.lastMeasure & 0xFF);
          sendResponse(0x03, data, 3);
          updateOLEDStatus();
        } 
        else if (mode == 0b01) { // off
          s.periodic = false;
          sendResponse(0x00);
          updateOLEDStatus();
        } 
        else if (mode == 0b10) { // on <period_ms>
          while (Serial1.available() < 2);
          uint8_t hi = Serial1.read();
          uint8_t lo = Serial1.read();
          s.periodMs = (hi << 8) | lo;
          s.periodic = true;
          s.lastShot = millis();
          sendResponse(0x00);
          updateOLEDStatus();
        }
      }
      break;

    case 0xD: // unit
      {
        uint8_t unit = (param & 0x0C) >> 2;
        uint8_t sid = (param & 0x03);
        
        if (sid >= NUM_SENSORS || unit > 2) { 
          uint8_t errorData = 0xFF;
          sendResponse(0x01, &errorData, 1);
          return; 
        }
        sensors[sid].unit = unit;
        sendResponse(0x00);
        updateOLEDStatus();
      }
      break;

    case 0xE: // delay
      {
        uint8_t sid = (param & 0x03);
        
        if (sid >= NUM_SENSORS) {
          uint8_t errorData = 0xFF;
          sendResponse(0x01, &errorData, 1);
          return;
        }
        
        while (Serial1.available() < 2);
        uint8_t hi = Serial1.read();
        uint8_t lo = Serial1.read();
        sensors[sid].delayMs = (hi << 8) | lo;
        sendResponse(0x00);
      }
      break;

    case 0xF: // status
      {
        uint8_t sid = (param & 0x03);
        
        if (sid >= NUM_SENSORS) {
          uint8_t errorData = 0xFF;
          sendResponse(0x01, &errorData, 1);
          return;
        }
        
        SensorConfig &s = sensors[sid];
        uint8_t data[8];
        data[0] = sid;
        data[1] = s.address;
        data[2] = s.unit;
        data[3] = (s.delayMs >> 8);
        data[4] = (s.delayMs & 0xFF);
        data[5] = s.periodic;
        data[6] = (s.periodMs >> 8);
        data[7] = (s.periodMs & 0xFF);
        sendResponse(0x02, data, 8);
      }
      break;

    default:
      {
        uint8_t errorData = 0xFF;
        sendResponse(0x01, &errorData, 1);
      }
      break;
  }
}

void setup() {
  Wire.begin();
  Serial1.begin(9600);
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  delay(100);
  while (Serial1.available() > 0) {
    Serial1.read();
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    for (;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(0, 0);
  display.println("COMPROBANDO");
  display.println("SENSORES...");
  display.display();
  delay(1000);
  
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    sensors[i].active = isI2CDeviceAvailable(sensors[i].address);
  }
  
  updateOLEDStatus();
  
  while (Serial1.available() > 0) {
    Serial1.read();
  }
}

void loop() {
  if (Serial1.available()) {
    uint8_t code = Serial1.read();
    handleCommand(code);
  }

  unsigned long now = millis();
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    SensorConfig &s = sensors[i];
    if (s.periodic && (now - s.lastShot >= s.periodMs)) {
      s.lastShot = now;
      s.lastMeasure = readSRF02(s.address, s.unit);
      uint8_t data[3];
      data[0] = (s.lastMeasure == 0xFFFF) ? 0xFF : i;
      data[1] = (uint8_t)(s.lastMeasure >> 8);
      data[2] = (uint8_t)(s.lastMeasure & 0xFF);
      sendResponse(0x03, data, 3);
      updateOLEDStatus();
    }
  }
}
