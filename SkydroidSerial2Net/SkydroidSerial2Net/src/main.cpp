#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "soc/uart_periph.h"

#define RXD2 16  
#define TXD2 17  

// Configurazione Wi-Fi
const char* ssid = "DEVSS AIR";
const char* password = "devssair";

// Configurazione UDP
const char* remoteIP = "255.255.255.255"; // Sostituisci con l'IP del PC/Dispositivo remoto che riceve i dati
const uint16_t remotePort = 14553;      // Porta remota (14550 è quella standard per Mavlink/QGroundControl)
const uint16_t localPort = 14555;       // Porta locale dell'ESP32 per ascoltare i dati in ingresso

WiFiUDP udp;
uint8_t buffer[2048]; // Buffer per il transito dei dati

void setup() {
  Serial.begin(115200); 
  
  // Inizializza la Serial2 ad alta velocità
  Serial2.setRxBufferSize(2048);
  #if defined(CONFIG_IDF_TARGET_ESP32)
    UART2.conf0.tick_ref_always_on = 1; // Forza il clock APB a 80MHz per precisione del baudrate
  #endif
  Serial2.begin(2000000, SERIAL_8N1, RXD2, TXD2);

  // Connessione Wi-Fi
  Serial.print("Connessione a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWi-Fi Connesso!");
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());

  // Avvio del socket UDP
  udp.begin(localPort);
  Serial.printf("Ascolto UDP avviato sulla porta %d\n", localPort);
}

void loop() {
  // 1. DA SERIALE (Skydroid) A UDP (Rete)
  int availableSerial = Serial2.available();
  if (availableSerial > 0) {
    // Leggi quanti più byte possibili nel limite del buffer
    int bytesToRead = min(availableSerial, (int)sizeof(buffer));
    Serial2.readBytes(buffer, bytesToRead);

    // Invia i dati letti via UDP
    udp.beginPacket(remoteIP, remotePort);
    udp.write(buffer, bytesToRead);
    udp.endPacket();
  }

  // 2. DA UDP (Rete) A SERIALE (Skydroid)
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    // Leggi il pacchetto UDP in arrivo
    int bytesRead = udp.read(buffer, min(packetSize, (int)sizeof(buffer)));
    
    // Scrivi i dati direttamente sulla Serial2
    Serial2.write(buffer, bytesRead);
  }
}