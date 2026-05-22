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
#define PIN_LED_ADDR    42  // DOUT_LED_ADDR (SK6812/NeoPixel) [cite: 551]



// --- USB NATIVE PINS ---
// #define PIN_USB_DN      19  // USB D- [cite: 548]
// #define PIN_USB_DP      20  // USB D+ [cite: 552]

#endif