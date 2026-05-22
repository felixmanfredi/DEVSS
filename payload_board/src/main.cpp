#include "main.h"

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
  
  
  // Inizializza Access Point WiFi
  if (!initWiFiAccessPoint(ssid, password, 1, false, 4)) {
    Serial.println("Errore nell'inizializzazione dell'Access Point WiFi");
    while (true); // Blocca l'esecuzione
  }

  initESPNow(); // Inizializza ESP-NOW
 

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
    

           
    delay(10);
  
 
}


void sendFloat(float valore) {
  byte* p = (byte*)(void*)&valore;
  for (int i = 0; i < 4; i++) {
    Serial.write(p[i]); // Invia i 4 byte del float uno alla volta
  }
}


  