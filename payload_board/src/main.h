#include <Arduino.h>
#include "definitions.h"
#include <AsyncTCP.h>
#include "SimpleCLI.h"
#include <ArduinoJson.h>
#include <esp_now.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define VERSION "1.0.0"

/**** BUZZER *****/
int freq = 2000;
int channel = 1;
int resolution = 8;
bool enabledBuzzer=true;


/***** WIFI *******/
#define PAYLOAD_TYPE "MBES"

const char* ssid = "DEVSS_MBES";
const char* password = "123456789";
/***** CLI *******/
SimpleCLI cli;
Command cmdHelp;
Command cmdStatus;
Command cmdInfo;

Command cmdReboot;
Command cmdScan;

Command cmdMotor;


/***** ESP-NOW ******/
uint8_t NUMac[6];
typedef struct struct_message {
    char testo[128];
    int contatore;
    float lettura;
} struct_message;

bool nu_connected=false;

//struct_message datiInviati;
//struct_message datiRicevuti;

esp_now_peer_info_t peerInfo;


/****** TEMPERATURE SENSOR ******/
#define ONE_WIRE_BUS 25
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

/***** STATUS *****/

int motorState=0;
float current_voltage=0.0f;


void beep(int count,int pause);
void debug(const char* msg);
void debug(String msg);
void debugln(const char* msg);
void debugf(const char* format, ...);
void debugln(String msg);
void initESPNow();
void ESPNOWNUScanner();
void parseCommand(const String json,const uint8_t *mac_addr_from);
bool registerDynamicPeer(const uint8_t* mac, int channel);
bool initWiFiAccessPoint(const char* ssid, const char* password, int channel, bool hidden, int maxConnections);
void setup();
void loop();
void setupCli();
void setupTelnet();
void sendFloat(float valore);

void updateStatus();
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len);


uint8_t voltageToPercent(float voltage);
float readVoltage();

void motorUp();
void motorDown();
void motorStop();
void StatusTask(void *parameter);
void printAddress(DeviceAddress deviceAddress);
JsonDocument getInfo();