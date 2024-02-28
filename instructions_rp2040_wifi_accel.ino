/*
  WiFi Web Server for Challenger 2040 Wi-Fi/BLE Board

  Shitty sample code stolen by (and from) Gary, thanks for nothing, asshole

 */
#include <WiFiEspAT.h>
#include <ChallengerWiFi.h>
#include <Adafruit_NeoPixel.h>
#include <EspATMQTT.h>
#include <MC34X9.h>

#ifndef STASSID
#define STASSID "iPhone (10)"
#define STAPSK "deusXsilica"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

WiFiServer server(80);
EspATMQTT mqtt;

#define PIN        D14
#define NUMPIXELS 1
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// bSpi: I2C/SPI bus selection.  SPI: 0, I2C: 1
const uint8_t bSpi = 1;

// Chip Select & Address
uint8_t chipSelect = 0;
const uint8_t SPIChipSelectPin = 10; // SPI chipSelectPin
const uint8_t I2CAddress = 0x4c; // I2C address

const int INTERRUPT_PIN = 8;
int FIFO_THRE_SIZE = 30;
const bool enableFifoThrINT = false;
bool enableFIFO = false;

const bool enableTILT = false;
const bool enableFLIP = false;
const bool enableANYM = false;
const bool enableSHAKE = false;
const bool enableTILT_35 = false;

const bool interruptEnabled = enableFifoThrINT || enableTILT || enableFLIP || enableANYM || enableSHAKE || enableTILT_35;

MC34X9 MC34X9_acc = MC34X9();

void checkRange()
{
  switch (MC34X9_acc.GetRangeCtrl())
  {
    case MC34X9_RANGE_16G:
      Serial.println("Range: +/- 16 g");
      break;
    case MC34X9_RANGE_12G:
      Serial.println("Range: +/- 12 g");
      break;
    case MC34X9_RANGE_8G:
      Serial.println("Range: +/- 8 g");
      break;
    case MC34X9_RANGE_4G:
      Serial.println("Range: +/- 4 g");
      break;
    case MC34X9_RANGE_2G:
      Serial.println("Range: +/- 2 g");
      break;
    default:
      Serial.println("Range: +/- ?? g");
      break;
  }
}

void sensorFIFO()
{
  //Enable FIFO and interrupt
  MC34X9_acc.stop();
  MC34X9_acc.SetSampleRate(MC34X9_SR_50Hz);
  MC34X9_acc.SetFIFOCtrl(MC34X9_FIFO_CTL_ENABLE, MC34X9_FIFO_MODE_WATERMARK, FIFO_THRE_SIZE);
  MC34X9_acc.SetFIFOINTCtrl(false, false, enableFifoThrINT); //Enable FIFO threshold interrupt
  MC34X9_acc.wake();

  Serial.println("Sensor FIFO enable.");
}

void readAndOutput() 
{
  // Read the raw sensor data count
  MC34X9_acc_t rawAccel = MC34X9_acc.readRawAccel();

  // Output Count
  Serial.print("X:\t"); Serial.print(rawAccel.XAxis); Serial.print("\t");
  Serial.print("Y:\t"); Serial.print(rawAccel.YAxis); Serial.print("\t");
  Serial.print("Z:\t"); Serial.print(rawAccel.ZAxis); Serial.print("\t");
  Serial.println("counts");

  // Display the results (acceleration is measured in m/s^2)
  Serial.print("X: \t"); Serial.print(rawAccel.XAxis_g); Serial.print("\t");
  Serial.print("Y: \t"); Serial.print(rawAccel.YAxis_g); Serial.print("\t");
  Serial.print("Z: \t"); Serial.print(rawAccel.ZAxis_g); Serial.print("\t");
  Serial.println("m/s^2");

  Serial.println("---------------------------------------------------------");
  return;
}

void setNEO(int red, int green, int blue) 
{
  pixels.clear();
  pixels.setPixelColor(0, pixels.Color(red, green, blue));
  pixels.show();  
}

void setup() {
  Serial.println("mCube Accelerometer MC34X9 initialisation (IIC mode):");
  
  // Define SPI chip select or I2C address
  if (!bSpi) {
    chipSelect = SPIChipSelectPin;
  } else {
    chipSelect = I2CAddress;
  }

  // Check chip setup
  checkRange();
  Serial.println();

  //Test read
  readAndOutput();

  // Enable feature & interrput
  enableFIFO = enableFIFO || enableFifoThrINT;
  if (enableFIFO) {
    sensorFIFO();
  }

  // Init MC34X9 Object
  if (!MC34X9_acc.begin(bSpi, chipSelect)) {
    Serial.println("Invalid chip ID");
    // Invalid chip ID
    // Block here
    while (true);
    return;
  }
  
  setNEO(128,128,128);
  Serial.begin(115200);
  while (!Serial);

  //Serial1.begin(AT_BAUD_RATE);
  if (Challenger2040WiFi.reset())
    Serial.println(F("WiFi Chip reset OK !"));
  else {
    Serial.println(F("Could not reset WiFi chip !"));
    setNEO(255,0,0);
    while(true);
  }
  
  WiFi.init(Serial2);

  if (WiFi.status() == WL_NO_MODULE) {
      Serial.println("Communication with WiFi module failed!");
      setNEO(255,255,0);
      // don't continue
      while (true);
  }
  /*
  // waiting for connection to Wifi network set with the SetupWiFiConnection sketch
  setNEO(0,0,255);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println(".");
    delay(1000);
  }
  */

  mqtt.begin();
  mqtt.enableNTPTime(true, NULL, 3, "0.pool.ntp.org", NULL, NULL);
  char *time_from_ESP;
  mqtt.getNTPTime(&time_from_ESP);
  Serial.println(time_from_ESP);
  server.begin();

  IPAddress ip = WiFi.localIP();
  Serial.println();
  Serial.println("Connected to WiFi network.");
  Serial.print("To access the server, enter \"http://");
  Serial.print(ip);
  Serial.println("/\" in web browser.");
  setNEO(0,128,0);
}

void loop() {

  WiFiClient client = server.available();
  if (client) {
    IPAddress ip = client.remoteIP();
    Serial.print("new client ");
    Serial.println(ip);
    delay(300);

    while (client.connected()) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        line.trim();
        Serial.println(line);

        // if you've gotten to the end of the HTTP header (the line is blank),
        // the http request has ended, so you can send a reply
        if (line.length() == 0) {
          // send a standard http response header
          Serial.println("Send response");
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          client.println("<p>");
          char *time_from_ESP;
          mqtt.getNTPTime(&time_from_ESP);
          client.println(time_from_ESP);
          client.println("</html>");
          client.flush();
          break;
        }
      }
    }

    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}
