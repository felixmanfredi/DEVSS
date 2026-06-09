#include <main.h>
HardwareSerial lteSerial(1); // Usa UART1
HardwareSerial serialJetson(0); // Usa UART0
AsyncWebServer server(80);
JsonDocument status;
TaskHandle_t StatusTaskHandle = NULL;
String input;

/************** ESP-NOW *****************/
// Callback quando i dati vengono inviati dati in ESP-NOW
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {

}

// Callback quando si ricevono dati in ESP-NOW
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
   
    struct_message dataESPNOW;
    memcpy(&dataESPNOW, incomingData, sizeof(dataESPNOW));

    //cerca il payload corrispondente al MAC address del mittente e aggiorna il campo lastMessage con i dati ricevuti
    String type;
    if(updatePayloadMessage(mac_addr, String(dataESPNOW.testo), type)) {
    } else {
        //converti il dato ricevuto in json
        if(addPayload(mac_addr,type, 0, false)) {
            updatePayloadMessage(mac_addr, String(dataESPNOW.testo), type);
        }
    }
    
}

/**
 * Effettua una scansione delle reti Wi-Fi nelle vicinanze per identificare
 * i MAC Address dei potenziali dispositivi ESP-NOW.
 */
std::vector<esp_peer_found> ESPNOWScanner() {
  std::vector<esp_peer_found> foundPeers; // Vettore dinamico C++ (gestisce le String in automatico)
  
  debugln("\n--- Inizio Scansione Dispositivi Vicini ---");
  WiFiMode_t modalitaPrecedente = WiFi.getMode();
  WiFi.mode(WIFI_AP_STA); 

  int retiTrovate = WiFi.scanNetworks(false, true, false);

  if (retiTrovate > 0) {
    for (int i = 0; i < retiTrovate; ++i) {
      String ssid = WiFi.SSID(i);
      if(ssid.startsWith("DEVSS_")) {
        String type = ssid.substring(6);

        esp_peer_found peer;
        peer.channel = WiFi.channel(i);
        peer.RSSI = WiFi.RSSI(i);
        peer.ssid = ssid;
        peer.type = type; 
        memcpy(peer.mac, WiFi.BSSID(i), 6);

        foundPeers.push_back(peer); // Aggiunge solo i dispositivi validi, ridimensionandosi da solo

        addPayload(WiFi.BSSID(i), type, WiFi.channel(i), false); 
      }
      delay(10);
    }
  }

  WiFi.mode(modalitaPrecedente);
  WiFi.scanDelete();

  return foundPeers; // Sicuro, pulito, contiene solo i record validi
}

/**
  * Invia un messaggio broadcast a tutti i peer ESP-NOW registrati.
  * Il messaggio viene inviato come stringa JSON.
  */
void sendDataESPNOWBroadcast(String data) {
  // Invia a tutti i peer registrati (MAC address NULL)
  esp_err_t result = esp_now_send(NULL, (uint8_t *) data.c_str(), data.length());
  if (result != ESP_OK) {
    debug("Error: Errore nell'instradamento del messaggio broadcast, codice: ");
    //debugln(result);
  }
}

/**
 * Invia un messaggio a un peer ESP-NOW specifico, identificato dal suo MAC address.
 */
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
  if (result == ESP_OK) {
    debugln("Sent: Messaggio instradato correttamente dal chip radio.");
  } else {
    debug("Error: Errore nell'instradamento del messaggio, codice: ");
   
  }
}


/**
 * Aggiunge un nuovo payload alla lista dei payloads, se c'è spazio disponibile.
 * Ogni payload contiene il MAC address, il tipo di dispositivo e lo stato di connessione
 */
bool addPayload(const uint8_t* mac, String type, int ch,bool connected) {
  JsonDocument payloadDoc;
  payloadDoc["cmd"] = "connect";
  String jsonResponse;
  serializeJson(payloadDoc, jsonResponse);  
  
  for (int i = 0; i < 10; i++) {
      
        if (payloads[i].type == "") { // Trova la prima posizione libera
            memcpy(payloads[i].mac, mac, 6);
            payloads[i].type = type;
            payloads[i].channel = ch;
            payloads[i].connected = connected;
            if (registerDynamicPeer(mac, ch)) {
                sendDataESPNOW(mac, jsonResponse);
            }

            return true;
            
        }
    }
    return false;
}

/**
 * Aggiorna il campo lastMessage del payload corrispondente al MAC address specificato, se esiste.
 */
bool updatePayloadMessage(const uint8_t* mac_addr, String message,String& type) {
  String jsonData = message;
  JsonDocument receivedDoc;
  DeserializationError error = deserializeJson(receivedDoc, jsonData);
  if(!error) {
    type = receivedDoc["T"] | "unknown";
  }
  for (int i = 0; i < 10; i++) {
        if (memcmp(payloads[i].mac, mac_addr, 6) == 0) {
            
            payloads[i].lastMessage = jsonData;
            payloads[i].counter++;
            return true;
        }
    }
    return false;
}

/**
 * Registra dinamicamente un peer ESP-NOW con il MAC address e il canale specificati. Restituisce true se la registrazione ha successo, false altrimenti.
 */
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
        debugf("ESP-NOW: Nuovo peer [%02X:%02X:%02X:%02X:%02X:%02X] registrato su interfaccia AP.\n", 
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return true;
    } else {
        debugf("ESP-NOW: Errore registrazione peer. Codice: %d\n", addStatus);
        return false;
    }
}

/************* OTA *****************/
unsigned long ota_progress_millis = 0; // Timer per il logging del progresso OTA

/**
 * Callback chiamata all'inizio di un aggiornamento OTA.
 */
void onOTAStart() {
  // Log when OTA has started
  debugln("OTA update started!");
  // <Add your own code here>
}

/**
 * Callback chiamata durante il progresso di un aggiornamento OTA.
 */
void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    debugf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

/**
 * Callback chiamata al termine di un aggiornamento OTA.
*/
void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    debugln("OTA update finished successfully!");
  } else {
    debugln("There was an error during OTA update!");
  }
  // <Add your own code here>
}

/************ HELPER **************/

JsonDocument getInfo(){
  JsonDocument docInfo;
  docInfo["state"] = "OK";
  docInfo["version"] = VERSION;
  docInfo["uptime_seconds"] = millis() / 1000;
  docInfo["lte"] = lteOn ? "ON" : "OFF";
  docInfo["wifi"] = wifiOn ? "ON" : "OFF";
  docInfo["wifi_ssid"] = ssid;
  docInfo["wifi_channel"] = WiFi.channel();
  docInfo["wifi_password"] = password;
  docInfo["rpi"] = rpiOn ? "ON" : "OFF";

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
   terminal += msg;
   Serial.print(msg);
    
}

void debug(String msg) {
   terminal += msg;
   Serial.print(msg);
    
}

void debugln(const char* msg) {
    terminal += msg+ '\n';
    Serial.println(msg);
    
}

void debugf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    terminal += buffer;
    Serial.printf(format, args);
}

void debugln(String msg) {
    terminal += msg + '\n';
    Serial.println(msg.c_str());
}

/************ SETUP **************/

/**
 * Inizializza ESP-NOW, registrando le callback di invio e ricezione, e aggiungendo il peer con cui comunicare.
 */
void initESPNow(){
  if (esp_now_init() != ESP_OK) {
      debugln("Errore nell'inizializzazione di ESP-NOW");
      return;
  }

  // Registra la callback di invio
  esp_now_register_send_cb(OnDataSent);
  
  // Registra la callback di ricezione
  esp_now_register_recv_cb(OnDataRecv);

  /*
  // Registra il peer (l'altro ESP32)
  memcpy(peerInfo.peer_addr, receiverMac, 6);
  peerInfo.channel = 0;  // Usa il canale corrente del Wi-Fi
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      debugln("Impossibile aggiungere il peer");
      return;
  }
      */
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
bool turn_on_wifi(const char* ssid, const char* password, int channel, bool hidden, int maxConnections) {
    // Verifica che la password sia valida (minimo 8 caratteri per WPA2)
    if (password != NULL && strlen(password) < 8) {
        //debugln("Errore: la password deve essere di almeno 8 caratteri");
        return false;
    }
    // Imposta la modalità WiFi su Access Point
    WiFi.mode(WIFI_AP);

    // Configura l'access point
    bool result = WiFi.softAP(ssid, password, channel, hidden, maxConnections);
    if (result) {
        // Ottieni e stampa l'indirizzo IP dell'access point
        IPAddress apIP = WiFi.softAPIP();
       
        initWebserver();
        debugln("WLAN - Access Point avviato con successo.");
       
    } else {
        debugln("WLAN - Errore: impossibile avviare l'access point WiFi");
    }


    wifiOn = result;

    return result;
}

/**
 * Spegne il WiFi, disconnettendo tutti i client e disabilitando l'AP. Imposta lo stato wifiOn su false.
 */
void turn_off_wifi() {
    WiFi.softAPdisconnect(true); // Disconnette tutti i client e disabilita l'AP
    WiFi.mode(WIFI_OFF); // Spegne il WiFi completamente
    wifiOn = false;
    debugln("WLAN - Access Point WiFi spento");
}

/**
 * Inizializza il webserver.
 */
void initWebserver() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      
      String jsonResponse;
      serializeJson(status, jsonResponse);

      request->send(200, "application/json", jsonResponse);
    });

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
      
      String jsonResponse;
      serializeJson(status, jsonResponse);

      request->send(200, "application/json", jsonResponse);
    });

    server.on("/payloads", HTTP_GET, [](AsyncWebServerRequest *request) {
      
      JsonArray arr = JsonDocument().to<JsonArray>();
      for(int i=0; i<10; i++) {
        if(payloads[i].type != "") {
          JsonObject payloadObj = arr.createNestedObject();
          char macStr[18];
          sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
                  payloads[i].mac[0], payloads[i].mac[1], payloads[i].mac[2], 
                  payloads[i].mac[3], payloads[i].mac[4], payloads[i].mac[5]);
          payloadObj["mac"] = String(macStr);
          payloadObj["type"] = payloads[i].type;
          payloadObj["channel"] = payloads[i].channel;
          payloadObj["connected"] = payloads[i].connected;
          payloadObj["lastMessage"] = payloads[i].lastMessage;
          payloadObj["counter"] = payloads[i].counter;
        }
      }

      String jsonResponse;
      serializeJson(arr, jsonResponse);

      request->send(200, "application/json", jsonResponse);
    });


    server.on("/cli", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (request->hasParam("cmd", false)) {
        String cmd = request->getParam("cmd", false)->value();
        cli.parse(cmd);
      }

      // Costruiamo una pagina HTML completa con un Form di invio
      String htmlContent = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
      htmlContent += "<title>DEVSS Navigation Unit CLI</title>";
      htmlContent += "<style>";
      htmlContent += "  body { background-color: #121212; color: #e0e0e0; font-family: sans-serif; padding: 20px; }";
      htmlContent += "  .container { max-width: 900px; margin: 0 auto; }";
      htmlContent += "  h2 { color: #00ff66; font-size: 1.4rem; margin-bottom: 20px; }";
      htmlContent += "  form { display: flex; gap: 10px; margin-bottom: 20px; }";
      htmlContent += "  input[type='text'] { flex: 1; background-color: #1e1e1e; border: 1px solid #333; color: #fff; padding: 10px; font-family: monospace; border-radius: 4px; font-size: 1rem; }";
      htmlContent += "  input[type='text']:focus { border-color: #00ff66; outline: none; }";
      htmlContent += "  button { background-color: #00ff66; color: #121212; border: none; padding: 10px 20px; font-weight: bold; cursor: pointer; border-radius: 4px; font-size: 1rem; transition: background 0.2s; }";
      htmlContent += "  button:hover { background-color: #00cc55; }";
      htmlContent += "  pre { background-color: #1e1e1e; border: 1px solid #222; color: #ffffff; padding: 15px; border-radius: 4px; font-family: monospace; font-size: 0.95rem; overflow-x: auto; white-space: pre-wrap; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }";
      htmlContent += "</style></head><body>";
      htmlContent += "<div class='container'>";
      htmlContent += "  <h2>DEVSS Web Console Terminal</h2>";
      
      // Il form fa una chiamata GET verso /cli passandogli il parametro ?cmd=
      htmlContent += "  <form action='/cli' method='GET'>";
      htmlContent += "    <input type='text' name='cmd' placeholder='Inserisci un comando (es. status, scan, help)...' autofocus required>";
      htmlContent += "    <button type='submit'>Invia</button>";
      htmlContent += "  </form>";
      
      // Visualizzazione del log del terminale con i \n preservati
      htmlContent += "  <pre>" + terminal + "</pre>";
      htmlContent += "</div>";
      htmlContent += "</body></html>";

      AsyncWebServerResponse *response = request->beginResponse(200, "text/html", htmlContent);
      response->addHeader("Content-Disposition", "inline");
      request->send(response);
      delay(1000);
      terminal="";
    });
    

    server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
      
      std::vector<esp_peer_found> foundPeers = ESPNOWScanner();
      String jsonResponse;
      
      int size=foundPeers.size();

      JsonArray arr = JsonDocument().to<JsonArray>();
      for(int i=0; i<size; i++) {
        
          JsonObject peerObj = arr.createNestedObject();
          peerObj["ssid"] = foundPeers[i].ssid;
          peerObj["type"] = foundPeers[i].type;
          peerObj["channel"] = foundPeers[i].channel;
          peerObj["RSSI"] = foundPeers[i].RSSI;
          char macStr[18];
          sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
                  foundPeers[i].mac[0], foundPeers[i].mac[1], foundPeers[i].mac[2], 
                  foundPeers[i].mac[3], foundPeers[i].mac[4], foundPeers[i].mac[5]);
          peerObj["mac"] = String(macStr);
       
      }

       serializeJson(arr, jsonResponse); 


      request->send(200, "application/json", jsonResponse);
    });

    server.on("/info", HTTP_GET, [](AsyncWebServerRequest *request) {
      
      JsonDocument docInfo = getInfo();
      String jsonResponse;
      serializeJson(docInfo, jsonResponse);

      request->send(200, "application/json", jsonResponse);
    });

    server.on("/reboot",HTTP_GET,[](AsyncWebServerRequest *request){
      
      request->send(200, "application/json", "{\"result\":true}");
      ESP.restart();
    });

    server.begin();
    
  

    ElegantOTA.begin(&server);    // Start ElegantOTA
    // ElegantOTA callbacks
    ElegantOTA.onStart(onOTAStart);
    ElegantOTA.onProgress(onOTAProgress);
    ElegantOTA.onEnd(onOTAEnd);

    debugln("OTA - HTTP server started");
}

/**
 * Inizializza la GPIO, configurando i pin del Raspberry e del modem come uscite, e il pin ADC come ingresso. Imposta i pin dei LED su HIGH per tenerli spenti all'inizio.
 */
void initGPIO() {
  ledcSetup(BUZZER_CHANNEL, 6000, BUZZER_RESOLUTION);
  ledcAttachPin(PIN_BUZZER, BUZZER_CHANNEL);

  // Configura il pin del Raspberry come uscita
  pinMode(PIN_RASPBERRY, OUTPUT);
  pinMode(PIN_MODEM, OUTPUT);
  // Configura il pin ADC come ingresso (opzionale, poiché analogRead lo fa automaticamente)
  pinMode(PIN_ADC_CH0, INPUT);
}

/**
 * Accende il Raspberry Pi, impostando il pin di alimentazione su HIGH. Spegne il Raspberry Pi, impostando il pin di alimentazione su LOW.
 */
void turn_on_raspberry() {
  digitalWrite(PIN_RASPBERRY, HIGH); // Accendi Rpi
  rpiOn = true;
  debugln("Raspberry Pi acceso");
}

/**
 * Spegne il Raspberry Pi, impostando il pin di alimentazione su LOW.
 */
void turn_off_raspberry() {
  digitalWrite(PIN_RASPBERRY, LOW); // Spegni Rpi
  rpiOn = false;
  debugln("Raspberry Pi spento");
}

/**
 * Accende il modulo LTE.
 */
void turn_on_lte(){
  digitalWrite(PIN_MODEM, HIGH); // Accendi il modem
  debugln("Inizializzazione modulo LTE...");
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

  debugln("modulo LTE acceso e comunicazione seriale inizializzata");
  //telnetClient.println("modulo LTE pronto");
  sendATCommand("AT", 2000); // Test base  
  lteOn = true;

}
/**
 * Spegne il modulo LTE, disabilitando la comunicazione seriale e impostando il pin di alimentazione su LOW. Imposta lo stato lteOn su false.
 */
void turn_off_lte(){
  if (LTE_PWR_PIN > 0) {
    digitalWrite(LTE_PWR_PIN, LOW); // Spegni modulo LTE
    delay(3000);
  }
  lteSerial.end();
  lteOn = false;
  debugln("modulo LTE spento");
}

/**
 * Invia un comando AT al modulo LTE e attende una risposta per un certo timeout. Restituisce la risposta come stringa. Se non arriva nessuna risposta entro il timeout, restituisce un messaggio di timeout.
 */
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
  cli.setOnError(errorCLICallback);

  cmdHelp = cli.addCommand("help", [](cmd* c){
    debug(cli.toString());
  });
  cmdHelp.setDescription("Show this help message");
 
  cmdReboot = cli.addCommand("reboot", [](cmd* c){
    debugln("Riavvio in corso...");
    delay(100);
    ESP.restart();
  });
  cmdReboot.setDescription("Reboot the device");

  cmdInfo = cli.addCommand("info", [](cmd* c){
    
    JsonDocument docInfo = getInfo();
    String jsonResponse;
    serializeJson(docInfo, jsonResponse);
    debugln(jsonResponse.c_str());
  });
  cmdInfo.setDescription("Show device information and status");

  cmdStatus = cli.addCommand("status", [](cmd* c){
    String jsonResponse;
    serializeJson(status, jsonResponse);
    debugln(jsonResponse.c_str());
   

  });
  cmdStatus.setDescription("Show current status");

  cmdScan = cli.addCommand("scan", [](cmd* c){
    ESPNOWScanner();
  });
  cmdScan.setDescription("Scansiona i payload nelle vicinanze");
  
  cmdTurnOnLTE = cli.addCommand("lte_on", [](cmd* c){
    turn_on_lte();
  });
  cmdTurnOnLTE.setDescription("Accende il modulo LTE");
  
  cmdTurnOffLTE = cli.addCommand("lte_off", [](cmd* c){
    turn_off_lte();
  });
  cmdTurnOffLTE.setDescription("Spegne il modulo LTE");

  cmdTurnOnRPi = cli.addCommand("rpi_on", [](cmd* c){
    turn_on_raspberry();
  });
  cmdTurnOnRPi.setDescription("Accende il Raspberry Pi");

  cmdTurnOffRPi = cli.addCommand("rpi_off", [](cmd* c){
    turn_off_raspberry();
  });
  cmdTurnOffRPi.setDescription("Spegne il Raspberry Pi");

  cmdTurnOnWiFi = cli.addCommand("wifi_on", [](cmd* c){
    if(turn_on_wifi(ssid, password, 1, false, 4)) {
      debugln("WiFi Access Point attivo");
    } else {
      debugln("Errore nell'attivazione del WiFi Access Point");
    }
  });
  cmdTurnOnWiFi.setDescription("Accende il WiFi Access Point");

  cmdTurnOffWiFi = cli.addCommand("wifi_off", [](cmd* c){
    turn_off_wifi();
    debugln("WiFi Access Point spento");
  });
  cmdTurnOffWiFi.setDescription("Spegne il WiFi Access Point");


  cmdSendPayloadBroadcast = cli.addSingleArgCmd("broadcast", [](cmd* c){
    Command cmd(c); // Create wrapper object

    String payload = cmd.getArgument(0).getValue(); // Get the first argument as payload
    if(payload.length() == 0) {
      debugln("Usage: broadcast <message>");
      return;
    }
    sendDataESPNOWBroadcast(payload); // Invia a tutti i peer registrati
    debugln("Broadcast inviato: " + payload);
  });
  cmdSendPayloadBroadcast.setDescription("Invia un messaggio a tutti i peer ESP-NOW registrati. Usage: broadcast <message>");

  cmdSendPayloadToPeer = cli.addCommand("send", [](cmd* c){
    Command cmd(c); // Create wrapper object

    String typeStr = cmd.getArgument(0).getValue(); // Get the first argument as type
    String payload = cmd.getArgument(1).getValue(); // Get the second argument as payload

    if(typeStr.length() == 0 || payload.length() == 0) {
      debugln("Usage: send <type> <message>");
      return;
    }

    // Trova il primo payload che corrisponde al tipo specificato
    uint8_t* mac = nullptr;
    for (int i = 0; i < 10; i++) {
      if (payloads[i].type == typeStr) {
        mac = payloads[i].mac;
        break;
      }
    }
    if (!mac) {
      debugln("Payload non trovato per il tipo specificato");
      return;
    }

     // Formatta il MAC address in una stringa leggibile (es. "00:1A:2B:3C:4D:5E")
      char macStr[18];
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               mac[0], mac[1], mac[2],
               mac[3], mac[4], mac[5]);

    sendDataESPNOW(mac, payload); // Invia al peer specifico
   
  });
  cmdSendPayloadToPeer.setDescription("Invia un messaggio a un peer ESP-NOW specifico. Usage: send <type> <message>");
  cmdSendPayloadToPeer.addArgument("type");
  cmdSendPayloadToPeer.addArgument("message");

}

void setup() {
  Serial.begin(115200);

  debugln("DEVSS Navigation Unit");
  debugln("Version: " VERSION);
  debugln("");
  debugln("Inizializing...");

  serialJetson.begin(115200, SERIAL_8N1);
  setupCli(); // Inizializza l'interfaccia a riga di comando
  initGPIO(); // Inizializza la GPIO
  initBuzzer(); // Inizializza il buzzer
  
  turn_on_raspberry(); // Accende il Raspberry Pi
  turn_on_lte(); // Accende il modulo LTE


  

  // Inizializza Access Point WiFi
  if (!turn_on_wifi(ssid, password, 1, false, 4)) {
    debugln("Errore nell'inizializzazione dell'Access Point WiFi");
    while (true); // Blocca l'esecuzione
  }

  initESPNow(); // Inizializza ESP-NOW
  musicStart(); // Suona la melodia di avvio
  xTaskCreatePinnedToCore(
    StatusTask,         // Task function
    "StatusTask",       // Task name
    10000,             // Stack size (bytes)
    NULL,              // Parameters
    1,                 // Priority
    &StatusTaskHandle,  // Task handle
    1                  // Core 1
  );
  
  delay(1000);
  
  debugln("Ready to receive commands. Type 'help' for a list of commands.");
  
}

/************ LOOPS **************/
void loop() {

  
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


    if(serialJetson.available()) {
      String line = serialJetson.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) {
        cli.parse(line);
      }
    }
    
    delay(10);
    
 
}

void errorCLICallback(cmd_error* e) {
    CommandError cmdError(e); // Create wrapper object

    debug("ERROR: ");
    debugln(cmdError.toString().c_str());

    if (cmdError.hasCommand()) {
        debug("Did you mean \"");
        debug(cmdError.getCommand().toString().c_str());
        debugln("\"?");
    }
}

/**
* Task che si occupa di aggiornare periodicamente lo stato del dispositivo, leggendo la tensione della batteria e aggiornando il documento JSON con le informazioni correnti, inclusi i payloads dei dispositivi vicini. Il task invia lo stato aggiornato alla UART0 (modulo Jetson) ogni secondo.
 */
void StatusTask(void *parameter) {
  for (;;) { // Infinite loop
    updateStatus();  
    
    String jsonResponse;
    serializeJson(status, jsonResponse);
    serialJetson.println(jsonResponse); // Invia lo stato aggiornato alla UART0 (modulo Jetson)

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    
  }
}

/**
 * Aggiorna lo stato del dispositivo, leggendo la tensione della batteria e aggiornando il documento JSON con le informazioni correnti, inclusi i payloads dei dispositivi vicini.
 */
void updateStatus(){
  readBattery();
 
  
  status["V1"] = current_voltage_batt1;
  status["P1"]= percent_batt1;
  status["V2"] = current_voltage_batt2;
  status["P2"]= percent_batt2;
  status["G"] = readGPSBackup();

  JsonArray payloadsArray = status["PL"].to<JsonArray>();
  serializedPayloads(payloadsArray);
 

}

/**
 * Serializza i payloads in un array JSON da includere nella risposta API.
 * Ogni payload contiene il MAC address, il tipo di dispositivo, lo stato di connessione
 */
void serializedPayloads(JsonArray& arr) {
  for (int i = 0; i < 10; i++) {
    
    if (payloads[i].type != "") {
      JsonObject obj = arr.add<JsonObject>(); // Sintassi corretta per ArduinoJson v7
      
      // 1. Formatta il MAC address in una stringa leggibile (es. "00:1A:2B:3C:4D:5E")
      char macStr[18];
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               payloads[i].mac[0], payloads[i].mac[1], payloads[i].mac[2],
               payloads[i].mac[3], payloads[i].mac[4], payloads[i].mac[5]);
               
      // 2. Assegna i valori all'oggetto JSON
      obj["M"] = String(macStr);
      obj["T"] = payloads[i].type;
      obj["LM"] = payloads[i].lastMessage;
      obj["C"] = payloads[i].counter;
    }
  }
}

/**
 * Invia un valore float come messaggio ESP-NOW, convertendo il float in un array di byte e inviando i byte uno alla volta. Utile per inviare dati numerici precisi a un peer ESP-NOW.
 */
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
  int adcValue = analogReadMilliVolts(PIN_ADC_CH0); // Sostituisci con il pin corretto

  // Calcola la tensione reale applicando il fattore di divisione
  current_voltage_batt1 = ((float)adcValue/1000) * DIV_FACTOR_CH0; // Sostituisci con il fattore di divisione corretto

  // Applica l'offset di calibrazione se necessario
  current_voltage_batt1 += ADC_CALIB_OFFSET;

  float percentuale = (current_voltage_batt1 - V_MIN_6S) / (V_MAX_6S - V_MIN_6S) * 100.0;
  percent_batt1 = constrain(percentuale, 0.0, 100.0);

  // TODO: Se hai un secondo canale ADC per la seconda batteria, ripeti il processo per aggiornare current_voltage_batt2 e percent_batt2
  current_voltage_batt2 = current_voltage_batt1; // Sostituisci con il fattore di divisione corretto
  percent_batt2 = percent_batt1; // Sostituisci con il fattore di divisione corretto
}

/**
 * Legge le informazioni GPS dal modulo LTE, accendendo il GPS se necessario, e restituendo le informazioni formattate come stringa. Se il GPS è spento, lo accende e attende un momento prima di richiedere le informazioni. Se il GPS è acceso, richiede direttamente le informazioni sulla posizione GPS.
 */
String readGPSBackup(){
  if(lteOn){
    // 1. Accendi il GPS se non è già acceso
    String gpsPower = sendATCommand("AT+CGPS?", 2000);
    if (gpsPower.indexOf("+CGPS: 0") != -1) {
        sendATCommand("AT+CGPS=1,1", 2000);
        delay(1000); // Attendi un momento per l'accensione
    }

    // 2. Richiedi le informazioni sulla posizione GPS
    String gpsInfo = sendATCommand("AT+CGPSINFO", 5000);

    if (gpsInfo.indexOf("+CGPSINFO:") != -1) {
        // Estrai e formatta i dati
        gpsInfo.replace("+CGPSINFO: ", "");
        gpsInfo.replace("\r\n\r\nOK","");
        return gpsInfo; // Restituisce le informazioni GPS formattate 
      
    }
  }

  return ""; // Restituisce una stringa vuota se il GPS è spento o se c'è un errore
}

void musicStart() {
  for (int i = 0; i < 4; i++) {
    ledcWriteTone(BUZZER_CHANNEL, melodia[i]);
    ledcWrite(BUZZER_CHANNEL, BUZZER_DUTY_CYCLE);
    delay(durata[i]);
    ledcWrite(BUZZER_CHANNEL, 0);  // Spegne il suono
    delay(50);  // Breve pausa tra le note
  }
}