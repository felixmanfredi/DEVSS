#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <Arduino.h>

// --- UART DEFINITIONS ---
// Riferimento Page 3
// #define PIN_RX0_MAIN    44  // RX0 MCU [cite: 538]
// #define PIN_TX0_MAIN    43  // TX0 MCU [cite: 541]
#define PIN_RX2_AUX     38  // RX2 MCU / LED6 [cite: 563]
#define PIN_TX2_AUX     39  // TX2 MCU / LED5 [cite: 561]

// --- ADC / BATTERY MANAGEMENT PINS ---
// Riferimento Page 3 e Page 4
#define PIN_ADC_CH0     1   // ADC1_CH0 [cite: 473]
#define PIN_ADC_CH1     2   // ADC1_CH1 [cite: 481]
#define PIN_ADC_CH2     3   // ADC1_CH2 [cite: 486]
#define PIN_ADC_V_USB   4   // ADC1_CH3 (ADC_V_USB) [cite: 490]
#define PIN_ADC_CH4     5   // ADC1_CH4 [cite: 492]
#define PIN_ADC_CH5     6   // ADC1_CH5 [cite: 497]
#define PIN_ADC_CH6     7   // ADC1_CH6 [cite: 500]
#define PIN_ADC_CH7     8   // ADC1_CH7 [cite: 503]
#define PIN_ADC_CH8     9   // ADC1_CH8 (V_charger/Batt check) [cite: 506]
#define PIN_ADC_CH9     10  // ADC1_CH9 [cite: 510]

// --- BATTERY VOLTAGE DIVIDERS (Updated Table) ---
// Multipliers to convert ADC voltage to Real voltage
// Formula: Real_V = ADC_V * DIV_FACTOR
#define DIV_FACTOR_V_USB  2.0000f    // 100K / 100K (PIN_ADC_V_USB)
#define DIV_FACTOR_CH2    1.5000f    // 1M / 2M (Probabile Cella 1)
#define DIV_FACTOR_CH1    3.0246f    // 2470k / 1220k
#define DIV_FACTOR_CH5    5.4700f    // 4470k / 1M
#define DIV_FACTOR_CH6    6.3033f    // 6470k / 1220k
#define DIV_FACTOR_CH7    7.9426f    // 8470k / 1220k
#define DIV_FACTOR_CH8    9.1967f    // 10M / 1220k
#define DIV_FACTOR_CH4    10.8361f   // 12M / 1220k
#define DIV_FACTOR_CH9    10.8361f   // 12M / 1220k

// Offset di calibrazione globale (opzionale)
#define ADC_CALIB_OFFSET  0.0f


// --- MISC INPUTS/OUTPUTS ---
#define PIN_BUZZER      40  // Buzzer attivo [cite: 560]
#define PIN_LED    23  // DOUT_LED_ADDR (SK6812/NeoPixel) [cite: 551]
#define PIN_RELE1      16  // Relè 1 (Controllo motore su/giù) [cite: 559]
#define PIN_RELE2      17  // Relè 2 (Controllo motore
#define BTN1            19  // Pulsante 1 (Input digitale) [cite: 558]
#define BTN2            5  // Pulsante 2 (Input digitale) [cite:
#define BTN3            25  // Pulsante 2 (Input digitale) [cite:
#define PIN_PC          15
#define PIN_PC_STATE    0

<<<<<<< Updated upstream
// --- ARM MOTOR (apertura/chiusura bracci antenne GPS) ---
// JST Driver Motore - ESP32 (vedi etichetta): pin 3=IN1, 4=IN2, 5=SDA, 6=SCL
// Driver motore con sensore di corrente INA260 integrato (protezione fine corsa)
#define ARM_MOTOR_IN1   13  // IN1 driver (filo verde)
#define ARM_MOTOR_IN2   33  // IN2 driver (filo rosso)
#define ARM_I2C_SDA     14  // SDA INA260 (filo giallo)
#define ARM_I2C_SCL     12  // SCL INA260 (filo blu) - NB: GPIO12 e' uno strapping pin ESP32
#define BTN_ARM_APRI    26  // Pulsante DESTRA -> apre i bracci
#define BTN_ARM_CHIUDI  27  // Pulsante SINISTRA -> chiude i bracci

const float ARM_STOP_CURRENT_MA = 450.0f; // soglia sovracorrente per arresto motore (fine corsa)

// --- PROTEZIONE BATTERIA SCARICA ---
// Sotto questa percentuale (calcolata dalla LUT 4S): chiudi i bracci e alza il palo automaticamente
const uint8_t LOW_BATTERY_PERCENT_THRESHOLD = 20;
// Durata sollevamento automatico del palo (non c'e' sensore di corrente sul motore palo) - DA TARARE sul tempo di corsa reale
const unsigned long POLE_RAISE_ON_LOW_VOLTAGE_MS = 10000;
=======
// motore avanti => PIN_PWM_MOTOR_IN1 HIGH, PIN_PWM_MOTOR_IN2 LOW
// motore indietro => PIN_PWM_MOTOR_IN1 LOW, PIN_PWM_MOTOR_IN2 HIGH
// motore fermo => PIN_PWM_MOTOR_IN1 LOW, PIN_PWM_MOTOR_IN2 LOW
#define PIN_PWM_MOTOR_IN1 13 
#define PIN_PWM_MOTOR_IN2 33

// Sensore di corrente INA260
#define INA260_ADDRESS 0x40
#define PIN_I2C_SDA 14
#define PIN_I2C_SCL 12
#define BTN_MOTOR_ANTENNA1            26  // Pulsante apertura antenna (PULLUP)
#define BTN_MOTOR_ANTENNA2            27  // Pulsante chiusura antenna (PULLUP)
>>>>>>> Stashed changes

const int READING_OFFSET = 200;
const int ADC_PIN = 34;
const float R1 = 47000.0;
const float R2 = 4700.0;
const float VREF = 3.3;
const int ADC_MAX = 4095;
const int NUM_SAMPLES = 64; // media mobile per stabilità

// --- USB NATIVE PINS ---
// #define PIN_USB_DN      19  // USB D- [cite: 548]
// #define PIN_USB_DP      20  // USB D+ [cite: 552]

#endif