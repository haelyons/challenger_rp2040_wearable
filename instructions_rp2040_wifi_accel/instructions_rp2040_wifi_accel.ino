/*
  WiFi Web Server for Challenger 2040 Wi-Fi/BLE Board communication with Max MSP
  
  - Additions are iLabs adapted MC34X9 Accelerometer implementation, and writing this
  data to HTML via the Challenge WiFi chip.
  - Included in the Github repository is a Max patch that reads the HTML to split and
  process the accel values

  Hardware and Driver Resources:
  https://ilabs.se/product/bi2c-accelerometer/
  https://github.com/PontusO/EspATMQTT
  https://espressif-docs.readthedocs-hosted.com/projects/arduino-esp32/en/latest/api/wifi.html#:~:text=Working%20as%20AP,inside%20the%20ESP32%2C%20for%20example.
  https://docs.espressif.com/projects/esp-at/en/latest/esp32c3/AT_Command_Set/Basic_AT_Commands.html
  https://github.com/PontusO/mc34x9_driver/blob/master/examples/MC34X9_demo/MC34X9_demo.ino
  https://github.com/garyexplains/examples/blob/master/Challenger_RP2040/Arduino/WiFiWebServer4Time/WiFiWebServer4Time.ino

  Application Resources:
  https://aatishb.com/materials/srr/workshop3.pdf -- accelerometer processing to get orientation with Processing app. example
*/ 

#include <MC34X9.h>
#include <WiFiEspAT.h>
#include <ChallengerWiFi.h>
#include <Adafruit_NeoPixel.h>

WiFiServer server(80);

#define PIN        D14
#define NUMPIXELS 1
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// I2C/SPI bus selection: SPI: 0 / I2C: 1
const uint8_t bSpi = 1;

uint8_t chipSelect = 0; // Chip Select & Address
const uint8_t SPIChipSelectPin = 10; // SPI chipSelectPin
const uint8_t I2CAddress = 0x4c; // I2C address

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

String readAndOutput() 
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
  
  String data = "X: " + String(rawAccel.XAxis_g) + " m/s^2, " +
                "Y: " + String(rawAccel.YAxis_g) + " m/s^2, " +
                "Z: " + String(rawAccel.ZAxis_g) + " m/s^2";
  return data;
}

void checkSamplingRate()
{
  Serial.println("Low Power Mode SR");
  switch (MC34X9_acc.GetSampleRate())
  {
    case MC34X9_SR_25Hz:
      Serial.println("Output Sampling Rate: 25 Hz");
      break;
    case MC34X9_SR_50Hz:
      Serial.println("Output Sampling Rate: 50 Hz");
      break;
    case MC34X9_SR_62_5Hz:
      Serial.println("Output Sampling Rate: 62.5 Hz");
      break;
    case MC34X9_SR_100Hz:
      Serial.println("Output Sampling Rate: 100 Hz");
      break;
    case MC34X9_SR_125Hz:
      Serial.println("Output Sampling Rate: 125 Hz");
      break;
    case MC34X9_SR_250Hz:
      Serial.println("Output Sampling Rate: 250 Hz");
      break;
    case MC34X9_SR_500Hz:
      Serial.println("Output Sampling Rate: 500 Hz");
      break;
    case MC34X9_SR_DEFAULT_1000Hz:
      Serial.println("Output Sampling Rate: 1000 Hz");
      break;
    default:
      Serial.println("Output Sampling Rate: ?? Hz");
      break;
  }
}

void setNEO(int red, int green, int blue) 
{
  pixels.clear();
  pixels.setPixelColor(0, pixels.Color(red, green, blue));
  pixels.show();  
}

void setup() 
{
  setNEO(128,128,128);
  Serial.begin(115200);
  while (!Serial);

  Serial.println("mCube Accelerometer MC34X9 initialisation (IIC mode):");
  
  // Define SPI chip select or I2C address
  if (!bSpi) {
    chipSelect = SPIChipSelectPin;
  } else {
    chipSelect = I2CAddress;
  }

  // Check chip setup
  checkRange();
  checkSamplingRate();
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
  Serial.println("Began MC34X9 successfully...");
  Serial.println("");

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
  server.begin();

  IPAddress ip = WiFi.localIP();
  Serial.println();
  Serial.println("Hosting WiFi network, use Wireshark to find the address of the web server...");
  Serial.print("To access the server, enter \"http://");
  Serial.print("HOST.IP");
  Serial.println("/\" in web browser.");
  setNEO(0,128,0);
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    IPAddress ip = client.remoteIP();
    Serial.print("new client ");
    Serial.println(ip);
    delay(10);

    while (client.connected()) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        line.trim();
        Serial.println(line);

        // Assume readAndOutput() is modified to return the accelerometer data as a String
        String accelData = readAndOutput();

        if (line.length() == 0) {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");
          client.println("Refresh: 0.5"); // refresh every 10 milliseconds, though not practical
          client.println();
          client.println("<!DOCTYPE HTML><html>");
          client.println("<p>Accelerometer Data:</p>");
          client.print("<p>");
          client.print(accelData); // Print the accelerometer data in the HTML content
          client.println("</p></html>");
          client.flush();
          break;
        }
      }
    }
    client.stop();
    Serial.println("client disconnected");
  }
}
