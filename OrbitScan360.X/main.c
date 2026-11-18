#define FCY 4000000UL // 4 MHz (zgodnie z ustawieniem FOSC/2 w MCC)
#include <libpic30.h>
#include "mcc_generated_files/system.h"
#include "mcc_generated_files/pin_manager.h"
#include "mcc_generated_files/tmr1.h"
#include "mcc_generated_files/mccp1_compare.h"

volatile float dystans_cm = 0.0f;

// Funkcja mierz?ca odleg?o?? z czujnika HC-SR04
float zmierz_odleglosc_cm(void) {
    uint16_t timeout = 0;

    // 1. Impuls wyzwalaj?cy 10 us na RA0 (TRIG)
    TRIG_SetLow();
    __delay_us(2);
    TRIG_SetHigh();
    __delay_us(10);
    TRIG_SetLow();

    // 2. Oczekiwanie na pocz?tek odpowiedzi na RB14 (ECHO)
    while (ECHO_GetValue() == 0) {
        timeout++;
        if (timeout > 15000) return -1.0f; // Brak odpowiedzi (krótki timeout, ?eby nie zacina? serwa)
    }

    // 3. Pomiar czasu trwania stanu wysokiego
    TMR1_Counter16BitSet(0);
    TMR1_Start();

    timeout = 0;
    while (ECHO_GetValue() == 1) {
        timeout++;
        if (timeout > 15000) {
            TMR1_Stop();
            return -2.0f; // Obiekt poza zasi?giem
        }
    }
    TMR1_Stop();

    // 4. Przeliczenie tyków (1 tyk = 2 us) na centymetry
    uint16_t tyki = TMR1_Counter16BitGet();
    float czas_us = tyki * 2.0f;

    return czas_us / 58.0f;
}

// Funkcja aktualizuj?ca diod? LED na podstawie pomiaru
void sprawdz_przeszkode_i_led(void) {
    dystans_cm = zmierz_odleglosc_cm();

    // Zapal zielon? diod? RC4 (stan niski), gdy obiekt jest bli?ej ni? 20 cm
    if (dystans_cm > 0.0f && dystans_cm < 20.0f) {
        LED1_SetLow();
    } else {
        LED1_SetHigh();
    }
}

int main(void) {
    SYSTEM_Initialize();
    LED1_SetHigh(); // Wy??cz diod? na starcie

    uint16_t pozycja;
    uint8_t licznik_krokow = 0;

    while (1) {
        // 1. P?ynny ruch od min (660) do max (2500)
        for (pozycja = 660; pozycja <= 2500; pozycja += 5) {
            MCCP1_COMPARE_DualCompareValueSet(1, pozycja);
            
            // Wykonuj pomiar co 6 kroków serwa (ok. co 30 ms - optymalnie dla HC-SR04)
            licznik_krokow++;
            if (licznik_krokow >= 6) {
                sprawdz_przeszkode_i_led();
                licznik_krokow = 0;
            }
            
            __delay_ms(5);
        }

        // Pauza na maksymalnym wychyleniu z aktywnym skanowaniem (5 x 50 ms = 250 ms)
        for (uint8_t i = 0; i < 5; i++) {
            sprawdz_przeszkode_i_led();
            __delay_ms(50);
        }

        // 2. P?ynny ruch z powrotem od max (2500) do min (660)
        for (pozycja = 2500; pozycja >= 660; pozycja -= 5) {
            MCCP1_COMPARE_DualCompareValueSet(1, pozycja);
            
            licznik_krokow++;
            if (licznik_krokow >= 6) {
                sprawdz_przeszkode_i_led();
                licznik_krokow = 0;
            }
            
            __delay_ms(5);
        }

        // Pauza na minimalnym wychyleniu z aktywnym skanowaniem (250 ms)
        for (uint8_t i = 0; i < 5; i++) {
            sprawdz_przeszkode_i_led();
            __delay_ms(50);
        }
    }

    return 1;
}
