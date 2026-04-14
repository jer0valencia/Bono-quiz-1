#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_timer.h"

// ================= PINES =================
#define LED_GREEN 2
#define LED_RED   5

#define BTN_RIGHT 0
#define BTN_LEFT  22

#define DIR_A 21

#define PWM_NPN 18

#define POT ADC1_CHANNEL_6

// ================= DISPLAY =================
int seg_pins[7]   = {12,13,14,15,25,26,27};
int digit_pins[3] = {4,32,33};

// ================= VARIABLES =================
volatile int current_digit = 0;
volatile int display_digits[3] = {0,0,0}; // NUEVA LÓGICA
volatile int pwm_value = 0;

// ================= TABLA 7 SEG =================
const uint8_t numbers[10][7] = {
    {1,1,1,1,1,1,0},
    {0,1,1,0,0,0,0},
    {1,1,0,1,1,0,1},
    {1,1,1,1,0,0,1},
    {0,1,1,0,0,1,1},
    {1,0,1,1,0,1,1},
    {1,0,1,1,1,1,1},
    {1,1,1,0,0,0,0},
    {1,1,1,1,1,1,1},
    {1,1,1,1,0,1,1}
};

// ================= DISPLAY =================
void multiplex_display(void* arg)
{
    // Apagar todos los dígitos
    for(int i = 0; i < 3; i++)
        gpio_set_level(digit_pins[i], 0);

    int digit = display_digits[current_digit];

    // Si el dígito está "apagado"
    if(digit == -1){
        current_digit = (current_digit + 1) % 3;
        return;
    }

    // Encender segmentos
    for(int i = 0; i < 7; i++)
        gpio_set_level(seg_pins[i], !numbers[digit][i]);

    // Activar dígito
    gpio_set_level(digit_pins[current_digit], 1);

    current_digit = (current_digit + 1) % 3;
}

// ================= PWM =================
void pwm_control(void* arg)
{
    static int counter = 0;

    counter++;
    if(counter >= 255) counter = 0;

    gpio_set_level(PWM_NPN, (counter > pwm_value));
}

// ================= FUNCION NUEVA =================
void update_display_buffer(int number)
{
    
    if(number >= 100){
        display_digits[2] = number / 100;
        display_digits[1] = (number / 10) % 10;
        display_digits[0] = number % 10;
    }
    else if(number >= 10){
        display_digits[2] = -1; // apagado
        display_digits[1] = number / 10;
        display_digits[0] = number % 10;
    }
    else{
        display_digits[2] = -1;
        display_digits[1] = -1;
        display_digits[0] = number;
    }
}

// ================= MAIN =================
void app_main(void)
{
    int outputs[] = {LED_GREEN, LED_RED, DIR_A, PWM_NPN};

    for(int i = 0; i < 4; i++){
        gpio_reset_pin(outputs[i]);
        gpio_set_direction(outputs[i], GPIO_MODE_OUTPUT);
    }

    // Segmentos
    for(int i = 0; i < 7; i++){
        gpio_reset_pin(seg_pins[i]);
        gpio_set_direction(seg_pins[i], GPIO_MODE_OUTPUT);
    }

    for(int i = 0; i < 3; i++){
        gpio_reset_pin(digit_pins[i]);
        gpio_set_direction(digit_pins[i], GPIO_MODE_OUTPUT);
    }

    // Botones
    gpio_set_direction(BTN_RIGHT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN_RIGHT, GPIO_PULLUP_ONLY);

    gpio_set_direction(BTN_LEFT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN_LEFT, GPIO_PULLUP_ONLY);

    // ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(POT, ADC_ATTEN_DB_12);

    // TIMER DISPLAY
    esp_timer_handle_t display_timer;
    esp_timer_create_args_t display_args = {
        .callback = &multiplex_display
    };
    esp_timer_create(&display_args, &display_timer);
    esp_timer_start_periodic(display_timer, 2000);

    // TIMER PWM
    esp_timer_handle_t pwm_timer;
    esp_timer_create_args_t pwm_args = {
        .callback = &pwm_control
    };
    esp_timer_create(&pwm_args, &pwm_timer);
    esp_timer_start_periodic(pwm_timer, 50);

    // ================= LOOP =================
    while(1)
    {
        int adc = adc1_get_raw(POT);
        pwm_value = adc / 16;

        int btn_r = !gpio_get_level(BTN_RIGHT);
        int btn_l = !gpio_get_level(BTN_LEFT);

        // Dirección simplificada
        if(btn_r){
            gpio_set_level(DIR_A, 1);
            gpio_set_level(LED_GREEN, 1);
            gpio_set_level(LED_RED, 0);
        }
        else if(btn_l){
            gpio_set_level(DIR_A, 0);
            gpio_set_level(LED_GREEN, 0);
            gpio_set_level(LED_RED, 1);
        }

        int porcentaje = abs(((pwm_value * 100) / 255) - 100);

        update_display_buffer(porcentaje); // NUEVA LÓGICA

        printf("ADC:%d PWM:%d %%:%d\n",
               adc, pwm_value, porcentaje);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}