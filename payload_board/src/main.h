#include <Arduino.h>
#include "definitions.h"
#include <AsyncTCP.h>
#include "SimpleCLI.h"
#include <ArduinoJson.h>
#include <esp_now.h>
#include <WiFi.h>
#define VERSION "1.0.0"

/**** BUZZER *****/
int freq = 2000;
int channel = 1;
int resolution = 8;
bool enabledBuzzer=true;


/***** WIFI *******/
const char* ssid = "PAYLOAD";
const char* password = "123456789";

/***** CLI *******/
SimpleCLI cli;
Command cmdHelp;
Command cmdStatus;


/***** ESP-NOW ******/
uint8_t receiverMac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
typedef struct struct_message {
    char testo[32];
    int contatore;
    float lettura;
} struct_message;

struct_message datiInviati;
struct_message datiRicevuti;

esp_now_peer_info_t peerInfo;

/***** STATUS *****/


float current_voltage=0.0f;


void beep(int count,int pause);
void debug(const char* msg);
void debugln(const char* msg);
void initESPNow();
void initBuzzer();
bool initWiFiAccessPoint(const char* ssid, const char* password, int channel, bool hidden, int maxConnections);
void setup();
void loop();
void setupCli();
void setupTelnet();
void sendFloat(float valore);

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len);