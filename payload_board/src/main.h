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
#include <Wire.h>
#include <Adafruit_INA260.h>

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
Command cmdPCOn;
Command cmdPCOff;


Command cmdReboot;
Command cmdScan;

Command cmdMotor;
Command cmdArm;

bool pc_state=false;

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

/****** ARM MOTOR (bracci antenne GPS) ******/
Adafruit_INA260 armCurrentSensor = Adafruit_INA260();
bool armCurrentSensorFound = false;
int armMotorState = 0; // 0=stop, 1=apertura, 2=chiusura
float armCurrent_mA = 0.0f;
bool armAutoActive = false; // true mentre i bracci si chiudono per protezione batteria scarica (bypassa lo stop a rilascio pulsante)

/****** PROTEZIONE BATTERIA SCARICA ******/
bool lowVoltageActionDone = false; // evita di ritriggerare l'azione finche' la percentuale non risale sopra soglia
bool poleAutoActive = false; // true mentre il palo si alza per protezione batteria scarica (bypassa lo stop a rilascio pulsante)
unsigned long poleAutoStartMillis = 0;


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

void motorUp(bool monostate);
void motorDown(bool monostate);
void motorStop();

void armApri(bool monostate);
void armChiudi(bool monostate);
void armStop();
void checkArmOvercurrent();
void checkLowVoltageProtection(float voltage, uint8_t percent);
void checkPoleAutoRaiseTimeout();

void StatusTask(void *parameter);
String printAddress(uint8_t* deviceAddress);
JsonDocument getInfo();
void pc_on();
void pc_off();