#include "main.h"

TaskHandle_t StatusTaskHandle = NULL;
JsonDocument status;
String input;


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
    debugln(dataESPNOW.testo);
    parseCommand(String(dataESPNOW.testo), mac_addr);
    //Serial.printf("Testo: %s\n", datiRicevuti.testo);
    registerDynamicPeer(mac_addr, 1); // Registra dinamicamente il peer se non già presente
}


void parseCommand(const String json,const uint8_t *mac_addr_from) {
  JsonDocument doc;
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
      motorUp(false);
    } else if (direction == 2) {
      motorDown(false);
    } else {
      motorStop();
    }
  }else if (strcmp(command, "arm") == 0) {
    int direction = doc["direction"];
    if(direction == 1) {
      armApri(false);
    } else if (direction == 2) {
      armChiudi(false);
    } else {
      armStop();
    }
  }else if(strcmp(command,"pc")==0){
    int state = doc["state"];
      if(state) pc_on(); else pc_off();
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
      debugln("SSID "+ssid);
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


JsonDocument getInfo(){
  JsonDocument docInfo;
  docInfo["state"] = "OK";
  docInfo["version"] = VERSION;
  docInfo["uptime_seconds"] = millis() / 1000;
  docInfo["wifi_ssid"] = ssid;
  docInfo["wifi_channel"] = WiFi.channel();
  docInfo["wifi_password"] = password;
  docInfo["nu_connected"] = nu_connected;
  docInfo["nu_mac"]=printAddress(NUMac);
  docInfo["pc_state"]=pc_state;
  docInfo["arm_state"]=armMotorState;
  docInfo["arm_current_mA"]=armCurrent_mA;

  return docInfo;
}

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

void debug(String msg) {
   Serial.print(msg);
    
}

void debugln(const char* msg) {
    
    Serial.println(msg);
    
}

void debugf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
   
    Serial.printf(format, args);
}

void debugln(String msg) {
   
    Serial.println(msg.c_str());
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

    // Configura l'access point
    bool result = WiFi.softAP(ssid, password, channel, hidden, maxConnections);
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

    String jsonResponse;
    serializeJson(status, jsonResponse);
    debugln(jsonResponse.c_str());


  });
  cmdStatus.setDescription("Show current status");

  
  cmdInfo = cli.addCommand("info", [](cmd* c){
    
    JsonDocument docInfo = getInfo();
    String jsonResponse;
    serializeJson(docInfo, jsonResponse);
    debugln(jsonResponse.c_str());
  });
  cmdInfo.setDescription("Show device information and status");

  cmdMotor = cli.addSingleArgCmd("motor", [](cmd* c){
    Command cmd(c);
    
    int direction = cmd.getArgument(0).getValue().toInt();
    if(direction == 1) {
      motorUp(false);
      Serial.println("Motore su");
    } else if(direction == 2) {
      motorDown(false);
      Serial.println("Motore giù");
    } else if(direction == 0) {
      motorStop();
      Serial.println("Motore fermo");
    } else {
      Serial.println("Direzione sconosciuta. Usa '1', '2' o '0'.");
    }
  });
 
  cmdArm = cli.addSingleArgCmd("arm", [](cmd* c){
    Command cmd(c);

    int direction = cmd.getArgument(0).getValue().toInt();
    if(direction == 1) {
      armApri(false);
      Serial.println("Bracci: apertura");
    } else if(direction == 2) {
      armChiudi(false);
      Serial.println("Bracci: chiusura");
    } else if(direction == 0) {
      armStop();
      Serial.println("Bracci: stop");
    } else {
      Serial.println("Direzione sconosciuta. Usa '1' (apri), '2' (chiudi) o '0' (stop).");
    }
  });

  cmdPCOn=cli.addSingleArgCmd("pc_on",[](cmd* c){
    pc_on();
  });
  
  cmdPCOff=cli.addSingleArgCmd("pc_off",[](cmd* c){
    pc_off();
  });
}

String printAddress(uint8_t* deviceAddress)
{

  char macStr[18];
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               deviceAddress[0], deviceAddress[1], deviceAddress[2],
               deviceAddress[3], deviceAddress[4], deviceAddress[5]);

  return String(macStr);
  
}

void setup() {
  Serial.begin(115200);
 

  pinMode(PIN_RELE1, OUTPUT);
  pinMode(PIN_RELE2, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(BTN3, INPUT);
  pinMode(PIN_PC,OUTPUT);
  pinMode(PIN_PC_STATE,INPUT_PULLDOWN);
  digitalWrite(PIN_PC,LOW);

  pinMode(ARM_MOTOR_IN1, OUTPUT);
  pinMode(ARM_MOTOR_IN2, OUTPUT);
  pinMode(BTN_ARM_APRI, INPUT_PULLUP);
  pinMode(BTN_ARM_CHIUDI, INPUT_PULLUP);
  armStop();

  Wire.begin(ARM_I2C_SDA, ARM_I2C_SCL);
  if (armCurrentSensor.begin()) {
    armCurrentSensorFound = true;
    Serial.println("INA260 bracci: trovato");
  } else {
    Serial.println("INA260 bracci: NON trovato, protezione sovracorrente disabilitata");
  }

   setupCli(); // Inizializza l'interfaccia a riga di comando

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
  sensors.setCheckForConversion(true); // Abilita il controllo dello stato di conversione
  sensors.setWaitForConversion(true);

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
    
    
    if(btn1State == LOW) {
      poleAutoActive = false; // comando manuale: annulla il sollevamento automatico in corso
      motorUp(true);
    } else if (btn2State == LOW) {
      poleAutoActive = false;
      motorDown(true);
    } else if (btn1State == HIGH && btn2State==HIGH && motorState>0 && !poleAutoActive) {
      motorStop();
    }

    int btnArmApriState = digitalRead(BTN_ARM_APRI);
    int btnArmChiudiState = digitalRead(BTN_ARM_CHIUDI);

    if(btnArmApriState == LOW) {
      armAutoActive = false; // comando manuale: annulla la chiusura automatica in corso
      armApri(true);
    } else if (btnArmChiudiState == LOW) {
      armAutoActive = false;
      armChiudi(true);
    } else if (btnArmApriState == HIGH && btnArmChiudiState == HIGH && armMotorState>0 && !armAutoActive) {
      armStop();
    }

    checkArmOvercurrent();
    checkPoleAutoRaiseTimeout();

     // From serial
    if(Serial.available()) {
      char c = Serial.read();
      debug(String(c)); // Echo del carattere ricevuto
      if (c == '\n' || c == '\r') {
        if (input.length() > 0) {
          debug("# ");
          debugln(input);
          
          cli.parse(input);
          input = ""; // Pulisce l'input dopo averlo processato
        }
      } else if (c != -1) { // Se è stato letto un carattere valido
        input += c; // Aggiunge il carattere all'input
      }
    }
    

   
    delay(10);
  
 
}

void updateStatus(){
  float voltage = readVoltage();
  uint8_t percent = voltageToPercent(voltage);
  status["T"] = PAYLOAD_TYPE;
  status["V"] = voltage;
  status["P"]= percent;

  checkLowVoltageProtection(voltage, percent);

  sensors.requestTemperatures(); // Send the command to get temperatures
  // After we got the temperatures, we can print them here.
  // We use the function ByIndex, and as an example get the temperature from the first sensor only.
  float tempC = sensors.getTempCByIndex(0);
  if (tempC != DEVICE_DISCONNECTED_C)
      status["Tc"] = tempC-4; //applicato offset di 4°

  String jsonResponse;
  serializeJson(status, jsonResponse);
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

  pc_state=digitalRead(PIN_PC_STATE);

}

void pc_on(){
  digitalWrite(PIN_PC,HIGH);
  delay(500);
  digitalWrite(PIN_PC,LOW);
  Serial.println("PC on");
}

void pc_off(){
  digitalWrite(PIN_PC,HIGH);
  delay(5000);
  digitalWrite(PIN_PC,LOW);
  Serial.println("PC off");
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

void motorUp(bool monostate=false){
  debugln("Motor Up");
  if(monostate)
    motorState=1;
  digitalWrite(PIN_RELE1, HIGH);
  digitalWrite(PIN_RELE2, LOW);
  
}

void motorDown(bool monostate=false){
    debugln("Motor Down");
  if(monostate)
    motorState=2;
  digitalWrite(PIN_RELE1, LOW);
  digitalWrite(PIN_RELE2, HIGH);
}

void motorStop(){
    debugln("Motor Stop");

  motorState=0;
  digitalWrite(PIN_RELE1, LOW);
  digitalWrite(PIN_RELE2, LOW);
}

void armApri(bool monostate=false){
  debugln("Bracci: apertura");
  if(monostate)
    armMotorState=1;
  digitalWrite(ARM_MOTOR_IN1, HIGH);
  digitalWrite(ARM_MOTOR_IN2, LOW);
}

void armChiudi(bool monostate=false){
  debugln("Bracci: chiusura");
  if(monostate)
    armMotorState=2;
  digitalWrite(ARM_MOTOR_IN1, LOW);
  digitalWrite(ARM_MOTOR_IN2, HIGH);
}

void armStop(){
  debugln("Bracci: stop");

  armMotorState=0;
  digitalWrite(ARM_MOTOR_IN1, LOW);
  digitalWrite(ARM_MOTOR_IN2, LOW);
}

/**
 * Legge la corrente dal sensore INA260 sul driver dei bracci e ferma il motore
 * se supera la soglia (motore a fine corsa contro il fermo meccanico).
 */
void checkArmOvercurrent(){
  if (!armCurrentSensorFound || armMotorState == 0) return;

  armCurrent_mA = armCurrentSensor.readCurrent();
  if (armCurrent_mA > ARM_STOP_CURRENT_MA) {
    armStop();
    armAutoActive = false;
    Serial.print("Bracci: STOP per sovracorrente, corrente rilevata: ");
    Serial.println(armCurrent_mA);
  }
}

/**
 * Protezione batteria scarica: sotto LOW_BATTERY_PERCENT_THRESHOLD chiude i bracci
 * e alza il palo automaticamente (azione singola, non blocca i comandi manuali successivi).
 */
void checkLowVoltageProtection(float voltage, uint8_t percent){
  if (percent < LOW_BATTERY_PERCENT_THRESHOLD) {
    if (!lowVoltageActionDone) {
      lowVoltageActionDone = true;
      Serial.print("ATTENZIONE: batteria scarica (");
      Serial.print(percent);
      Serial.print("%, ");
      Serial.print(voltage, 2);
      Serial.println("V) - chiudo i bracci e alzo il palo");

      armAutoActive = true;
      armChiudi(true);

      poleAutoActive = true;
      poleAutoStartMillis = millis();
      motorUp(true);
    }
  } else {
    lowVoltageActionDone = false; // ripristina l'armamento della protezione quando la tensione risale
  }
}

/**
 * Ferma il palo dopo POLE_RAISE_ON_LOW_VOLTAGE_MS dal sollevamento automatico
 * (il motore palo non ha un sensore di corrente per rilevare il fine corsa).
 */
void checkPoleAutoRaiseTimeout(){
  if (poleAutoActive && (millis() - poleAutoStartMillis >= POLE_RAISE_ON_LOW_VOLTAGE_MS)) {
    motorStop();
    poleAutoActive = false;
  }
}