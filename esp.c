#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"     
#include "lib/font.h"
#include "lib/ssd1306.h"
#include "hardware/pio.h"
#include "pico/bootrom.h"

#define BOTAO_A 5 //pino saida botao a
#define BOTAO_B 6 //pino saida botao b
#define LED_PIN_GREEN 11 //pino saida led verde
#define LED_PIN_BLUE 12 //pino saida led azul
#define LED_PIN_RED 13  //pino saida led vermelho
#define NUM_PIXELS 25 // número de leds na matriz
#define matriz 7    //matriz de leds
#define I2C_SDA 14 //pino SDA
#define I2C_SCL 15 //pino SCL
#define SW_PIN 22 //pino do botão SW JOYSTICK
#define VRX_PIN 26   //pino do eixo X do joystick
#define VRY_PIN 27   //pino do eixo Y do joystick
#define I2C_PORT i2c1 //porta I2C
#define display_address 0x3C //endereço do display
#define gpio_buzzer 21 //pino buzzer A
#define microfone 28 //pino do microfone

// rotina para definição da intensidade de cores do led
uint matrix_rgb(float r, float g, float b){
  unsigned char R, G, B;
  R = r * 255;
  G = g * 255;
  B = b * 255;
  return (G << 24) | (R << 16) | (B << 8);
}

// Função para converter a posição do matriz para uma posição do vetor.
int getIndex(int x, int y){
  // Se a linha for par (0, 2, 4), percorremos da esquerda para a direita.
  // Se a linha for ímpar (1, 3), percorremos da direita para a esquerda.
  if (y % 2 == 0){
    return 24 - (y * 5 + x); // Linha par (esquerda para direita).
  }else{
    return 24 - (y * 5 + (4 - x)); // Linha ímpar (direita para esquerda).
  }
}

// Funcao para desenhar a matriz
void desenho_pio(double *desenho, uint32_t valor_led, PIO pio, uint sm, double r, double g, double b){
  for (int16_t i = 0; i < NUM_PIXELS; i++){
    valor_led = matrix_rgb(desenho[i] * r, desenho[i] * g, desenho[i] * b);
    pio_sm_put_blocking(pio, sm, valor_led);
  };
}

void acionar_buzzer(int interval)
{
  gpio_set_function(gpio_buzzer, GPIO_FUNC_PWM);      // Configura pino como saída PWM
    uint slice_num = pwm_gpio_to_slice_num(gpio_buzzer); // Obter o slice do PWM

    pwm_set_clkdiv(slice_num, 125.0);                  
    pwm_set_wrap(slice_num, 500);                      
    pwm_set_gpio_level(gpio_buzzer, 150);              
    pwm_set_enabled(slice_num, true);                  // Ativar o PWM

    sleep_ms(interval);                                    // Manter o som pelo intervalo

    pwm_set_enabled(slice_num, false);                 // Desativar o PWM  
}


void funcionamento_on() {
gpio_put(LED_PIN_GREEN, true);
sleep_ms(1000);
gpio_put(LED_PIN_GREEN,false);
sleep_ms(2000);
}

double apagar_leds[25] = {0.0, 0.0, 0.0, 0.0, 0.0, // Apagar LEDs da matriz
    0.0, 0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0, 0.0};

double acender_leds[25] = {1.0, 1.0, 1.0, 1.0, 1.0, // Acender LEDs da matriz
    1.0, 1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, 1.0, 1.0};    



int main(){
    stdio_init_all();
    gpio_init(LED_PIN_GREEN);
    gpio_set_dir(LED_PIN_GREEN, GPIO_OUT);
    
    while (true){
        funcionamento_on();
    }
}
