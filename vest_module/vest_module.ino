#include "Adafruit_NeoPixel.h"
#include <bluefruit.h>
#include "notes.h"

#define buzzerPin A0
#define lightSensorPin A8
#define PIN0 A1
#define PIN1 A2
#define PIN2 A3 
#define LEDS 30
#define PIN_ONBOARD 8
#define LEDS_ONBOARD 10
#define indcTimer 10000 // indicator stays on for 10s
#define brakesTimer 50  // brakes pulse on/off at 10ms intervals
#define maxBrakeCycles 2 // quickly pulse brakes 3 times before slowly pulsing

BLEUart bleuart; // uart over ble
static boolean connected = false;
static uint8_t outputData[9] = {1, 0, 1, 0, 0, 0, 0, 0, 0};

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
Adafruit_NeoPixel onboard = Adafruit_NeoPixel(LEDS_ONBOARD, PIN_ONBOARD, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip0 = Adafruit_NeoPixel(LEDS, PIN0, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip1 = Adafruit_NeoPixel(LEDS, PIN1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2 = Adafruit_NeoPixel(LEDS, PIN2, NEO_GRB + NEO_KHZ800);

const uint32_t off = onboard.Color(0, 0, 0);
const uint32_t red = onboard.Color(255, 0, 0);
const uint32_t green = onboard.Color(0, 255, 0);

// orange
static uint32_t orange0 = strip0.Color(255, 170, 0);
static uint32_t orange1 = strip1.Color(255, 170, 0);
static uint32_t orange2 = strip2.Color(255, 170, 0);

// red
static uint32_t red0 = strip0.Color(255, 0, 0);
static uint32_t red1 = strip1.Color(255, 0, 0);
static uint32_t red2 = strip2.Color(255, 0, 0);

uint8_t resetCounter, counter = 0;
int8_t indcDir = 0;
unsigned long initMillisIndc;
unsigned long prevMillisIndc = 0;

boolean enableBrakes = false;
uint64_t brakesCount = 0;
uint8_t brakesCycles = 0;
unsigned long prevMillisBrakes = 0;

uint8_t pairCounter = 0;
unsigned long prevMillisPair = 0;

uint16_t flashPeriod = 1000;
unsigned long prevFlashTime = 0;
static boolean flash = false;

//void adjustBrightness()
//{
//  static float lightSensorVal = analogRead(lightSensorPin);
//  static uint8_t brightness = map(lightSensorVal, 0, 1023, 0, 255);
//
//  Serial.printf("Brightness: %d\r\n", brightness);
//
//  // red
//  red0 = strip0.Color(brightness, 0, 0);
//  red1 = strip1.Color(brightness, 0, 0);
//  red2 = strip2.Color(brightness, 0, 0);
//}

void clearStrips()
{
  strip0.fill((0, 0, 0), 0, LEDS);
  strip1.fill((0, 0, 0), 0, LEDS);
  strip2.fill((0, 0, 0), 0, LEDS);
  showStrips();
}

void showStrips()
{
  strip0.show();
  strip1.show();
  strip2.show();
}

void displayInputData(uint8_t inputData[])
{
  Serial.printf("Receiving values:");
  for (uint8_t i = 0; i < 9; i++)
    Serial.printf(" %02x", inputData[i]);
  Serial.printf("\r\n");
}

void indicator()
{
  if (indcDir != 0 && millis() - prevMillisIndc >= 50)
  {
    if (!enableBrakes)
    {
      outputData[4] = (indcDir == -1) ? 1 : 0;
      outputData[5] = (indcDir == 1) ? 1 : 0;
      if (counter % LEDS == 0)
      {
        counter = resetCounter;
        clearStrips();
      }
      strip0.setPixelColor(counter, orange0);
      strip1.setPixelColor(counter + indcDir, orange1);
      strip2.setPixelColor(counter, orange2);
      showStrips();
      counter += 3 * indcDir; 
    }   
    prevMillisIndc = millis();
  }
  if (millis() - initMillisIndc > indcTimer)
  {
    indcDir = 0;
    clearStrips();
    outputData[4] = 0;
    outputData[5] = 0;
  }
}

void brakes()
{
  if (enableBrakes)
  {
    outputData[6] = 1;
    if (brakesCycles <= maxBrakeCycles)
    {
      if (millis() - prevMillisBrakes >= brakesTimer && brakesCount <= 20)
      {
        clearStrips();
        if (brakesCount % 2 == 0)
        {
          strip0.fill(red0, 6, 18);
          strip1.fill(red1, 6, 18);
          strip2.fill(red2, 6, 18);
        }
        showStrips();
        brakesCount++;
        prevMillisBrakes = millis();
      }
      else if (millis() - prevMillisBrakes >= 500)
      {
        brakesCount = 0;
        brakesCycles++;
        prevMillisBrakes = millis();
      } 
    }
    else
    {
      for (int i = 6; i < 24; i++)
      {
        strip0.setPixelColor(i, red0);
        strip1.setPixelColor(i, red1);
        strip2.setPixelColor(i, red2);
      }
      showStrips();
      delay(100);
      enableBrakes = false;
    }
  }
  else
  {
    outputData[6] = 0;
  }
}

bool handleInput(uint8_t inputData[])
{
  // display data
  displayInputData(inputData);
  
  // data must start with 0101
  for (int i = 0; i < 4; i++)
  {
    if (inputData[i] != i % 2)
      {
        Serial.println("Invalid data");
        return false; 
      }
  }

  // set indicator direction
  if ((inputData[4] == 1 && indcDir == -1) || (inputData[5] == 1 && indcDir == 1))
  {
    indcDir = 0;
    clearStrips();
    outputData[4] = 0;
    outputData[5] = 0;
  }
  else 
  {
    counter = 0;
    initMillisIndc = millis();
    if (inputData[4] == 0x01)
    {
      indcDir = -1;
      resetCounter = 29;
    }
    if (inputData[5] == 0x01)
    {
      indcDir = 1;
      resetCounter = 0;
    }
  }

  // set brakes state
  if (inputData[6] == 0x01)
  {
    enableBrakes = true;
    brakesCount = 0;
    brakesCycles = 0;
  }
//  enableBrakes = (inputData[6] == 0x01) ? true : false;
//  brakesCount = (inputData[6] == 0x01) ? brakesCount : 0;
//  brakesCycles = (inputData[6] == 0x01) ? brakesCycles : 0;

  flashPeriod = (float)(2000 * ((float)inputData[8] / 255));
  if (flashPeriod == 0)
  {
    flashPeriod = 1000;
  }
  Serial.println(flashPeriod);

  return true;
}

void pairingLightSequence()
{
  if (!connected && millis() - prevMillisPair >= 100)
  {
    onboard.setPixelColor(pairCounter % LEDS_ONBOARD, off);
    pairCounter++;
    onboard.setPixelColor(pairCounter % LEDS_ONBOARD, red);
    onboard.show();
    prevMillisPair = millis();
    if (pairCounter == LEDS_ONBOARD)
      pairCounter = 0;
  }
  else if (millis() - prevMillisPair >= 10000)
  {
    onboard.fill(green, 0, 9);
    onboard.show();
    delay(100);
    onboard.fill(off, 0, 9);
    onboard.show();
    prevMillisPair = millis();
  }
}

void runningLights()
{
  if (connected && indcDir == 0 && !enableBrakes && millis() - prevFlashTime >= flashPeriod)
  {
    if (flash)
    {
       strip0.fill(red0, 13, 4);
       strip1.fill(red1, 13, 4);
       strip2.fill(red2, 13, 4);
    }
    else
    {
      clearStrips();
    }
    showStrips();
    flash = !flash;
    prevFlashTime = millis();
  }
}

void setup()
{
  Serial.begin(115200);

#if CFG_DEBUG
  // Blocking wait for connection when debug mode is enabled via IDE
  while ( !Serial ) yield();
#endif

  // initalize ble
  Serial.println("Initializing...");
  Bluefruit.autoConnLed(true);
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin();
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  bleuart.begin();
  startAdv();
  Serial.println("Advertising Started");

  pinMode(buzzerPin, OUTPUT);
  // startup tone
  tone(buzzerPin, noteD5, 200);
  delay(250);
  tone(buzzerPin, noteD5, 200);
  delay(250);
  tone(buzzerPin, noteRest, 100);
  noTone(buzzerPin);

  pinMode(lightSensorPin, INPUT);
  
  // initialize Neopixel strips
  onboard.begin();
  onboard.show();
  strip0.begin();
  strip1.begin();
  strip2.begin();
  showStrips();
}


void startAdv(void)
{
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
}

bool readStates()
{
  boolean writeData = false;
  static uint8_t prevOutputData[9] = {1, 0, 1, 0, 0, 0, 0, 0, 0};
  
  // Compare current and new outputData
  for (uint8_t i = 4; i < 9; i++)
  {
    if (outputData[i] != prevOutputData[i])
    {
      prevOutputData[i] = outputData[i];
      writeData = true;
    }
  }

  return writeData;
}
 
void loop()
{
  while ( bleuart.available() )
  {
    uint8_t inputData[9];
    bleuart.read(inputData, 9);
    handleInput(inputData);
  }
  brakes();
  indicator();
  runningLights();
  pairingLightSequence();
//  adjustBrightness();
  if (readStates())
  {
    bleuart.write(outputData, 9);
  }
}

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));

  connected = true;
  
  // flash green and play tones when connected
  tone(buzzerPin, noteC5, 100);
  onboard.fill(green, 0, 9);
  onboard.show();
  delay(100);
  tone(buzzerPin, noteD5, 100);
  onboard.fill(off, 0, 9);
  onboard.show();
  delay(100);
  tone(buzzerPin, noteE5, 100);
  onboard.fill(green, 0, 9);
  onboard.show();
  delay(100);
  onboard.fill(off, 0, 9);
  onboard.show();
  delay(100);
  onboard.fill(green, 0, 9);
  onboard.show();
  delay(500);
  onboard.fill(off, 0, 9);
  onboard.show();
  tone(buzzerPin, noteRest, 100);
  noTone(buzzerPin);

  Serial.print("Connected to ");
  Serial.println(central_name);
}

/**
   Callback invoked when a connection is dropped
   @param conn_handle connection where this event happens
   @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
*/
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  connected = false;
  clearStrips();
  
  tone(buzzerPin, noteE5, 100);
  delay(100);
  tone(buzzerPin, noteD5, 100);
  delay(100);
  tone(buzzerPin, noteC5, 100);
  delay(100);
  tone(buzzerPin, noteRest, 100);
  noTone(buzzerPin);
  

  Serial.println();
  Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);
}
