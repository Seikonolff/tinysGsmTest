#include <Arduino.h>
#include "wiring_private.h"

Uart GSM_Serial(&sercom0, 5, 6, SERCOM_RX_PAD_1, UART_TX_PAD_0);
Uart MP3_Serial(&sercom1, 13, 8, SERCOM_RX_PAD_1, UART_TX_PAD_2);

void SERCOM0_Handler()
{
  GSM_Serial.IrqHandler();
}

void SERCOM1_Handler()
{
  MP3_Serial.IrqHandler();
}

/**************************************************************
 *
 * For this example, you need to install PubSubClient library:
 *   https://github.com/knolleary/pubsubclient
 *   or from http://librarymanager/all#PubSubClient
 *
 * TinyGSM Getting Started guide:
 *   https://tiny.cc/tinygsm-readme
 *
 * For more MQTT examples, see PubSubClient library
 *
 **************************************************************
 * This example connects to HiveMQ's showcase broker.
 *
 * You can quickly test sending and receiving messages from the HiveMQ webclient
 * available at http://www.hivemq.com/demos/websocket-client/.
 *
 * Subscribe to the topic GsmClientTest/ledStatus
 * Publish "toggle" to the topic GsmClientTest/led and the LED on your board
 * should toggle and you should see a new message published to
 * GsmClientTest/ledStatus with the newest LED status.
 *
 **************************************************************/


#define TINY_GSM_MODEM_SIM7600

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

// Set serial for AT commands (to the module)
// Use Hardware Serial on Mega, Leonardo, Micro
#ifndef __AVR_ATmega328P__
#define SerialAT GSM_Serial

// or Software Serial on Uno, Nano
#else
#include <SoftwareSerial.h>
SoftwareSerial SerialAT(2, 3);  // RX, TX
#endif

// See all AT commands, if wanted
//#define DUMP_AT_COMMANDS

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG SerialMon

// Range to attempt to autobaud
// NOTE:  DO NOT AUTOBAUD in production code.  Once you've established
// communication, set a fixed baud rate using modem.setBaud(#).
#define GSM_AUTOBAUD_MIN 9600
#define GSM_AUTOBAUD_MAX 115200
#define GSM_NORMAL_BAUD  115200

// Add a reception delay, if needed.
// This may be needed for a fast processor at a slow baud rate.
// #define TINY_GSM_YIELD() { delay(2); }

// Define how you're planning to connect to the internet.
// This is only needed for this example, not in other code.
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

// set GSM PIN, if any
#define GSM_PIN ""

// Your GPRS credentials, if any
const char apn[]      = "orange";
const char gprsUser[] = "orange";
const char gprsPass[] = "orange";

// Your WiFi connection credentials, if applicable
//const char wifiSSID[] = "YourSSID";
//const char wifiPass[] = "YourWiFiPass";

// MQTT details
const char* PublicServer = "broker.hivemq.com";

const char* mqttPrivateServer = "d789384c1e7d4abbbd123adb6be9c37f.s1.eu.hivemq.cloud";
const char* mqttServerPort = "8883";
const char* mqttUser = "mathi";
const char* mqttPassword = "Mathi1604!";

const char* topicLed       = "GsmClientTest/led";
const char* topicInit      = "GsmClientTestmathi/init";
const char* helloFromServer = "GsmClientTestmathi/hello";
const char* topicLedStatus = "GsmClientTest/ledStatus";

#include <TinyGsmClient.h>
//#include <SSLClient.h>
#include <PubSubClient.h>

// Just in case someone defined the wrong thing..
#if TINY_GSM_USE_GPRS && not defined TINY_GSM_MODEM_HAS_GPRS
#undef TINY_GSM_USE_GPRS
#undef TINY_GSM_USE_WIFI
#define TINY_GSM_USE_GPRS false
#define TINY_GSM_USE_WIFI true
#endif
#if TINY_GSM_USE_WIFI && not defined TINY_GSM_MODEM_HAS_WIFI
#undef TINY_GSM_USE_GPRS
#undef TINY_GSM_USE_WIFI
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#endif


//#define DUMP_AT_COMMANDS

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif
TinyGsmClient gsmTransportLayer(modem);
//SSLClient securePresentationLayer(&gsmTransportLayer);
PubSubClient  mqtt(gsmTransportLayer);

#define LED_PIN 13
int ledStatus = LOW;

uint32_t lastReconnectAttempt = 0;

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  SerialMon.print("Message arrived [");
  SerialMon.print(topic);
  SerialMon.print("]: ");
  SerialMon.write(payload, len);
  SerialMon.println();

  // Only proceed if incoming message's topic matches
  if (String(topic) == topicLed) {
    ledStatus = !ledStatus;
    digitalWrite(LED_PIN, ledStatus);
    mqtt.publish(topicLedStatus, ledStatus ? "1" : "0");
  }
}

boolean mqttConnect() {
  SerialMon.print("Connecting to ");
  SerialMon.print(PublicServer);

  // Connect to MQTT Broker
  boolean status = mqtt.connect("GsmClientTest");

  // Or, if you want to authenticate MQTT:
  // boolean status = mqtt.connect(clusterName, mqttUser, mqttPassword);

  if (status == false) {
    SerialMon.println(" fail");
    return false;
  }
  SerialMon.println(" success");
  delay(1000);
  mqtt.publish(topicInit, "GsmClientTest started");
  mqtt.subscribe(helloFromServer);
  SerialMon.println("init mqtt published");
  //mqtt.subscribe(topicLed);
  return mqtt.connected();
}

float gsm_latitude  = 0;
float gsm_longitude = 0;
float gsm_accuracy  = 0;
int   gsm_year      = 0;
int   gsm_month     = 0;
int   gsm_day       = 0;
int   gsm_hour      = 0;
int   gsm_minute    = 0;
int   gsm_second    = 0;


void setup() {
  pinPeripheral(5, PIO_SERCOM_ALT); // GSM Serial
  pinPeripheral(6, PIO_SERCOM_ALT); // GSM Serial

  SerialMon.begin(9600);
  delay(10);

  pinMode(LED_PIN, OUTPUT);

  // !!!!!!!!!!!
  // Set your reset, enable, power pins here
  // !!!!!!!!!!!

  // Set GSM module baud rate
  TinyGsmAutoBaud(SerialAT, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
  //SerialAT.begin(GSM_NORMAL_BAUD);
  //SerialAT.print("AT+IPREX=9600");
  delay(6000);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Initializing modem...");
  modem.restart();
  // modem.init();

  String modemInfo = modem.getModemInfo();
  SerialMon.print("Modem Info: ");
  SerialMon.println(modemInfo);
/*
#if TINY_GSM_USE_GPRS
  // Unlock your SIM card with a PIN if needed
  if (GSM_PIN && modem.getSimStatus() != 3) { modem.simUnlock(GSM_PIN); }
#endif
*/
if (GSM_PIN && modem.getSimStatus() != 3) { modem.simUnlock(GSM_PIN); }
/*
#if TINY_GSM_USE_WIFI
  // Wifi connection parameters must be set before waiting for the network
  SerialMon.print(F("Setting SSID/password..."));
  if (!modem.networkConnect(wifiSSID, wifiPass)) {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" success");
#endif
*/
/*
#if TINY_GSM_USE_GPRS && defined TINY_GSM_MODEM_XBEE
  // The XBee must run the gprsConnect function BEFORE waiting for network!
  modem.gprsConnect(apn, gprsUser, gprsPass);
#endif
*/
  SerialMon.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" success");

  if (modem.isNetworkConnected()) { SerialMon.println("Network connected"); }

#if TINY_GSM_USE_GPRS
  // GPRS connection parameters are usually set after network registration
  SerialMon.print(F("Connecting to "));
  SerialMon.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" success");

  if (modem.isGprsConnected()) { SerialMon.println("GPRS connected"); }
#endif

  //modem.
  // MQTT Broker setup
  mqtt.setServer(PublicServer, 1883);
  mqtt.setCallback(mqttCallback);
  mqttConnect();

  #if true && defined TINY_GSM_MODEM_HAS_GPS
    DBG("Enabling GPS/GNSS/GLONASS and waiting 15s for warm-up");
  #if !defined(TINY_GSM_MODEM_SARAR5)  // not needed for this module
    modem.enableGPS();
  #endif
  #endif
}

void loop() {
  // Make sure we're still registered on the network
  if (!modem.isNetworkConnected()) {
    SerialMon.println("Network disconnected");
    if (!modem.waitForNetwork(180000L, true)) {
      SerialMon.println(" fail");
      delay(10000);
      return;
    }
    if (modem.isNetworkConnected()) {
      SerialMon.println("Network re-connected");
    }

#if TINY_GSM_USE_GPRS
    // and make sure GPRS/EPS is still connected
    if (!modem.isGprsConnected()) {
      SerialMon.println("GPRS disconnected!");
      SerialMon.print(F("Connecting to "));
      SerialMon.print(apn);
      if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println(" fail");
        delay(10000);
        return;
      }
      if (modem.isGprsConnected()) { SerialMon.println("GPRS reconnected"); }
    }
#endif
  }

  if (!mqtt.connected()) {
    SerialMon.println("=== MQTT NOT CONNECTED ===");
    // Reconnect every 10 seconds
    uint32_t t = millis();
    if (t - lastReconnectAttempt > 10000L) {
      lastReconnectAttempt = t;
      if (mqttConnect()) { lastReconnectAttempt = 0; }
    }
    delay(100);
    return;
  }

  mqtt.loop();

  /*
  for (int8_t i = 15; i; i--) {
    DBG("Requesting current GSM location");
    if (modem.getGsmLocation(&gsm_latitude, &gsm_longitude, &gsm_accuracy,
                             &gsm_year, &gsm_month, &gsm_day, &gsm_hour,
                             &gsm_minute, &gsm_second)) {
      DBG("Latitude:", String(gsm_latitude, 8),
          "\tLongitude:", String(gsm_longitude, 8));
      DBG("Accuracy:", gsm_accuracy);
      DBG("Year:", gsm_year, "\tMonth:", gsm_month, "\tDay:", gsm_day);
      DBG("Hour:", gsm_hour, "\tMinute:", gsm_minute, "\tSecond:", gsm_second);
      break;
    } else {
      DBG("Couldn't get GSM location, retrying in 15s.");
      delay(15000L);
    }
  }
  DBG("Retrieving GSM location again as a string");
  String location = modem.getGsmLocation();
  DBG("GSM Based Location String:", location);
  */
 
 // Test the GPS functions
  
    delay(15000L);
    float gps_latitude  = 0;
    float gps_longitude = 0;
    float gps_speed     = 0;
    float gps_altitude  = 0;
    int   gps_vsat      = 0;
    int   gps_usat      = 0;
    float gps_accuracy  = 0;
    int   gps_year      = 0;
    int   gps_month     = 0;
    int   gps_day       = 0;
    int   gps_hour      = 0;
    int   gps_minute    = 0;
    int   gps_second    = 0;

    if(modem.getBetterGPS(&gps_latitude, &gps_longitude, &gps_speed, &gps_altitude,
                      &gps_vsat, &gps_usat, &gps_accuracy, &gps_year, &gps_month,
                      &gps_day, &gps_hour, &gps_minute, &gps_second)) {
      SerialMon.print("Latitude: ");
      SerialMon.print(gps_latitude, 8);
      SerialMon.print("\tLongitude: ");
      SerialMon.print(gps_longitude, 8);
      SerialMon.print("\tSpeed: ");
      SerialMon.print(gps_speed);
      SerialMon.print("\tAltitude: ");
      SerialMon.print(gps_altitude);
      SerialMon.print("\tVisible Satellites: ");
      SerialMon.print(gps_vsat);
      SerialMon.print("\tUsed Satellites: ");
      SerialMon.print(gps_usat);
      SerialMon.print("\tAccuracy: ");
      SerialMon.print(gps_accuracy);
      SerialMon.print("\tYear: ");
      SerialMon.print(gps_year);
      SerialMon.print("\tMonth: ");
      SerialMon.print(gps_month);
      SerialMon.print("\tDay: ");
      SerialMon.print(gps_day);
      SerialMon.print("\tHour: ");
      SerialMon.print(gps_hour);
      SerialMon.print("\tMinute: ");
      SerialMon.print(gps_minute);
      SerialMon.print("\tSecond: ");
      SerialMon.println(gps_second);
    } else {
      SerialMon.println("Couldn't get GPS/GNSS/GLONASS location, retrying in 15s.");
      delay(15000L);
    }

    /*
    for (int8_t i = 15; i; i--) {
      DBG("Requesting current GPS/GNSS/GLONASS location");
      if (modem.getGPS(&gps_latitude, &gps_longitude, &gps_speed, &gps_altitude,
                      &gps_vsat, &gps_usat, &gps_accuracy, &gps_year, &gps_month,
                      &gps_day, &gps_hour, &gps_minute, &gps_second)) {
        DBG("Latitude:", String(gps_latitude, 8),
            "\tLongitude:", String(gps_longitude, 8));
        DBG("Speed:", gps_speed, "\tAltitude:", gps_altitude);
        DBG("Visible Satellites:", gps_vsat, "\tUsed Satellites:", gps_usat);
        DBG("Accuracy:", gps_accuracy);
        DBG("Year:", gps_year, "\tMonth:", gps_month, "\tDay:", gps_day);
        DBG("Hour:", gps_hour, "\tMinute:", gps_minute, "\tSecond:", gps_second);
        break;
      } else {
        DBG("Couldn't get GPS/GNSS/GLONASS location, retrying in 15s.");
        delay(15000L);
      }
    }
    DBG("Retrieving GPS/GNSS/GLONASS location again as a string");
    String gps_raw = modem.getGPSraw();
  #if !defined(TINY_GSM_MODEM_SARAR5)  // not available for this module
    DBG("GPS/GNSS Based Location String:", gps_raw);
    //DBG("Disabling GPS");
    //modem.disableGPS();
  #endif
  */
}