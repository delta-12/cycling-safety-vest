#include "BLEDevice.h"

#define DEBOUNCE 100

const uint8_t leftLedPin = 21;
const uint8_t rightLedPin = 22;
const uint8_t brakeLedPin = 23;

// rightIndcPin will be 13 in final design
// left and right pins have been switched for testing
uint8_t leftIndcPin = 19;
static uint8_t leftIndcState = 0;
unsigned long leftIndcMillis = 0;
uint8_t rightIndcPin = 15;
static uint8_t rightIndcState = 0;
unsigned long rightIndcMillis = 0;
//uint8_t brakes = 0;

static boolean braking = false;
static boolean leftIndcOn = false;
static boolean rightIndcOn = false;

unsigned long prevMillisPair = 0;

/*
   Current value of output characteristic persisted here
   left and right indicator can switch depending on orientation of strips
   0-3: 0101
   4:   left indicator
   5:   right indicator
   6:   brakes
   7:   brightness
   8:   flash rate
*/
static uint8_t outputData[9] = {0, 1, 0, 1, 0, 0, 0, 0, 0};

/* Specify the Service UUID of Server */
static BLEUUID serviceUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
/* Specify the Characteristic UUID of Server
    TX: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
    RX: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
*/
static BLEUUID txUUID("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");
static BLEUUID rxUUID("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteTXCharacteristic;
static BLERemoteCharacteristic* pRemoteRXCharacteristic;
static BLEAdvertisedDevice* myDevice;

static void rxNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                            uint8_t* pData, size_t length, bool isNotify)
{
  Serial.print("Notify callback for characteristic ");
  Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
  Serial.print(" of data length ");
  Serial.println(length);
  Serial.printf("data: ");
  for (int i = 0; i < 9; i++)
  {
    Serial.printf(" %02x ", pData[i]);
  }
  Serial.println();
  leftIndcOn = (pData[4] == 1) ? true : false;
  rightIndcOn = (pData[5] == 1) ? true : false;
  braking = (pData[6] == 1) ? true : false;
}

class MyClientCallback : public BLEClientCallbacks
{
    void onConnect(BLEClient* pclient)
    {

    }

    void onDisconnect(BLEClient* pclient)
    {
      connected = false;
      Serial.println("Disconnected from Vest Module.");
    }
};

void displayOutputData()
{
  Serial.printf("Writing value:");
  for (uint8_t i = 0; i < 9; i++)
    Serial.printf(" %02x", outputData[i]);
  Serial.printf("\r\n");
}

/* Start connection to the BLE Server */
bool connectToServer()
{
  Serial.print("Establishing connection... ");
  Serial.println(myDevice->getAddress().toString().c_str());

  BLEClient*  pClient  = BLEDevice::createClient();
  Serial.println(" - Created client");

  pClient->setClientCallbacks(new MyClientCallback());

  /* Connect to the remote BLE Server */
  pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
  Serial.println(" - Connected to server");

  /* Obtain a reference to the service we are after in the remote BLE server */
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr)
  {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our service");


  /* Obtain a reference to the tx characteristic in the service of the remote BLE server */
  pRemoteTXCharacteristic = pRemoteService->getCharacteristic(txUUID);
  if (pRemoteTXCharacteristic == nullptr)
  {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(txUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our TX characteristic");

  /* Check if tx can be written to */
  if (pRemoteTXCharacteristic->canWrite())
  {
    Serial.println("Characteristic is writable");
    displayOutputData();
    pRemoteTXCharacteristic->writeValue((uint8_t *)outputData, 9);
  }

  /* Obtain a reference to the rx characteristic in the service of the remote BLE server */
  pRemoteRXCharacteristic = pRemoteService->getCharacteristic(rxUUID);
  if (pRemoteTXCharacteristic == nullptr)
  {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(rxUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found RX our characteristic");

  /* Check if values can be read from rx */
  if (pRemoteRXCharacteristic->canRead())
  {
    Serial.println("Characteristic is readable");
  }

  if(pRemoteRXCharacteristic->canNotify())
  {
    pRemoteRXCharacteristic->registerForNotify(rxNotifyCallback);
  }

  connected = true;

  // flash in unison when connected
  digitalWrite(leftLedPin, HIGH);
  digitalWrite(rightLedPin, HIGH);
  digitalWrite(brakeLedPin, HIGH);
  delay(100);
  digitalWrite(leftLedPin, LOW);
  digitalWrite(rightLedPin, LOW);
  digitalWrite(brakeLedPin, LOW);
  delay(100);
  digitalWrite(leftLedPin, HIGH);
  digitalWrite(rightLedPin, HIGH);
  digitalWrite(brakeLedPin, HIGH);
  delay(100);
  digitalWrite(leftLedPin, LOW);
  digitalWrite(rightLedPin, LOW);
  digitalWrite(brakeLedPin, LOW);
  delay(100);
  digitalWrite(leftLedPin, HIGH);
  digitalWrite(rightLedPin, HIGH);
  digitalWrite(brakeLedPin, HIGH);
  delay(500);
  digitalWrite(leftLedPin, LOW);
  digitalWrite(rightLedPin, LOW);
  digitalWrite(brakeLedPin, LOW);
  
  return true;
}
/* Scan for BLE servers and find the first one that advertises the service we are looking for. */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks
{
    /* Called for each advertising BLE server. */
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
      Serial.print("BLE Advertised Device found: ");
      Serial.println(advertisedDevice.toString().c_str());

      /* We have found a device, let us now see if it contains the service we are looking for. */
      if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID))
      {
        BLEDevice::getScan()->stop();
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = true;
        doScan = true;
        Serial.println("Stopping scan, device found!");
      }
    }
};

void leftIndcISR()
{
  if (millis() - leftIndcMillis >= DEBOUNCE && connected)
  {
    leftIndcState++;
    leftIndcMillis = millis();
  }
}

void rightIndcISR()
{
  if (millis() - rightIndcMillis >= DEBOUNCE && connected)
  {
    rightIndcState++;
    rightIndcMillis = millis();
  }
}

/*
 * Brakes:
 * check if delta is positive above certain range
 * or negative below certain range
 * magnitutde of delta must also be large enough
 */
bool readStates()
{
  boolean writeData = false;
  uint8_t newOutputData[9] = {0, 1, 0, 1, 0, 0, 0, 0, 0};

  // Read input states
  newOutputData[4] = leftIndcState % 2;
  leftIndcState = 0;
  newOutputData[5] = rightIndcState % 2;
  rightIndcState = 0;

  // Compare current and new outputData
  for (uint8_t i = 4; i < 9; i++)
  {
    if (newOutputData[i] !=  outputData[i])
    {
      outputData[i] = newOutputData[i];
      writeData = true;
    }
  }

  return writeData;
}

void pairingLightSequence()
{
  static uint8_t state = 0;
  if (!connected && millis() - prevMillisPair >= 100)
  {
    digitalWrite(leftLedPin, state);
    digitalWrite(rightLedPin, !state);
    digitalWrite(brakeLedPin, state);
    state = !state;
    prevMillisPair = millis(); 
  }
}

void blinkLeds()
{
  static unsigned long prevMillisIndcLed = 0;
  static uint8_t state = 0;
  if (braking && millis() - prevMillisIndcLed > 100)
  {
    digitalWrite(brakeLedPin, state);
    state = !state;
    prevMillisIndcLed = millis();
  }
  else if (leftIndcOn && millis() - prevMillisIndcLed > 100)
  {
    digitalWrite(leftLedPin, state);
    state = !state;
    prevMillisIndcLed = millis();
  }
  else if (rightIndcOn && millis() - prevMillisIndcLed > 100)
  {
    digitalWrite(rightLedPin, state);
    state = !state;
    prevMillisIndcLed = millis();
  }
}

void setup()
{
  // braking and indc leds
  pinMode(leftLedPin, OUTPUT);
  pinMode(rightLedPin, OUTPUT);
  pinMode(brakeLedPin, OUTPUT);

  // indc button inputs
  pinMode(leftIndcPin, INPUT);
  pinMode(rightIndcPin, INPUT);

  // interrupts for indc buttons
  attachInterrupt(leftIndcPin, leftIndcISR, FALLING);
  attachInterrupt(rightIndcPin, rightIndcISR, FALLING);

  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("Control Module");

  /* Retrieve a Scanner and set the callback we want to use to be informed when we
     have detected a new device.  Specify that we want active scanning and start the
     scan to run for 5 seconds. */
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
}

void loop()
{

  /* If the flag "doConnect" is true, then we have scanned for and found the desired
     BLE Server with which we wish to connect.  Now we connect to it.  Once we are
     connected we set the connected flag to be true. */
  if (doConnect == true)
  {
    if (connectToServer())
    {
      Serial.println("Successfully connected to Vest Module.");
    }
    else
    {
      Serial.println("Failed to connect to Vest Module.");
    }
    doConnect = false;
  }

  /* If connected to Vest Module, update the characteristic each time we are reached
     with the current input states */
  if (connected)
  {
    if (readStates())
    {
      displayOutputData();
      pRemoteTXCharacteristic->writeValue((uint8_t *)outputData, 9);
    }
  }
  else if (doScan)
  {
    BLEDevice::getScan()->start(0);  // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
  }
  pairingLightSequence();
  blinkLeds();
}
