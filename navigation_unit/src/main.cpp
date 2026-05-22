#include <main.h>
HardwareSerial lteSerial(1); // Usa UART1

AsyncWebServer server(80);
JsonDocument doc;
/************** ESP-NOW *****************/
// Callback quando i dati vengono inviati
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("\r\nStato ultimo invio:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Consegna Riuscita" : "Consegna Fallita");
}

// Callback quando si ricevono dati
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
    memcpy(&datiRicevuti, incomingData, sizeof(datiRicevuti));
    
    Serial.print("\r\n--- Dati Ricevuti da MAC: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X:", mac_addr[i]);
    }
    Serial.println();
    
    Serial.printf("Testo: %s\n", datiRicevuti.testo);
    Serial.printf("Contatore: %d\n", datiRicevuti.contatore);
    Serial.printf("Lettura: %.2f\n", datiRicevuti.lettura);
}

/************* OTA *****************/
unsigned long ota_progress_millis = 0; // Timer per il logging del progresso OTA

/**
 * Callback chiamata all'inizio di un aggiornamento OTA.
 */
void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
}

/**
 * Callback chiamata durante il progresso di un aggiornamento OTA.
 */
void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

/**
 * Callback chiamata al termine di un aggiornamento OTA.
*/
void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}

/************ HELPER **************/
void beep(int count=1,int pause=1000){
  if( enabledBuzzer){
    for(int i=0;i<count;i++){
      ledcWriteTone(channel, 2000);
      delay(pause);
      ledcWriteTone(channel, 0);
      delay(pause);
    }
  }
}

void debug(const char* msg) {
    Serial.print(msg);
    
}

void debugln(const char* msg) {
    Serial.println(msg);
    
}


/************ SETUP **************/

/**
 * Inizializza ESP-NOW, registrando le callback di invio e ricezione, e aggiungendo il peer con cui comunicare.
 */
void initESPNow(){
  if (esp_now_init() != ESP_OK) {
      Serial.println("Errore nell'inizializzazione di ESP-NOW");
      return;
  }

  // Registra la callback di invio
  esp_now_register_send_cb(OnDataSent);
  
  // Registra la callback di ricezione
  esp_now_register_recv_cb(OnDataRecv);

  // Registra il peer (l'altro ESP32)
  memcpy(peerInfo.peer_addr, receiverMac, 6);
  peerInfo.channel = 0;  // Usa il canale corrente del Wi-Fi
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Impossibile aggiungere il peer");
      return;
  }
}

/**
 * Inizializza il buzzer.
 */
void initBuzzer(){
  ledcSetup(channel, freq, resolution);
  ledcAttachPin(PIN_BUZZER, channel);
  enabledBuzzer=true;
  beep(3,100);
 
}

/**
 * Inizializza un Access Point WiFi e il webserver con i parametri specificati.
 * Restituisce true se l'AP è stato avviato con successo, false altrimenti.
 */
bool initWiFiAccessPoint(const char* ssid, const char* password, int channel, bool hidden, int maxConnections) {
    // Verifica che la password sia valida (minimo 8 caratteri per WPA2)
    if (password != NULL && strlen(password) < 8) {
        //debugln("Errore: la password deve essere di almeno 8 caratteri");
        return false;
    }
    // Imposta la modalità WiFi su Access Point
    WiFi.mode(WIFI_AP);

    //------- IP DI CLASSE DIVERSA DA QUELLA ETHERNET, ALTRIMENTI AVRò PROBLEMI DI DNS, LASCIO LA CONFIG AUTOMATICA, CLASSE 4-----------------------------
    // Configura IP personalizzato per AP
    // IPAddress local_IP(192, 168, 1, 111);
    // IPAddress gateway(192, 168, 1, 1);    // Gateway = IP dell'AP
    // IPAddress subnet(255, 255, 255, 0);    
    // WiFi.softAPConfig(local_IP, gateway, subnet);
    //-------------------------------------------------------------------------------------------------------------

    // Configura l'access point
    bool result = WiFi.softAP(ssid, password, channel, hidden, maxConnections);
    if (result) {
        // Ottieni e stampa l'indirizzo IP dell'access point
        IPAddress apIP = WiFi.softAPIP();
        debug("WLAN - Access Point \"");
        debug(ssid);
        debug("\" avviato. IP: ");
        debugln(apIP.toString().c_str());
        // Stampa altre informazioni utili
        debug("WLAN - Canale: ");
        debugln(String(channel).c_str());
        debug("WLAN - Password: ");
        if (password != NULL && strlen(password) > 0) {
            debugln(password);
        } else {
            debugln("Non impostata (rete aperta)");
        }
        debug("WLAN - Connessioni massime: ");
        debugln(String(maxConnections).c_str());
       
    } else {
        debugln("WLAN - Errore: impossibile avviare l'access point WiFi");
    }


    

    return result;
}

/**
 * Inizializza ElegantOTA con il webserver specificato.
 */
void initElegantOTA() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      
      
      String jsonResponse;
      serializeJson(doc, jsonResponse);

      request->send(200, "application/json", jsonResponse);
    });

    

    server.begin();
    Serial.println("OTA - HTTP server started");
  

    ElegantOTA.begin(&server);    // Start ElegantOTA
    // ElegantOTA callbacks
    ElegantOTA.onStart(onOTAStart);
    ElegantOTA.onProgress(onOTAProgress);
    ElegantOTA.onEnd(onOTAEnd);
}


void initGPIO() {
  // Configura il pin del Raspberry come uscita
  pinMode(PIN_RASPBERRY, OUTPUT);
  pinMode(PIN_MODEM, OUTPUT);



  digitalWrite(PIN_RASPBERRY, HIGH); // Assicurati che il LED sia spento all'inizio
  digitalWrite(PIN_MODEM, HIGH); // Assicurati che il LED sia spento all'inizio



  // Configura il pin ADC come ingresso (opzionale, poiché analogRead lo fa automaticamente)
  pinMode(PIN_ADC_CH0, INPUT);
}


void turn_on_lte(){

  Serial.println("Inizializzazione modulo LTE...");
  //telnetClient.println("Inizializzazione modulo LTE...");
  if (LTE_PWR_PIN > 0) {
    pinMode(LTE_PWR_PIN, OUTPUT);
    digitalWrite(LTE_PWR_PIN, HIGH); // Accendi modulo LTE
    delay(3000);
  }

  if (LTE_RST_PIN > 0) {
    pinMode(LTE_RST_PIN, OUTPUT);
    digitalWrite(LTE_RST_PIN, LOW);
    delay(100);
    digitalWrite(LTE_RST_PIN, HIGH);
    delay(100);
    digitalWrite(LTE_RST_PIN, LOW);
    delay(2000);
  }

  // Inizializza comunicazione seriale con modulo LTE
  lteSerial.begin(115200, SERIAL_8N1, LTE_RX_PIN, LTE_TX_PIN);
  delay(1000);

  Serial.println("modulo LTE pronto");
  //telnetClient.println("modulo LTE pronto");
  sendATCommand("AT", 2000); // Test base  

}

String sendATCommand(String command, int timeout) {
  // Pulisci buffer
  while (lteSerial.available()) {
    lteSerial.read();
  }

  // Invia comando
  lteSerial.println(command);

  // Attendi risposta
  unsigned long start = millis();
  String response = "";

  while (millis() - start < timeout) {
    if (lteSerial.available()) {
      char c = lteSerial.read();
      response += c;
    }
  }

  response.trim();
  if (response.length() == 0) {
    response = "TIMEOUT - No response from module";
  }
  
  return response;
}

/**
 * Imposta l'interfaccia a riga di comando (CLI).
 */
void setupCli(){

  cli.setCaseSensitive(false);

  cmdHelp = cli.addCommand("help", [](cmd* c){
    Serial.print(cli.toString());
  });
  cmdHelp.setDescription("Show this help message");
 





  cmdStatus = cli.addCommand("status", [](cmd* c){
    debugln("Status:");
    
    
    
    
    debug(" Voltage: ");
    debugln(String(current_voltage).c_str());

  });
  cmdStatus.setDescription("Show current status");

 
  
}



int servoIndex=-1;
void setup() {
  Serial.begin(115200);
  setupCli(); // Inizializza l'interfaccia a riga di comando
  initBuzzer(); // Inizializza il buzzer
  
  pixels.begin();

  // Inizializza Access Point WiFi
  if (!initWiFiAccessPoint(ssid, password, 1, false, 4)) {
    Serial.println("Errore nell'inizializzazione dell'Access Point WiFi");
    while (true); // Blocca l'esecuzione
  }

  initESPNow(); // Inizializza ESP-NOW
  initElegantOTA(); // Inizializza ElegantOTA
  initGPIO(); // Inizializza la GPIO
  turn_on_lte(); // Accende il modulo LTE

}



/************ LOOPS **************/
void loop() {

  
    // From serial
    String input = Serial.readString();
    if (input.length() > 0) {
      Serial.print("# ");
      Serial.print(input);
      cli.parse(input);
    }
    
    updateStatus(); // Legge la tensione della batteria e aggiorna current_voltage
    
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    Serial.println(jsonResponse);

    delay(10);
    
 
}

void updateStatus(){
  readBattery();
  doc["status"] = "OK";
  doc["version"] = VERSION;
  doc["voltage"] = current_voltage;
  doc["uptime_seconds"] = millis() / 1000;


}


void sendFloat(float valore) {
  byte* p = (byte*)(void*)&valore;
  for (int i = 0; i < 4; i++) {
    Serial.write(p[i]); // Invia i 4 byte del float uno alla volta
  }
}


/**
 * Legge la tensione della batteria utilizzando i pin ADC e applicando i fattori di divisione per ottenere il valore reale.
 * Aggiorna la variabile globale current_voltage con il valore calcolato.
 */
void readBattery(){
  // Leggi il valore ADC dal pin specificato
  int adcValue = analogRead(PIN_ADC_CH0); // Sostituisci con il pin corretto

  // Calcola la tensione reale applicando il fattore di divisione
  current_voltage = adcValue * DIV_FACTOR_CH0; // Sostituisci con il fattore di divisione corretto

  // Applica l'offset di calibrazione se necessario
  current_voltage += ADC_CALIB_OFFSET;
}