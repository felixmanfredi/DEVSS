#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include "soc/uart_periph.h"

#define RXD2 16  
#define TXD2 17  
#define BRIDGE_SERIAL Serial2

const uint32_t BRIDGE_BAUD = 1500000;

// Configurazione Wi-Fi
const char* ssid = "DEVSS";
const char* password = "DevssAir1234";

// Configurazione UDP
const char* remoteIP = "255.255.255.255";
const uint16_t remotePort = 14553;
const uint16_t localPort = 14555;

WiFiUDP udp;
uint8_t buffer[2048];

IPAddress MCAST_ADDR(239, 255, 0, 1);
const uint16_t MCAST_PORT = 8877;

// ---- Frame dati UDP -> seriale (esistente) ----
const uint8_t SYNC_0 = 0xAA;
const uint8_t SYNC_1 = 0x55;
const size_t MAX_PACKET = 1500;
uint8_t packetBuffer[MAX_PACKET];

// ---- Frame REST ----
#define REST_SYNC_0 0xA1
#define REST_SYNC_1 0xA2
#define REST_RESP_SYNC_0 0xB1
#define REST_RESP_SYNC_1 0xB2
#define REST_MAX_PAYLOAD 4096

uint8_t restPayloadBuffer[REST_MAX_PAYLOAD];

enum RxState {
  RX_WAIT_SYNC0,
  RX_WAIT_SYNC1,
  RX_WAIT_LEN_LOW,
  RX_WAIT_LEN_HIGH,
  RX_WAIT_PAYLOAD,
  RX_WAIT_CHECKSUM
};

RxState rxState = RX_WAIT_SYNC0;
uint16_t rxPayloadLen = 0;
uint16_t rxPayloadIdx = 0;
uint8_t rxChecksum = 0;

// ---- Invio frame generico ----
void sendFramedRaw(uint8_t sync0, uint8_t sync1, const uint8_t* data, size_t len) {
  uint8_t checksum = 0;
  for (size_t i = 0; i < len; i++) {
    checksum ^= data[i];
  }

  uint8_t header[4];
  header[0] = sync0;
  header[1] = sync1;
  header[2] = (uint8_t)(len & 0xFF);
  header[3] = (uint8_t)((len >> 8) & 0xFF);

  BRIDGE_SERIAL.write(header, sizeof(header));
  BRIDGE_SERIAL.write(data, len);
  BRIDGE_SERIAL.write(checksum);
}

void sendFramed(const uint8_t* data, size_t len) {
  sendFramedRaw(SYNC_0, SYNC_1, data, len);
}

void sendRestResponse(const uint8_t* data, size_t len) {
  sendFramedRaw(REST_RESP_SYNC_0, REST_RESP_SYNC_1, data, len);
}

// ---- Esecuzione della chiamata REST ----
void handleRestRequest(uint8_t* payload, size_t len) {
  String data((char*)payload, len);

  int firstLineEnd = data.indexOf('\n');
  if (firstLineEnd < 0) return;

  String requestLine = data.substring(0, firstLineEnd);
  requestLine.trim();

  int spaceIdx = requestLine.indexOf(' ');
  if (spaceIdx < 0) return;

  String method = requestLine.substring(0, spaceIdx);
  String url = requestLine.substring(spaceIdx + 1);
  method.trim();
  url.trim();

  // Parsing header
  int pos = firstLineEnd + 1;
  const int MAX_HEADERS = 16;
  String headerNames[MAX_HEADERS];
  String headerValues[MAX_HEADERS];
  int headerCount = 0;

  while (pos < (int)data.length()) {
    int lineEnd = data.indexOf('\n', pos);
    if (lineEnd < 0) lineEnd = data.length();
    String line = data.substring(pos, lineEnd);
    line.trim();
    pos = lineEnd + 1;

    if (line.length() == 0) {
      break; // riga vuota -> fine header, inizio body
    }

    int colonIdx = line.indexOf(':');
    if (colonIdx > 0 && headerCount < MAX_HEADERS) {
      headerNames[headerCount] = line.substring(0, colonIdx);
      headerValues[headerCount] = line.substring(colonIdx + 1);
      headerNames[headerCount].trim();
      headerValues[headerCount].trim();
      headerCount++;
    }
  }

  String body = "";
  if (pos < (int)data.length()) {
    body = data.substring(pos);
  }

  // Esegue la richiesta
  HTTPClient http;
  http.setTimeout(5000); // evita blocchi troppo lunghi
  http.begin(url);

  for (int i = 0; i < headerCount; i++) {
    http.addHeader(headerNames[i], headerValues[i]);
  }

  method.toUpperCase();
  int httpCode = -1;

  if (method == "GET") {
    httpCode = http.GET();
  } else if (method == "POST") {
    httpCode = http.POST(body);
  } else if (method == "PUT") {
    httpCode = http.PUT(body);
  } else if (method == "PATCH") {
    httpCode = http.PATCH(body);
  } else if (method == "DELETE") {
    httpCode = http.sendRequest("DELETE", body);
  } else {
    httpCode = http.sendRequest(method.c_str(), body);
  }

  String responseBody;
  if (httpCode > 0) {
    responseBody = http.getString();
  } else {
    responseBody = String("ERRORE_HTTPCLIENT: ") + http.errorToString(httpCode);
  }

  http.end();

  String responsePayload = String(httpCode) + "\n" + responseBody;
  sendRestResponse((const uint8_t*)responsePayload.c_str(), responsePayload.length());
}

// ---- Lettura frame REST dalla seriale ----
void processSerialInput() {
  while (BRIDGE_SERIAL.available() > 0) {
    uint8_t b = BRIDGE_SERIAL.read();

    switch (rxState) {
      case RX_WAIT_SYNC0:
        if (b == REST_SYNC_0) rxState = RX_WAIT_SYNC1;
        break;

      case RX_WAIT_SYNC1:
        if (b == REST_SYNC_1) {
          rxState = RX_WAIT_LEN_LOW;
        } else if (b != REST_SYNC_0) {
          rxState = RX_WAIT_SYNC0;
        }
        break;

      case RX_WAIT_LEN_LOW:
        rxPayloadLen = b;
        rxState = RX_WAIT_LEN_HIGH;
        break;

      case RX_WAIT_LEN_HIGH:
        rxPayloadLen |= ((uint16_t)b << 8);
        if (rxPayloadLen == 0 || rxPayloadLen > REST_MAX_PAYLOAD) {
          rxState = RX_WAIT_SYNC0; // lunghezza non valida
        } else {
          rxPayloadIdx = 0;
          rxChecksum = 0;
          rxState = RX_WAIT_PAYLOAD;
        }
        break;

      case RX_WAIT_PAYLOAD:
        restPayloadBuffer[rxPayloadIdx++] = b;
        rxChecksum ^= b;
        if (rxPayloadIdx >= rxPayloadLen) rxState = RX_WAIT_CHECKSUM;
        break;

      case RX_WAIT_CHECKSUM:
        if (b == rxChecksum) {
          handleRestRequest(restPayloadBuffer, rxPayloadLen);
        }
        rxState = RX_WAIT_SYNC0;
        break;
    }
  }
}

void setup() {
  BRIDGE_SERIAL.setRxBufferSize(2048);
  #if defined(CONFIG_IDF_TARGET_ESP32)
    UART2.conf0.tick_ref_always_on = 1;
  #endif

  BRIDGE_SERIAL.begin(BRIDGE_BAUD, SERIAL_8N1, RXD2, TXD2);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  if (!udp.beginMulticast(MCAST_ADDR, MCAST_PORT)) {
    // errore multicast
  }
}

void loop() {
  // 1. Multicast/UDP -> seriale (MAVLink), come già presente
  int packetSize = udp.parsePacket();
  if (packetSize > 0 && packetSize <= (int)MAX_PACKET) {
    int len = udp.read(packetBuffer, MAX_PACKET);
    if (len > 0) {
      sendFramed(packetBuffer, (size_t)len);
    }
  }

  // 2. Seriale -> richieste REST -> HTTP -> risposta su seriale
  processSerialInput();
}