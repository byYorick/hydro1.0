#include "encoder.h"
#include "driver/gpio.h"
#include "esp_log.h"

// Encoder pin definitions (will be passed from main app)
static int enc_a_pin = -1;
static int enc_b_pin = -1;
static int enc_sw_pin = -1;

void encoder_init(void)
{
    if (enc_a_pin == -1 || enc_b_pin == -1 || enc_sw_pin == -1) {
        return;
    }
    
    gpio_set_direction(enc_a_pin, GPIO_MODE_INPUT);
    gpio_set_direction(enc_b_pin, GPIO_MODE_INPUT);
    gpio_set_direction(enc_sw_pin, GPIO_MODE_INPUT);
    gpio_pullup_en(enc_sw_pin);
}

void encoder_set_pins(int a_pin, int b_pin, int sw_pin)
{
    enc_a_pin = a_pin;
    enc_b_pin = b_pin;
    enc_sw_pin = sw_pin;
}