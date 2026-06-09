#include "main.h"

TaskHandle_t StatusTaskHandle = NULL;


JsonDocument doc;

struct VoltagePoint {
  float voltage;
  uint8_t percent;
};

const VoltagePoint LIPO_4S_LUT[] = {
  { 12.00f,   0 },   // scarica completa (3.00V/cella)
  { 12.80f,   5 },   // zona critica
  { 13.20f,  10 },
  { 13.60f,  20 },
  { 14.00f,  30 },
  { 14.40f,  40 },
  { 14.80f,  55 },   // zona piatta centrale
  { 15.20f,  70 },
  { 15.60f,  85 },
  { 16.40f,  95 },
  { 16.80f, 100 },   // carica completa (4.20V/cella)
};

const uint8_t LUT_SIZE = sizeof(LIPO_4S_LUT) / sizeof(LIPO_4S_LUT[0]);

/************** ESP-NOW *****************/
// Callback quando i dati vengono inviati
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    //Serial.print("\r\nStato ultimo invio:\t");
    //Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Consegna Riuscita" : "Consegna Fallita");
    if(status != ESP_NOW_SEND_SUCCESS) {
      nu_connected = false; // Se l'invio fallisce, consideriamo il dispositivo disconnesso
      delay(2000); // Piccola pausa per evitare loop troppo rapidi in caso di problemi di connessione
      ESPNOWNUScanner(); // Riscansiona per cercare di ristabilire la connessione

    }else {
      nu_connected = true; // Se l'invio ha successo, il dispositivo è connesso
    }
}

// Callback quando si ricevono dati
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
    struct_message dataESPNOW;
    memcpy(&dataESPNOW, incomingData, sizeof(dataESPNOW));


    memcpy(NUMac, mac_addr, 6);
    
    parseCommand(String(dataESPNOW.testo), mac_addr);
    //Serial.printf("Testo: %s\n", datiRicevuti.testo);
    registerDynamicPeer(mac_addr, 1); // Registra dinamicamente il peer se non già presente
}


void parseCommand(const String json,const uint8_t *mac_addr_from) {
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.print("Errore nella deserializzazione del JSON: ");
    Serial.println(error.c_str());
    return;
  }

  const char* command = doc["cmd"];
  if(strcmp(command, "connect") == 0) {
    memcpy(NUMac, mac_addr_from, 6);
  }else if (strcmp(command, "motor") == 0) {
    int direction = doc["direction"];
    if(direction == 1) {
      motorUp();
    } else if (direction == 2) {
      motorDown();
    } else {
      motorStop();
    }
    
  } else if (strcmp(command, "reboot") == 0) {
    ESP.restart();
  } else {
    Serial.print("Comando sconosciuto: ");
    Serial.println(command);
  }
}


/**
 * Effettua una scansione delle reti Wi-Fi nelle vicinanze per identificare
 * i MAC Address dei potenziali dispositivi ESP-NOW.
 */
void ESPNOWNUScanner() {
  
  // La scansione Wi-Fi richiede temporaneamente la modalità STA o AP_STA
  WiFiMode_t modalitaPrecedente = WiFi.getMode();
  WiFi.mode(WIFI_AP_STA); 

  int retiTrovate = WiFi.scanNetworks(false, true, false);


  if (retiTrovate > 0) {
    
    for (int i = 0; i < retiTrovate; ++i) {

      String ssid = WiFi.SSID(i);
      //verifica se il SSID è un possibile dispositivo Payload ESP-NOW (es. se il nome SSID è vuoto o se il BSSID ha un certo prefisso)
      if(ssid.startsWith("DEVSS_NU")) {
        
        for(int j=0; j<6; j++) {
          NUMac[j] = WiFi.BSSID(i)[j];
        }

        nu_connected=registerDynamicPeer(WiFi.BSSID(i), WiFi.channel(i));
      }

    delay(10);
    }
  }

  // Ripristina la modalità originale (nel tuo caso WIFI_AP)
  WiFi.mode(modalitaPrecedente);
  
  // Pulisci i dati della scansione dalla memoria
  WiFi.scanDelete();
}

bool registerDynamicPeer(const uint8_t* mac, int channel) {
    // Verifica se il peer è già registrato
    if (esp_now_is_peer_exist(mac)) {
        return true; 
    }

    esp_now_peer_info_t dPeer;
    memset(&dPeer, 0, sizeof(esp_now_peer_info_t)); // Pulisce la memoria
    
    memcpy(dPeer.peer_addr, mac, 6);
    dPeer.channel = channel; 
    dPeer.encrypt = false;
    
    // --- SOLUZIONE DEL PROBLEMA ---
    // Specifichiamo l'interfaccia radio corretto: usiamo WIFI_IF_AP 
    // perché l'ESP sta funzionando come Access Point (WIFI_AP)
    dPeer.ifidx = WIFI_IF_AP; 

    esp_err_t addStatus = esp_now_add_peer(&dPeer);
    if (addStatus == ESP_OK) {
        Serial.printf("ESP-NOW: Nuovo peer [%02X:%02X:%02X:%02X:%02X:%02X] registrato su interfaccia AP.\n", 
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return true;
    } else {
        Serial.printf("ESP-NOW: Errore registrazione peer. Codice: %d\n", addStatus);
        return false;
    }
}
void sendDataESPNOW(const uint8_t* mac, String data) {

  struct_message dataEPSNOW;


  // 1. Prepariamo i dati da inviare
  strcpy(dataEPSNOW.testo, data.c_str());
  dataEPSNOW.contatore = 0;
  dataEPSNOW.lettura = 0.0f;

  // 2. Inviamo il messaggio
  // Parametri: MAC destinatario (o NULL per inviarlo a tutti i peer registrati), 
  //            puntatore ai dati convertito in uint8_t*, dimensione dei dati.
  esp_err_t result = esp_now_send(mac, (uint8_t *) &dataEPSNOW, sizeof(dataEPSNOW));
  // 3. Verifica immediata dell'invio radio
  if (result != ESP_OK) {
    Serial.print("Error: Errore nell'instradamento del messaggio, codice: ");
    Serial.println(result);
  }
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
 * Imposta l'interfaccia a riga di comando (CLI).
 */
void setupCli(){

  cli.setCaseSensitive(false);

  cmdHelp = cli.addCommand("help", [](cmd* c){
    Serial.print(cli.toString());
  });
  cmdHelp.setDescription("Show this help message");
 

  cmdReboot = cli.addCommand("reboot", [](cmd* c){
    Serial.println("Riavvio in corso...");
    delay(100);
    ESP.restart();
  });

  cmdScan = cli.addCommand("scan", [](cmd* c){
    Serial.println("Scansione reti WiFi in corso...");
    ESPNOWNUScanner();
  });


  cmdStatus = cli.addCommand("status", [](cmd* c){
    debug("NU Connected: ");
    debugln(nu_connected ? "Yes" : "No");
    debug(" SSID: ");
    debugln(ssid);
  
    debug(" MAC: ");
    debug(String(NUMac[0], HEX).c_str());
    debug("-");
    debug(String(NUMac[1], HEX).c_str());
    debug("-");
    debug(String(NUMac[2], HEX).c_str());
    debug("-");
    debug(String(NUMac[3], HEX).c_str());
    debug("-");
    debug(String(NUMac[4], HEX).c_str());
    debug("-");
    debugln(String(NUMac[5], HEX).c_str());


    debug(" Voltage: ");
    debugln(String(current_voltage).c_str());


    DeviceAddress insideThermometer, outsideThermometer;

    int devCount = 0;

     devCount = sensors.getDeviceCount();
  Serial.print("#devices: ");
  Serial.println(devCount);

  // report parasite power requirements
  Serial.print("Parasite power is: ");
  if (sensors.readPowerSupply()) Serial.println("ON");  // no address means "scan all devices for parasite mode"
  else Serial.println("OFF");

  // Search for devices on the bus and assign based on an index.
  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0");
  if (!sensors.getAddress(outsideThermometer, 1)) Serial.println("Unable to find address for Device 1");


      Serial.print("Device 0 Address: ");
  printAddress(insideThermometer);
  Serial.println();
  Serial.print("Power = parasite: ");
  Serial.println(sensors.readPowerSupply(insideThermometer));
  Serial.println();
  Serial.println();

  Serial.print("Device 1 Address: ");
  printAddress(outsideThermometer);
  Serial.println();
  Serial.print("Power = parasite: ");
  Serial.println(sensors.readPowerSupply(outsideThermometer));
  Serial.println();
  Serial.println();

  });
  cmdStatus.setDescription("Show current status");

  cmdMotor = cli.addSingleArgCmd("motor", [](cmd* c){
    Command cmd(c);
    
    int direction = cmd.getArgument(0).getValue().toInt();
    if(direction == 1) {
      motorUp();
      Serial.println("Motore su");
    } else if(direction == 2) {
      motorDown();
      Serial.println("Motore giù");
    } else if(direction == 0) {
      motorStop();
      Serial.println("Motore fermo");
    } else {
      Serial.println("Direzione sconosciuta. Usa '1', '2' o '0'.");
    }
  });
 
  
}

void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 0x10) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

int servoIndex=-1;
void setup() {
  Serial.begin(115200);
  setupCli(); // Inizializza l'interfaccia a riga di comando
  initBuzzer(); // Inizializza il buzzer

  pinMode(PIN_RELE1, OUTPUT);
  pinMode(PIN_RELE2, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(BTN1, INPUT_PULLDOWN);
  pinMode(BTN2, INPUT_PULLDOWN);
  pinMode(BTN3, INPUT_PULLDOWN);

  // Inizializza Access Point WiFi
  if (!initWiFiAccessPoint(ssid, password, 1, false, 4)) {
    Serial.println("Errore nell'inizializzazione dell'Access Point WiFi");
    while (true); // Blocca l'esecuzione
  }

  initESPNow(); // Inizializza ESP-NOW
  ESPNOWNUScanner();
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db); // range 0-3.3V


  sensors.begin(); // Inizializza il sensore di temperatura (se presente)
  sensors.setResolution(12); // Imposta la risoluzione a 12 bit (0.0625°C)
  sensors.setCheckForConversion(false); // Abilita il controllo dello stato di conversione
  

  xTaskCreatePinnedToCore(
    StatusTask,         // Task function
    "StatusTask",       // Task name
    10000,             // Stack size (bytes)
    NULL,              // Parameters
    1,                 // Priority
    &StatusTaskHandle,  // Task handle
    1                  // Core 1
  );
  

}



/************ LOOPS **************/
void loop() {

    int btn1State = digitalRead(BTN1);
    int btn2State = digitalRead(BTN2);
    int btn3State = digitalRead(BTN3);
    
    if(btn1State == HIGH) {
      motorUp();
    } else if (btn2State == HIGH) {
      motorDown();
    } else if (btn3State == HIGH) {
      motorStop();
    }
    
    // From serial
    String input = Serial.readString();
    if (input.length() > 0) {
      Serial.print("# ");
      Serial.print(input);
      cli.parse(input);
    }
    

   
    delay(10);
  
 
}

void updateStatus(){
  float voltage = readVoltage();
  doc["type"] = PAYLOAD_TYPE;
  doc["voltage"] = voltage;
  doc["percent"]= voltageToPercent(voltage);
  
  sensors.requestTemperatures(); // Send the command to get temperatures
  // After we got the temperatures, we can print them here.
  // We use the function ByIndex, and as an example get the temperature from the first sensor only.
  float tempC = sensors.getTempCByIndex(0);
  if (tempC != DEVICE_DISCONNECTED_C)
      doc["temperature"] = tempC;

  String jsonResponse;
  serializeJson(doc, jsonResponse);
  // Controlliamo se il MAC è valido (diverso da zero) prima di inviare
  uint8_t zeroMac[6] = {0, 0, 0, 0, 0, 0};
  if (memcmp(NUMac, zeroMac, 6) != 0) {
      sendDataESPNOW(NUMac, jsonResponse); 
  } 

  if(nu_connected) {
    digitalWrite(PIN_LED, HIGH);
  } else {
    digitalWrite(PIN_LED, LOW);
  }

}


void sendFloat(float valore) {
  byte* p = (byte*)(void*)&valore;
  for (int i = 0; i < 4; i++) {
    Serial.write(p[i]); // Invia i 4 byte del float uno alla volta
  }
}

float readVoltage() {
  long sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum += analogRead(ADC_PIN) + READING_OFFSET;
    delayMicroseconds(100);
  }
  float avg = sum / (float)NUM_SAMPLES;
  float vADC = (avg / ADC_MAX) * VREF;
  float vBatt = vADC * (R1 + R2) / R2;
  return vBatt;
}

uint8_t voltageToPercent(float voltage) {
  // Sotto il minimo → 0%
  if (voltage <= LIPO_4S_LUT[0].voltage) return 0;
  // Sopra il massimo → 100%
  if (voltage >= LIPO_4S_LUT[LUT_SIZE - 1].voltage) return 100;

  // Trova il segmento e interpola linearmente tra i due punti
  for (uint8_t i = 0; i < LUT_SIZE - 1; i++) {
    if (voltage <= LIPO_4S_LUT[i + 1].voltage) {
      float vLow  = LIPO_4S_LUT[i].voltage;
      float vHigh = LIPO_4S_LUT[i + 1].voltage;
      float pLow  = LIPO_4S_LUT[i].percent;
      float pHigh = LIPO_4S_LUT[i + 1].percent;

      // Interpolazione lineare tra i due punti della LUT
      float pct = pLow + (voltage - vLow) / (vHigh - vLow) * (pHigh - pLow);
      return (uint8_t)(pct + 0.5f);  // arrotondamento
    }
  }
  return 100;
}


void StatusTask(void *parameter) {
  for (;;) { // Infinite loop
    updateStatus();        
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    
  }
}


void motorUp(){
  digitalWrite(PIN_RELE1, HIGH);
  digitalWrite(PIN_RELE2, LOW);
  
}

void motorDown(){
  digitalWrite(PIN_RELE1, LOW);
  digitalWrite(PIN_RELE2, HIGH);
}

void motorStop(){
  digitalWrite(PIN_RELE1, LOW);
  digitalWrite(PIN_RELE2, LOW);
}