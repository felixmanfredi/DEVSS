# DEVSS Navigation Unit - Gateway & Payload Manager

Questo firmware trasforma un modulo **ESP32** nella **DEVSS Navigation Unit**: un gateway di navigazione e comunicazione industriale ad architettura concorrente. Il dispositivo funge da coordinatore centrale (Access Point) in grado di gestire nodi radio esterni tramite **ESP-NOW**, interfacciarsi con un computer di bordo (**Nvidia Jetson** / **Raspberry Pi**), monitorare pacchi batteria multipli e veicolare dati all'esterno tramite un modem **LTE**.

---

## 🛠️ Architettura di Sistema e Multi-Tasking

Il firmware è stato progettato per sfruttare l'architettura dual-core dell'ESP32 tramite il sistema operativo in tempo reale **FreeRTOS**. La gestione dei flussi è suddivisa per ottimizzare le tempistiche ed evitare colli di bottiglia computazionali:

* **Core 0 (Loop Principale):** Gestisce l'I/O asincrono e il parsing in tempo reale dei comandi di testo in ingresso provenienti sia dal Monitor Seriale (`Serial`) che dal computer di bordo (`serialJetson`).
* **Core 1 (StatusTask dedicato):** Un task hardware parallelo (`StatusTask`) che si risveglia ogni secondo ($1000\text{ ms}$) per campionare i canali ADC della telemetria, calcolare lo stato delle batterie e serializzare i dati verso la Jetson.

                  ┌───────────────────────────┐
                  │    ESP32 Dual-Core MCU    │
                  └─────────────┬─────────────┘
         ┌──────────────────────┴──────────────────────┐
         ▼ (Core 0 - I/O & CLI)                        ▼ (Core 1 - FreeRTOS Task)
┌──────────────────────────┐                  ┌──────────────────────────┐
│ Loop: Lettura Seriale    │                  │ StatusTask (Ogni 1s)     │
│ Parsing Comandi CLI      │                  │ Lettura ADC Batterie     │
│ Instradamento Radio      │                  │ Serializzazione JSON     │
└──────────────────────────┘                  └──────────────────────────┘

---

## 📂 Moduli Funzionali e Comportamento

### 1. Sistema di Gestione Energetica (`readBattery`)
La funzione monitora lo stato di salute dei sistemi di alimentazione mediante gli ADC interni dell'ESP32:
* Legge il valore di tensione nativo in millivolt tramite `analogReadMilliVolts`.
* Converte il dato in tensione reale applicando un fattore di partizione hardware (`DIV_FACTOR_CH0`) e un offset di calibrazione software (`ADC_CALIB_OFFSET`).
* Calcola la carica residua interpolando linearmente i valori tra i limiti strutturali minimi e massimi di una configurazione LiPo 6S ($V_{min}$ e $V_{max}$).
* Gestisce in modo indipendente due canali distinti (`batt1` e `batt2`).

### 2. Core Radio ESP-NOW e Accoppiamento Dinamico
Il gateway orchestra una rete a stella con sensori/attuatori remoti (denominati *payloads*):
* **`ESPNOWScanner()`**: Cambia temporaneamente lo stato radio in `WIFI_AP_STA` per scansionare l'ambiente Wi-Fi. Cerca beacon con SSID aventi prefisso `"DEVSS_"`, ne estrae il tipo di periferica e il MAC address e li registra come peer attivi sulla stessa frequenza dell'Access Point (`WIFI_IF_AP`).
* **Handshake automatico**: All'atto dell'accoppiamento (`addPayload`), il gateway invia immediatamente al nodo un comando JSON di sincronizzazione (`{"cmd":"connect"}`).
* **`OnDataRecv(...)`**: Riceve in modalità asincrona le strutture dati radio native (`struct_message`), estrae le stringhe JSON annidate, aggiorna i buffer storici e incrementa i contatori di diagnostica del pacchetto.

### 3. Ottimizzazione della Telemetria JSON
Per massimizzare il data-rate e ridurre il carico di banda sulle interfacce seriali e Wi-Fi, la struttura del dizionario JSON globale (`status`) utilizza chiavi condensate a singolo carattere:
* `V1` / `V2`: Tensione in Volt della Batteria 1 e Batteria 2.
* `P1` / `P2`: Percentuale di carica residua della Batteria 1 e Batteria 2.
* `PL`: Array contenente la lista dei nodi payloads attivi, mappati con:
  * `M`: MAC Address del nodo (Formato stringa `HEX`).
  * `T`: Tipo di dispositivo (*Device Type*).
  * `LM`: Ultimo payload di dati o telemetria inviato dal nodo (*Last Message*).
  * `C`: Conteggio totale dei messaggi ricevuti dal boot (*Counter*).

### 4. Controllo Periferiche e Modulo LTE
Il sistema espone funzioni dedicate per l'attivazione o il sezionamento elettrico controllato dei carichi di bordo (`turn_on` / `turn_off`):
* **Modem LTE**: Gestito tramite sequenze GPIO di alimentazione e reset temporizzate e controllato tramite comandi AT via UART1 (`lteSerial`) a $115200\text{ baud}$.
* **Computer di Bordo**: Gestione della linea di alimentazione del Raspberry Pi tramite pin dedicato.
* **Wi-Fi Web Server**: Espone gli endpoint di diagnostica (`/info` e `/status`) e integra la libreria *ElegantOTA* per l'aggiornamento sicuro del firmware via browser HTTP.

---

## 💻 Interfaccia CLI (Command Line Interface)

La Navigation Unit accetta comandi di controllo in formato testuale sia dal terminale di debug locale (`Serial`) sia dai messaggi in arrivo sulla seriale della Jetson (`serialJetson`).

### Comandi di Sistema e Stato
| Comando | Descrizione |
| :--- | :--- |
| `help` | Mostra la lista dei comandi CLI supportati con le relative legende. |
| `info` | Restituisce un JSON con i metadati statici dell'unità (Versione, Uptime, Stati ON/OFF di Wi-Fi, LTE, RPi). |
| `status` | Stampa l'oggetto JSON dinamico contenente la telemetria delle batterie e dei payload. |
| `reboot` | Esegue un riavvio hardware controllato del microcontrollore. |

### Comandi di Gestione Hardware (Power Management)
| Comando | Descrizione |
| :--- | :--- |
| `wifi_on` / `wifi_off` | Attiva o disattiva l'Access Point Wi-Fi integrato e il relativo server web. |
| `lte_on` / `lte_off` | Accende/Spegne il modem LTE eseguendo i timing di alimentazione e la seriale UART1. |
| `rpi_on` / `rpi_off` | Alimenta o toglie l'alimentazione alla linea del computer di bordo (Raspberry Pi). |

### Comandi di Rete e Messaggistica Radio
| Comando | Descrizione |
| :--- | :--- |
| `scan` | Avvia una scansione ambientale Wi-Fi per agganciare nuovi moduli `DEVSS_`. |
| `broadcast <messaggio>` | Invia una stringa a tutti i nodi ESP-NOW registrati contemporaneamente. |
| `send <type> <message>` | Cerca nella memoria del gateway un nodo del tipo specificato (es. `GPS`, `IMU`) e invia un messaggio unicamente al suo indirizzo MAC. |

---

## 🔌 Connessioni Hardware standard
* **UART0 (`serialJetson`)**: Connessione TX/RX diretta verso l'interfaccia seriale della Nvidia Jetson ($115200\text{ bps}$, `8N1`).
* **UART1 (`lteSerial`)**: Interfaccia dedicata alla comunicazione dati con il modem cellulare LTE.
* **ADC CH0**: Pin di lettura analogica deputato al monitoraggio delle celle della batteria tramite partitore di tensione.
* **PIN_RASPBERRY / PIN_MODEM**: Uscite digitali per il controllo di switch di alimentazione (MOSFET / Relè).
* **PIN_BUZZER**: Connesso al generatore di frequenza PWM hardware `LEDC` per le notifiche sonore di diagnostica.