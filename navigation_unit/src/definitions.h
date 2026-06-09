#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <Arduino.h>


#define RXD1 18
#define TXD1 17


#define LTE_TX_PIN 6  // Pin TX dell'ESP32 collegato a RX del modulo LTE
#define LTE_RX_PIN 8  // Pin RX dell'ESP32 collegato a TX del modulo LTE
#define LTE_PWR_PIN 11 // Pin per accensione modulo LTE (opzionale)
#define LTE_RST_PIN 14 // Pin per reset modulo LTE (opzionale)
#define LTE_RI_PIN 7
#define LTE_STATUS_PIN 13
#define LTE_FLIGHTMODE_PIN 12
#define LTE_DTR_PIN 10

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
#define DIV_FACTOR_CH0    21.0000f   // 21M / 1M

// Offset di calibrazione globale (opzionale)
#define ADC_CALIB_OFFSET  0.0f


// --- MISC INPUTS/OUTPUTS ---
#define PIN_BUZZER      15  // Buzzer attivo [cite: 560]
#define PIN_LED_ADDR    42  // DOUT_LED_ADDR (SK6812/NeoPixel) [cite: 551]
#define PIN_RASPBERRY   48  // Indica se il Raspberry è acceso) [cite: 565]
#define PIN_MODEM   11  // Indica se il Raspberry è acceso) [cite: 565]

#define BUZZER_CHANNEL 0
#define BUZZER_RESOLUTION 8
#define BUZZER_DUTY_CYCLE 125  // 50% di 255 (8-bit)

#define V_MIN_6S  19.8 // ~3.3V per cella (Scarica)
#define V_MAX_6S  24.6 // 4.1V per cella (Carica)

// --- USB NATIVE PINS ---
// #define PIN_USB_DN      19  // USB D- [cite: 548]
// #define PIN_USB_DP      20  // USB D+ [cite: 552]

// Definizione delle note musicali (frequenze in Hz)
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523

#endif