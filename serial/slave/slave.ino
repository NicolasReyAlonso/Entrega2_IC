/* ----------------------------------------------------------------------
 *  Dispositivo esclavo con sensores + OLED
 * ---------------------------------------------------------------------- 
 */

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define SRF01_I2C_ADDRESS byte((0xC0)>>1)
#define SRF02_I2C_ADDRESS byte((0xD6)>>1)
#define SRF02_CMD_REG 0x00
#define SRF02_RANGE_HIGH 0x02

#define OLED_ADDR 0x3D
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define NUM_SENSORS 2

#ifdef ARDUINO_AVR_UNO
#include <SoftwareSerial.h>
constexpr uint8_t RX_PIN = 8;
constexpr uint8_t TX_PIN = 9;
SoftwareSerial Serial1(RX_PIN, TX_PIN);  // RX, TX
#endif

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
};

bool isI2CDeviceAvailable(uint8_t address) {
  Wire.beginTransmission(address);
  return (Wire.endTransmission() == 0);
}

uint16_t readSRF02(uint8_t address, uint8_t unit) {
  uint8_t cmd = 0x51;  // cm
  if (unit == 2) cmd = 0x50;  // inches
  if (unit == 0) cmd = 0x56;  // microseconds

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
  if (data && len > 0) {
    Serial1.write(data, len);
  }
  delay(50);
  digitalWrite(LED_BUILTIN, LOW);
}

void updateOLEDStatus() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(0, 0);
  display.println("SENSORES:");
  
  for (uint8_t i = 0; i < NUM_SENSORS && i < 3; i++) {
    display.println();
    display.print(sensors[i].name);
    display.print(": ");
    
    if (sensors[i].active) {
      if (sensors[i].lastMeasure == 0xFFFF) {
        display.println("ERR");
      } else {
        display.print(sensors[i].lastMeasure);
        const char* unit = (sensors[i].unit == 1) ? "cm" : 
                          (sensors[i].unit == 2) ? "in" : "us";
        display.print(" ");
        display.println(unit);
      }
      if (sensors[i].periodic) {
        display.print(" P:");
        display.print(sensors[i].periodMs);
        display.println("ms");
      }
    } else {
      display.println("OFF");
    }
  }
  
  display.display();
}

void handleCommand(uint8_t code) {
  uint8_t header = code & 0xF0;
  
  // HELP (0x00)
  if (code == 0x00) {
    sendResponse(0x00);
    return;
  }
  
  // US LIST (0x80)
  if (code == 0x80) {
    uint8_t data[1 + NUM_SENSORS * 2];
    data[0] = NUM_SENSORS;
    
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
      data[1 + i * 2] = sensors[i].address;
      data[1 + i * 2 + 1] = sensors[i].active ? 0x01 : 0x00;
    }
    
    sendResponse(0x80, data, 1 + NUM_SENSORS * 2);
    return;
  }
  
  // UNIT (0xD0-0xDF)
  if (header == 0xD0) {
    uint8_t unit_bits = (code >> 2) & 0b11;
    
    while (Serial1.available() < 1);
    uint8_t sensor_id = Serial1.read();
    
    if (sensor_id >= NUM_SENSORS) {
      sendResponse(0xFF);
      return;
    }
    
    sensors[sensor_id].unit = unit_bits;
    sendResponse(0x00);
    updateOLEDStatus();
    return;
  }
  
  // RANGING (0xC0-0xCF)
  if (header == 0xC0) {
    uint8_t mode_bits = (code >> 2) & 0b11;
    
    while (Serial1.available() < 3);
    uint8_t sensor_id = Serial1.read();
    uint8_t period_low = Serial1.read();
    uint8_t period_high = Serial1.read();
    uint16_t period = (period_high << 8) | period_low;
    
    if (sensor_id >= NUM_SENSORS || !sensors[sensor_id].active) {
      sendResponse(0xFF);
      return;
    }
    
    SensorConfig &s = sensors[sensor_id];
    
    if (mode_bits == 0b00) {  // one-shot
      s.lastMeasure = readSRF02(s.address, s.unit);
      
      uint8_t data[4];
      data[0] = sensor_id;
      data[1] = s.unit;
      data[2] = (s.lastMeasure >> 8) & 0xFF;
      data[3] = s.lastMeasure & 0xFF;
      
      sendResponse(0x10, data, 4);
      updateOLEDStatus();
    }
    else if (mode_bits == 0b01) {  // off
      s.periodic = false;
      sendResponse(0x00);
      updateOLEDStatus();
    }
    else if (mode_bits == 0b10) {  // on
      s.periodic = true;
      s.periodMs = period;
      s.lastShot = millis();
      sendResponse(0x00);
      updateOLEDStatus();
    }
    return;
  }
  
  // DELAY (0xE0-0xEF)
  if (header == 0xE0) {
    while (Serial1.available() < 3);
    uint8_t sensor_id = Serial1.read();
    uint8_t delay_low = Serial1.read();
    uint8_t delay_high = Serial1.read();
    uint16_t delay_ms = (delay_high << 8) | delay_low;
    
    if (sensor_id >= NUM_SENSORS) {
      sendResponse(0xFF);
      return;
    }
    
    sensors[sensor_id].delayMs = delay_ms;
    sendResponse(0x00);
    return;
  }
  
  // STATUS (0xF0-0xFF)
  if (header == 0xF0) {
    while (Serial1.available() < 1);
    uint8_t sensor_id = Serial1.read();
    
    if (sensor_id >= NUM_SENSORS) {
      sendResponse(0xFF);
      return;
    }
    
    SensorConfig &s = sensors[sensor_id];
    
    uint8_t data[5];
    data[0] = s.address;
    data[1] = (s.delayMs >> 8) & 0xFF;
    data[2] = s.delayMs & 0xFF;
    data[3] = s.unit;
    data[4] = s.periodic ? 0x00 : 0xFF;
    
    sendResponse(0x01, data, 5);
    return;
  }
  
  sendResponse(0xFF);
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
    while(1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("INICIANDO...");
  display.display();
  delay(1000);
  
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    sensors[i].active = isI2CDeviceAvailable(sensors[i].address);
  }
  
  updateOLEDStatus();
}

void loop() {
  if (Serial1.available()) {
    uint8_t code = Serial1.read();
    handleCommand(code);
  }

  unsigned long now = millis();
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    SensorConfig &s = sensors[i];
    if (s.periodic && s.active && (now - s.lastShot >= s.periodMs)) {
      s.lastShot = now;
      s.lastMeasure = readSRF02(s.address, s.unit);
      
      uint8_t data[4];
      data[0] = i;
      data[1] = s.unit;
      data[2] = (s.lastMeasure >> 8) & 0xFF;
      data[3] = s.lastMeasure & 0xFF;
      
      sendResponse(0x10, data, 4);
      updateOLEDStatus();
    }
  }
}
