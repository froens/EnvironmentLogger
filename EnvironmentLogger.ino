#include <dht.h>
#include <SD.h>
#include <SPI.h>
#if defined(ETH)
#include <Ethernet.h>
#endif
#include <LiquidCrystal.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);
File dataFile;
dht DHT;
long lastTS = 0;
#define DHT22_PIN 8

#if defined(ETH)
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 0, 177);
EthernetClient client;


const char PhantHost[] = "data.sparkfun.com";
const char PublicKey[] = "9J0vVYgVrrT5DEN7Rx33";
const char PrivateKey[] = "xz5GaBRaeeFAbdBYvk22";
#endif

struct
{
    uint32_t sdcard;
    uint32_t lcd;
    uint32_t lux;
    uint32_t dht;
    uint32_t ethernet;
} stat = { 1,1,1,1,1 };


void configureLux(void)
{
  Serial.println("Setting up lux-sensor");
  /* You can also manually set the gain or enable auto-gain support */
  // tsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light to avoid sensor saturation */
  // tsl.setGain(TSL2561_GAIN_16X);     /* 16x gain ... use in low light to boost sensitivity */
  tsl.enableAutoRange(true);            /* Auto-gain ... switches automatically between 1x and 16x */
  
  /* Changing the integration time gives you better sensor resolution (402ms = 16-bit data) */
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium resolution and speed   */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit data but slowest conversions */

  if(!tsl.begin()) {
    /* There was a problem detecting the ADXL345 ... check your connections */
    Serial.print("Ooops, no TSL2561 detected ... Check your wiring or I2C ADDR!");
    stat.lux = 0;
  }
  if(stat.lux == 1) Serial.println("Lux-sensor setup successfully");
}

const int chipSelect = 4;



String configureSD(void) {
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    return "";
  }
  Serial.println("Card initialized.");

  int fileInt = 0;
  while(SD.exists(String(fileInt) + ".csv")) {
    fileInt++;
  }
  
  String fileName = String(fileInt) + ".csv";
  Serial.println("File name: " + fileName);
  dataFile = SD.open(fileName, FILE_WRITE);
  if (dataFile) {
    dataFile.println("TIMESTAMP;LUX;TEMPERATURE;HUMIDITY");
    dataFile.flush();
    stat.sdcard = 1;
    return fileName;
  }
  
  return "";
}

void configureDHT(void) {
  Serial.println("Setting up DHT-sensor");
  
  int chk = DHT.read22(DHT22_PIN);
  switch (chk)
  {
  case DHTLIB_OK:
      Serial.println("DHT OK");
      break;
  case DHTLIB_ERROR_CHECKSUM:
      Serial.print("Checksum error,\t");
      stat.dht = 0;
      break;
  case DHTLIB_ERROR_TIMEOUT:
      Serial.print("Time out error,\t");
      stat.dht = 0;
      break;
  case DHTLIB_ERROR_CONNECT:
      Serial.print("Connect error,\t");
      stat.dht = 0;
      break;
  case DHTLIB_ERROR_ACK_L:
      Serial.print("Ack Low error,\t");
      stat.dht = 0;
      break;
  case DHTLIB_ERROR_ACK_H:
      Serial.print("Ack High error,\t");
      stat.dht = 0;
      break;
  default:
      Serial.print("Unknown error,\t");
      stat.dht = 0;
      break;
  }

  if(stat.dht == 1) Serial.println("DHT-sensor setup successfully");
}

#if defined(ETH)
void configureEthernet(void) {

  Serial.println("Setting up ethernet");
  
  if (Ethernet.begin(mac) == 0) {
    
    Serial.println("Failed to configure Ethernet using DHCP");
    stat.ethernet = 0;
  }

  if(stat.ethernet == 1) Serial.println("Ethernet setup successfully");
}
#endif

void configureLCD(String fileName) {
  Serial.println("Setting up LCD");
  lcd.clear();
  lcd.begin(16, 2);
  lcd.setCursor(0,0);
  lcd.print("L: ");
  
  lcd.setCursor(0,1);
  lcd.print("T: ");

  lcd.setCursor(9,1);
  lcd.print("H:  ");

  lcd.setCursor(9,0);
  lcd.print("F:" + fileName);
  
  Serial.println("LCD Setup successfully");
}

#if defined(ETH)
bool sendToPhantIO(int temp, int lux, int humidity) {
  if (client.connect(PhantHost, 80)) {
      // Post the data! Request should look a little something like:
      // GET /input/publicKey?private_key=privateKey&light=1024&switch=0&name=Jim HTTP/1.1\n
      // Host: data.sparkfun.com\n
      // Connection: close\n
      // \n
      client.print("GET /input/");
      client.print(PublicKey);
      client.print("?private_key=");
      client.print(PrivateKey);
      client.print("&");
      client.print("humidity=");
      client.print(String(humidity));
      client.print("&");
      client.print("light=");
      client.print(String(lux));
      client.print("&");
      client.print("temp=");
      client.print(String(temp));
      client.println(" HTTP/1.1");
      client.print("Host: ");
      client.println(PhantHost);
      client.println("Connection: close");
      client.println();
    } else {
      Serial.println(F("Connection failed"));
      return false;
    }
    
    while (client.connected())
    {
      if ( client.available() )
      {
        char c = client.read();
        Serial.print(c);
      }      
    }
    Serial.println();
    
    client.stop();

    return true;
}
#endif

void setup() {
  Serial.begin(9600);
  String fileName = configureSD();
  configureLCD(fileName);
  configureLux();
  configureDHT();
  
  #if defined(ETH)
  configureEthernet();
  #endif
}

void loop() {

  int lux;
  double temp, humidity;
  
  /* Get a new sensor event */ 
  if(stat.lux) {
    sensors_event_t event;
    tsl.getEvent(&event);
    if (event.light) {
      lux = event.light;
      String textLux = "Lux: " + String(lux);
      Serial.println(textLux);
    }
  }

  if(stat.dht) {
    DHT.read22(DHT22_PIN);
    humidity = DHT.humidity;
    String textHum = "Humidity: " + String(humidity,1);
    Serial.println(textHum);
    
    temp = DHT.temperature;
    String textTemp = "Temperature: " + String(temp,1);
    Serial.println(textTemp);
  }
  
  #if defined(ETH)
  if(stat.ethernet && sendToPhantIO(temp, lux, humidity)) {
    Serial.println("Measurements sent to phant");
  }
  #endif
  
  long ts = millis();
  if(stat.sdcard && ts > (lastTS + 60000)) {
    Serial.println("Writing to logfile");
    lastTS = ts;
    String logEntry = String(ts/1000) + ";" + String(lux) + ";" + String(temp) + ";" + String(humidity);
    Serial.println(logEntry);
    dataFile.println(logEntry);
    dataFile.flush();
    
  }
  
  
  if(stat.lcd) {
    lcd.setCursor(3,0);
    lcd.print(String(lux));

    lcd.setCursor(3,1);
    lcd.print(String(temp,1));

    lcd.setCursor(12,1);
    lcd.print(String(humidity,1));
  }
  
  delay(1000);
}
