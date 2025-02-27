#include <stdio.h>
#include <stdint.h>
#include <string.h>
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
#include "esp.pio.h"

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

// Variáveis globais
volatile bool alarme_ligado = false;

int cor_alarme = 0; // 0: vermelho, 1: verde, 2: azul
int som_alarme = 0; // 0: agudo, 1: grave
uint actual_time = 0;
ssd1306_t display;
PIO pio = pio0;
uint sm = 0;
uint valor_led;

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

void acionar_buzzer(int interval){
  gpio_set_function(gpio_buzzer, GPIO_FUNC_PWM);      // Configura pino como saída PWM
    uint slice_num = pwm_gpio_to_slice_num(gpio_buzzer); // Obter o slice do PWM

    pwm_set_clkdiv(slice_num, 125.0);                  
    pwm_set_wrap(slice_num, 500);                      
    pwm_set_gpio_level(gpio_buzzer, 150);              
    pwm_set_enabled(slice_num, true);                  // Ativar o PWM

    sleep_ms(interval);                                    // Manter o som pelo intervalo

    pwm_set_enabled(slice_num, false);                 // Desativar o PWM  
}

// Função para ler o nível do microfone
int ler_microfone() {
  adc_select_input(2); // Pino do microfone (ADC2)
  return adc_read();// Lê o ADC
}

// Função para exibir mensagem no display
void exibir_mensagem(const char *mensagem) {
  ssd1306_fill(&display, false);
  ssd1306_draw_string(&display, 0, 0, *mensagem);
  ssd1306_send_data(&display);
}


// Função de callback para os botões (interrupção)
void callback_abtn(uint gpio, uint32_t events) {
  uint time = to_ms_since_boot(get_absolute_time());
  if (time - actual_time > 300) { //DEBOUNCE: aplicado em todos os botões
      actual_time = time; // Atualiza o tempo da última ação
      if (gpio == BOTAO_A){
          
          alarme_ligado = !alarme_ligado;
          printf("%s\n", alarme_ligado ? "Alarme Habilitado" : "Alarme Desabilitado");
      }
  }
  
}

int ssd1306_get_string_width(ssd1306_t *display, const char *str) {
  int width = 8;
  for (size_t i = 0; i < strlen(str); i++) {
      // Assuming each character is 8 pixels wide.  Adjust if your font is different.
      width += 8;  
  }
  return width;
}

// Estruturas dos menus
const char *menu_principal[] = {
  "COR DO ALARME",
  "SOM DO AlARME"
};

const char *menu_cores[] = {
  "VERMELHO",
  "VERDE",
  "AZUL"
};

const char *menu_sons[] = {
  "AGUDO",
  "GRAVE"
};

int menu_selecionado = 0;
int submenu_selecionado = 0;
enum {MENU_PRINCIPAL, MENU_COR, MENU_SOM} estado_menu = MENU_PRINCIPAL;

void exibir_menu() {
  ssd1306_fill(&display, false);
  const char **menu_atual; // Ponteiro para o array de strings do menu atual
  int tamanho_menu;

  switch (estado_menu) {
      case MENU_PRINCIPAL:
          menu_atual = menu_principal;
          tamanho_menu = sizeof(menu_principal) / sizeof(menu_principal[0]);
          break;
      case MENU_COR:
          menu_atual = menu_cores;
          tamanho_menu = sizeof(menu_cores) / sizeof(menu_cores[0]);
          break;
      case MENU_SOM:
          menu_atual = menu_sons;
          tamanho_menu = sizeof(menu_sons) / sizeof(menu_sons[0]);
          break;
  }

  char linha[20];
  int y_offset = 5; // Ajuste a posição vertical inicial
  int x_offset = (128 - (strlen(linha) * 54)) / 2; // Centraliza a string
  for (int i = 0; i < tamanho_menu; i++) {
    if (i == (estado_menu == MENU_PRINCIPAL ? menu_selecionado : submenu_selecionado)) {
      sprintf(linha, "%s", menu_atual[i]);
      ssd1306_hline(&display, 0, 10, y_offset, 1); // Desenha o indicador de seleção
      ssd1306_draw_string(&display, linha, x_offset, y_offset); // Desenha o texto do menu
    } else {
      sprintf(linha, " %s", menu_atual[i]);
      ssd1306_draw_string(&display, linha, x_offset, y_offset); // Desenha o texto do menu
    }
    x_offset += 0;
    y_offset += 15;
    }

    ssd1306_send_data(&display);
}


void configurar_cor() {
  estado_menu = MENU_COR;
  submenu_selecionado = 0;
  exibir_menu();
}

void configurar_som() {
  estado_menu = MENU_SOM;
  submenu_selecionado = 0;
  exibir_menu();
}



void funcionamento_on() {
  ssd1306_fill(&display, false);
ssd1306_send_data(&display);
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
  PIO pio = pio0;
  bool frequenciaClock;
  uint16_t i;
  uint valor_led;
  float r = 0.0, b = 0.0, g = 0.0;
  const uint limiar_1 = 2000;     // Limiar para o LED vermelho
  const uint limiar_2 = 2400;    // Limiar para o LED azul

 // Inicialização
 stdio_init_all();
 //inicialização e configuração LED Verde
 gpio_init(LED_PIN_GREEN);
 gpio_set_dir(LED_PIN_GREEN, GPIO_OUT);
 gpio_put(LED_PIN_GREEN, false); 
 //inicialização e configuração LED Vermelho
 gpio_init(LED_PIN_RED);
 gpio_set_dir(LED_PIN_RED, GPIO_OUT);
 gpio_put(LED_PIN_RED, false); 
    //inicialização e configuração LED Azul
    gpio_init(LED_PIN_BLUE);
    gpio_set_dir(LED_PIN_BLUE, GPIO_OUT);
    gpio_put(LED_PIN_BLUE, false);
    //inicialização e configuração Botão A
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    //inicialização e configuração Botão B
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);
    //inicialização e configuração SW_PIN
    gpio_set_dir(SW_PIN, GPIO_IN);
    gpio_pull_up(SW_PIN); 
    adc_gpio_init(VRX_PIN); 
    adc_gpio_init(VRY_PIN); 
    gpio_init(SW_PIN);

    frequenciaClock = set_sys_clock_khz(128000, false); // frequência de clock
    gpio_init(matriz);
    gpio_set_dir(matriz, GPIO_OUT);

    
    //inicialização a comunição I2C
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    display.i2c_port = I2C_PORT;
    display.address = display_address;
    display.width = 128;
    display.height = 64;
    display.external_vcc = false;
    
    //Inicializa o display SSD1306
    ssd1306_init(&display, 128, 64, false, display_address, I2C_PORT);
    ssd1306_config(&display);
    ssd1306_send_data(&display);
    ssd1306_fill(&display, false);
    ssd1306_send_data(&display);
    
    adc_init();
    adc_gpio_init(microfone);

  
  exibir_menu();
  gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &callback_abtn);
  gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &callback_abtn);
  gpio_set_irq_enabled_with_callback(SW_PIN, GPIO_IRQ_EDGE_FALL, true, &callback_abtn);
    
  // configurações da PIO
  uint offset = pio_add_program(pio, &pio_matrix_program);
  uint sm = pio_claim_unused_sm(pio, true);
  pio_matrix_program_init(pio, sm, offset, matriz);
  
  const uint amostras_por_segundo = 8000; // Frequência de amostragem (8 kHz)
  uint64_t intervalo_us = 1000000 / amostras_por_segundo;
    
    while (true){
      if (!alarme_ligado && !gpio_get(BOTAO_B)) {
        printf("Selecionando opção");
        switch (estado_menu) {
            case MENU_PRINCIPAL:
                switch (menu_selecionado) {
                    case 0: // Cor do Alarme
                        estado_menu = MENU_COR;
                        break;
                    case 1: // Som do Alarme
                        estado_menu = MENU_SOM;
                        break;
                }
                break;
            case MENU_COR:
                // Lógica para selecionar a cor (ex: cor_alarme = submenu_selecionado;)
                cor_alarme = submenu_selecionado;
                estado_menu = MENU_PRINCIPAL; // Volta para o menu principal após selecionar
                break;
            case MENU_SOM:
                // Lógica para selecionar o som (ex: som_alarme = submenu_selecionado;)
                som_alarme = submenu_selecionado;
                estado_menu = MENU_PRINCIPAL; // Volta para o menu principal após selecionar
                break;
        }
        exibir_menu();
        sleep_ms(200); // Debounce
    }
    if (!alarme_ligado && !gpio_get(SW_PIN)) {
      printf("Retrocendendo");
      switch (estado_menu) {
          case MENU_COR:
          case MENU_SOM:
              estado_menu = MENU_PRINCIPAL;
              break;
      }
      exibir_menu();
      sleep_ms(200); // Debounce
  }
    // Controle via joystick (com alarme desligado)
    if (!alarme_ligado) {
      // Ler posição do joystick
      adc_select_input(0);
      int vrx = adc_read();
      uint16_t vrx_inverter = 4095 - vrx;


      // Navegação no menu principal
      if (estado_menu == MENU_PRINCIPAL) {
          if (vrx_inverter > 2400) { // Para baixo
              menu_selecionado = (menu_selecionado + 1) % (sizeof(menu_principal) / sizeof(menu_principal[0]));
          } else if (vrx_inverter < 1000) { // Para cima
              menu_selecionado = (menu_selecionado - 1 + sizeof(menu_principal) / sizeof(menu_principal[0])) % (sizeof(menu_principal) / sizeof(menu_principal[0]));
          }
          sleep_ms(300); // Debounce
      } else {
          // Navegação nos submenus (cores e sons)
          if (vrx_inverter> 2400) { // Para baixo
              submenu_selecionado = (submenu_selecionado + 1) % (estado_menu == MENU_COR ? 3 : 2); // Ajustar o tamanho do menu
          } else if (vrx_inverter < 1000) { // Para cima
              submenu_selecionado = (submenu_selecionado - 1 + (estado_menu == MENU_COR ? 3 : 2)) % (estado_menu == MENU_COR ? 3 : 2); // Ajustar o tamanho do menu
            }
            sleep_ms(300); // Debounce
      }
      exibir_menu();
  }

      if (alarme_ligado){
        funcionamento_on();

        if (alarme_ligado && ler_microfone() > 2000) { // Limiar de sensibilidade do microfone
          // Disparar alarme
          while (alarme_ligado){
            desenho_pio(acender_leds, valor_led, pio, sm, 1.0, 0.0, 0.0); // LEDs vermelhos
            acionar_buzzer(200);
            sleep_ms(200);
            desenho_pio(apagar_leds, valor_led, pio, sm, 0.0, 0.0, 0.0);
            sleep_ms(200);
          }
        }
      }
      
      
  
    }
  }
  