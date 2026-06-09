#include <Arduino.h>
#include <SPI.h>
#include "definitions.h"
#include <AsyncWebServer_ESP32_SC_W5500.h>
#include <ElegantOTA.h>
#include "SimpleCLI.h"
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <esp_now.h>
#include <vector>
#define VERSION "1.0.0"


String terminal="";
/**** BUZZER *****/
int freq = 2000;
int channel = 1;
int resolution = 8;
bool enabledBuzzer=true;

Adafruit_NeoPixel pixels(2, PIN_LED_ADDR, NEO_GRB + NEO_KHZ800);


/******* BUZZER ******/
int melodia[] = {NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5};
int durata[] = {200, 200, 200, 400};  // Durata delle note in millisecondi

/***** WIFI *******/
const char* ssid = "DEVSS_NU";
const char* password = "123456789";

/***** CLI *******/
SimpleCLI cli;
Command cmdHelp;
Command cmdInfo;
Command cmdStatus;
Command cmdScan;
Command cmdTurnOffLTE;
Command cmdTurnOnLTE;
Command cmdTurnOnWiFi;
Command cmdTurnOffWiFi;
Command cmdTurnOnRPi;
Command cmdTurnOffRPi;
Command cmdReboot;
Command cmdSendPayloadBroadcast;
Command cmdSendPayloadToPeer;



bool rpiOn = false;
bool lteOn = false;
bool wifiOn = false;




/***** ESP-NOW ******/

typedef struct esp_peer_found {
    uint8_t mac[6];
    String ssid;
    int32_t channel;
    int32_t RSSI;
    String type;
} esp_peer_found;




typedef struct payload {
    uint8_t mac[6];
    String type;
    int channel;
    bool connected;
    int counter=0;
    JsonObject lastMessageJson; // Campo per memorizzare l'ultimo messaggio in formato JSON
    String lastMessage;
} payload;


uint8_t receiverMac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
typedef struct struct_message {
    char testo[128];
    int contatore;
    float lettura;
} struct_message;

//struct_message datiInviati;
//struct_message datiRicevuti;

esp_now_peer_info_t peerInfo;

payload payloads[10]; // Array per memorizzare più payload



/***** STATUS *****/


float current_voltage_batt1=0.0f;
float percent_batt1=0.0f;
float current_voltage_batt2=0.0f;
float percent_batt2=0.0f;

/***** TELNET ******/
WiFiServer telnetServer(23);


void beep(int count,int pause);
void debug(const char* msg);
void debug(String msg);
void debugln(const char* msg);
void debugf(const char* format, ...);
void debugln(String msg);
void initESPNow();
bool addPayload(const uint8_t* mac, String type, int ch,bool connected);
bool updatePayloadMessage(const uint8_t* mac_addr, String message,String& type);
bool registerDynamicPeer(const uint8_t* mac, int channel);
void serializedPayloads(JsonArray& arr);
std::vector<esp_peer_found> ESPNOWScanner();
void sendDataESPNOW(const uint8_t* mac, String data);
void sendDataESPNOWBroadcast(String data);
void initBuzzer();
void initGPIO();
bool turn_on_wifi(const char* ssid, const char* password, int channel, bool hidden, int maxConnections);
void turn_off_wifi();
void setup();
void loop();
void StatusTask(void *parameter);
void initWebserver();
void onOTAStart();
void onOTAProgress(size_t current, size_t final);
void onOTAEnd(bool success);
void setupCli();
void errorCLICallback(cmd_error* e);
void setupTelnet();
void sendFloat(float valore);
String sendATCommand(String command, int timeout);
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len);
void musicStart();
void updateStatus();
void readBattery();
void turn_on_lte();
void turn_off_lte();
void turn_on_raspberry();
void turn_off_raspberry();
JsonDocument getInfo();
String readGPSBackup();