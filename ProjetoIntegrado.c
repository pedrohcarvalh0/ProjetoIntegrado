#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"

// Definições dos pinos
#define VRX 26                  // Pino X do joystick
#define VRY 27                  // Pino Y do joystick
#define BTN_A_PIN 5             // Botão A (Limpar)
#define BTN_B_PIN 6             // Botão B (Pintar)
#define BUZZER_PIN 21           // Buzzer
#define LED_MATRIX_PIN 7        // Matriz de LEDs WS2812
#define LED_RED_PIN   11        // LED RGB - Vermelho
#define LED_GREEN_PIN 12        // LED RGB - Verde
#define LED_BLUE_PIN  13        // LED RGB - Azul

// Configurações I2C para o display SSD1306
#define I2C_PORT           i2c1
#define I2C_SDA            14
#define I2C_SCL            15
#define SSD1306_ADDR       0x3C

// Canais do ADC
#define ADC_CHANNEL_0      0
#define ADC_CHANNEL_1      1

// Dimensões do display
#define DISPLAY_WIDTH      128
#define DISPLAY_HEIGHT     64

// Configurações do cursor e arte pixelada
#define CURSOR_SIZE        8
#define GRID_WIDTH (DISPLAY_WIDTH / CURSOR_SIZE)
#define GRID_HEIGHT (DISPLAY_HEIGHT / CURSOR_SIZE)
#define NUM_PIXELS 25      // Matriz de LEDs 5x5
#define MAX_PIXELS (GRID_WIDTH * GRID_HEIGHT)  // Número máximo de pixels na grade

// Tempo de debounce para os botões
#define DEBOUNCE_DELAY_US  300000

// Configurações PWM para o buzzer
#define BUZZER_FREQ        440     // 440Hz (nota A4)
#define PWM_WRAP           125000  // Para 1kHz @ 125MHz de clock do sistema

// Variáveis globais
ssd1306_t display;
uint8_t cursor_x = GRID_WIDTH / 2;
uint8_t cursor_y = GRID_HEIGHT / 2;
bool pixel_grid[GRID_WIDTH][GRID_HEIGHT] = {false};
uint16_t pixel_count = 0;
volatile bool btn_a_pressed = false;
volatile bool btn_b_pressed = false;
volatile uint64_t last_btn_a_time = 0;
volatile uint64_t last_btn_b_time = 0;

// Quadros de animação para limpar a tela (padrão X)
bool x_animation[3][NUM_PIXELS] = {
    { 1,0,0,0,1, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 1,0,0,0,1 },
    { 1,0,0,0,1, 0,1,0,1,0, 0,0,0,0,0, 0,1,0,1,0, 1,0,0,0,1 },
    { 1,0,0,0,1, 0,1,0,1,0, 0,0,1,0,0, 0,1,0,1,0, 1,0,0,0,1 }
};

// Protótipos das funções
void setup_gpio();
void setup_adc();
void setup_i2c();
void setup_pwm_buzzer();
void setup_ws2812();
void read_joystick(int16_t *x_value, int16_t *y_value);
void update_cursor_position(int16_t joy_x, int16_t joy_y);
void draw_cursor();
void draw_pixel_grid();
void paint_pixel();
void clear_canvas();
void update_rgb_led_by_pixel_count();
void play_buzzer(uint16_t frequency, uint16_t duration_ms);
void print_grid_to_uart();
void play_clear_animation();
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);
static inline void put_pixel(uint32_t pixel_grb);

// Função para interrupção dos botões
void gpio_callback(uint gpio, uint32_t events) {
    uint64_t current_time = time_us_64();
    
    if (gpio == BTN_A_PIN) {
        if (current_time - last_btn_a_time > DEBOUNCE_DELAY_US) {
            btn_a_pressed = true;
            last_btn_a_time = current_time;
        }
    } else if (gpio == BTN_B_PIN) {
        if (current_time - last_btn_b_time > DEBOUNCE_DELAY_US) {
            btn_b_pressed = true;
            last_btn_b_time = current_time;
        }
    }
}

// Inicializa os pinos GPIO
void setup_gpio() {
    // Pinos do LED RGB
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_put(LED_RED_PIN, 1);  // Iniciar com LED vermelho ligado (branco)
    
    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, 1);  // Iniciar com LED verde ligado (branco)
    
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    gpio_put(LED_BLUE_PIN, 1);  // Iniciar com LED azul ligado (branco)
    
    // Pinos dos botões com resistores de pull-up
    gpio_init(BTN_A_PIN);
    gpio_set_dir(BTN_A_PIN, GPIO_IN);
    gpio_pull_up(BTN_A_PIN);
    
    gpio_init(BTN_B_PIN);
    gpio_set_dir(BTN_B_PIN, GPIO_IN);
    gpio_pull_up(BTN_B_PIN);
    
    // Configura interrupções para os botões
    gpio_set_irq_enabled_with_callback(BTN_A_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled(BTN_B_PIN, GPIO_IRQ_EDGE_FALL, true);
}

// Inicializa o ADC para o joystick
void setup_adc() {
    adc_init();
    adc_gpio_init(VRX);
    adc_gpio_init(VRY);
}

// Inicializa o I2C para o display SSD1306
void setup_i2c() {
    i2c_init(I2C_PORT, 400 * 1000);  // 400 kHz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
}

// Inicializa o PWM para o buzzer
void setup_pwm_buzzer() {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(BUZZER_PIN, 0);
}

// Inicializa a matriz de LEDs WS2812
void setup_ws2812() {
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, LED_MATRIX_PIN, 800000, false);
    
    // Desliga todos os LEDs inicialmente
    for (int i = 0; i < NUM_PIXELS; i++) {
        put_pixel(0);
    }
}

// Lê os valores do joystick
void read_joystick(int16_t *x_value, int16_t *y_value) {
    // Lê o eixo X (ADC0)
    adc_select_input(ADC_CHANNEL_1);
    sleep_us(2);
    uint16_t raw_x = adc_read();
    
    // Lê o eixo Y (ADC1)
    adc_select_input(ADC_CHANNEL_0);
    sleep_us(2);
    uint16_t raw_y = adc_read();
    
    // Converte para valores com sinal centralizados em 0
    // A faixa do ADC é 0-4095, então 2048 é o centro
    *x_value = raw_x - 2048;
    *y_value = 2048 - raw_y;
}

// Atualiza a posição do cursor baseado no input do joystick
void update_cursor_position(int16_t joy_x, int16_t joy_y) {
    if (abs(joy_x) > 500) {
        if (joy_x > 0 && cursor_x < GRID_WIDTH - 1) {
            cursor_x++;
        } else if (joy_x < 0 && cursor_x > 0) {
            cursor_x--;
        }
    }
    
    if (abs(joy_y) > 500) {
        if (joy_y > 0 && cursor_y < GRID_HEIGHT - 1) {
            cursor_y++;
        } else if (joy_y < 0 && cursor_y > 0) {
            cursor_y--;
        }
    }
}

// Desenha o cursor no display
void draw_cursor() {
    // Desenha um quadrado 8x8 na posição do cursor
    ssd1306_rect(&display,
                 cursor_y * CURSOR_SIZE,
                 cursor_x * CURSOR_SIZE,
                 CURSOR_SIZE,
                 CURSOR_SIZE,
                 true,
                 pixel_grid[cursor_x][cursor_y] ? false : true);
}

// Desenha a grade de pixels no display
void draw_pixel_grid() {
    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            if (pixel_grid[x][y]) {
                // Preenche o pixel se estiver pintado
                ssd1306_rect(&display,
                             y * CURSOR_SIZE,
                             x * CURSOR_SIZE,
                             CURSOR_SIZE,
                             CURSOR_SIZE,
                             true,
                             true);
            }
        }
    }
}

// Pinta um pixel na posição atual do cursor
void paint_pixel() {
    // Alterna o estado do pixel
    pixel_grid[cursor_x][cursor_y] = !pixel_grid[cursor_x][cursor_y];
    
    // Atualiza a contagem de pixels
    if (pixel_grid[cursor_x][cursor_y]) {
        pixel_count++;
    } else {
        pixel_count--;
    }
}

// Limpa todos os pixels da tela e reposiciona o cursor
void clear_canvas() {
    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            pixel_grid[x][y] = false;
        }
    }
    pixel_count = 0;
    
    // Reposiciona o cursor no centro da grade
    cursor_x = GRID_WIDTH / 2;
    cursor_y = GRID_HEIGHT / 2;
}

// Atualiza a cor do LED RGB baseado na contagem de pixels
void update_rgb_led_by_pixel_count() {
    // Calcula a porcentagem de pixels pintados
    float percentage = (float)pixel_count / MAX_PIXELS;
    
    // Se todos os pixels estiverem pintados (100%), LED fica branco
    if (percentage >= 1.0) {
        gpio_put(LED_RED_PIN, 1);
        gpio_put(LED_GREEN_PIN, 1);
        gpio_put(LED_BLUE_PIN, 1);
    }
    // Verde para primeiros 33%
    else if (percentage < 0.33) {
        gpio_put(LED_RED_PIN, 0);
        gpio_put(LED_GREEN_PIN, 1);
        gpio_put(LED_BLUE_PIN, 0);
    }
    // Azul para 33% a 66%
    else if (percentage < 0.66) {
        gpio_put(LED_RED_PIN, 0);
        gpio_put(LED_GREEN_PIN, 0);
        gpio_put(LED_BLUE_PIN, 1);
    }
    // Vermelho para 66% a 95%
    else if (percentage < 0.95) {
        gpio_put(LED_RED_PIN, 1);
        gpio_put(LED_GREEN_PIN, 0);
        gpio_put(LED_BLUE_PIN, 0);
    }
    // Branco para 95% a 99%
    else {
        gpio_put(LED_RED_PIN, 1);
        gpio_put(LED_GREEN_PIN, 1);
        gpio_put(LED_BLUE_PIN, 1);
    }
}

// Executa a animação de limpeza na matriz de LEDs
void play_clear_animation() {
    // Toca um tom mais grave para limpar
    play_buzzer(200, 500);
    
    // Mostra os quadros da animação em X
    for (int f = 0; f < 3; f++) {
        // Exibe o quadro atual com cor vermelha
        for (int i = 0; i < NUM_PIXELS; i++) {
            put_pixel(x_animation[f][i] ? urgb_u32(10, 0, 0) : 0);
        }
        sleep_ms(150);
    }
    
    // Limpa a matriz de LEDs após a animação
    for (int i = 0; i < NUM_PIXELS; i++) {
        put_pixel(0);
    }
}

// Toca um tom no buzzer
void play_buzzer(uint16_t frequency, uint16_t duration_ms) {
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    
    if (frequency == 0) {
        pwm_set_gpio_level(BUZZER_PIN, 0);
        sleep_ms(duration_ms);
        return;
    }
    
    float divider = 20.0;
    pwm_set_clkdiv(slice_num, divider);
   
    uint16_t wrap = (125000000 / (frequency * divider)) - 1;
    pwm_set_wrap(slice_num, wrap);
    
    // Configura o duty cycle em 50%
    pwm_set_gpio_level(BUZZER_PIN, wrap / 2);
    sleep_ms(duration_ms);
    pwm_set_gpio_level(BUZZER_PIN, 0);
}

// Imprime a grade de pixels atual no mintor serial via UART
void print_grid_to_uart() {
    printf("\n\nGrade de Pixels (%d pixels pintados):\n", pixel_count);
    
    // Imprime a borda superior
    printf("+");
    for (int x = 0; x < GRID_WIDTH; x++) {
        printf("-");
    }
    printf("+\n");
    
    // Imprime a grade
    for (int y = 0; y < GRID_HEIGHT; y++) {
        printf("|");
        for (int x = 0; x < GRID_WIDTH; x++) {
            printf("%c", pixel_grid[x][y] ? '1' : '0');
        }
        printf("|\n");
    }
    
    // Imprime a borda inferior
    printf("+");
    for (int x = 0; x < GRID_WIDTH; x++) {
        printf("-");
    }
    printf("+\n");
}

// Converte valores RGB para formato GRB usado pelo WS2812
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

// Envia um pixel para a matriz de LEDs WS2812
static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

int main() {
    // Inicializa a stdio
    stdio_init_all();
    
    // Configura o hardware
    setup_gpio();
    setup_adc();
    setup_i2c();
    setup_pwm_buzzer();
    setup_ws2812();
    
    // Inicializa o display SSD1306
    ssd1306_init(&display, DISPLAY_WIDTH, DISPLAY_HEIGHT, false, SSD1306_ADDR, I2C_PORT);
    ssd1306_config(&display);
    ssd1306_fill(&display, false);
    ssd1306_draw_string(&display, "Pixel Art", 32, 28);
    ssd1306_send_data(&display);
    
    // Configura a cor inicial do LED RGB para branco
    gpio_put(LED_RED_PIN, 1);
    gpio_put(LED_GREEN_PIN, 1);
    gpio_put(LED_BLUE_PIN, 1);
    
    // Toca o som de inicialização
    play_buzzer(880, 100);
    sleep_ms(100);
    play_buzzer(1760, 100);
    
    // Aguarda um momento antes de começar
    sleep_ms(1000);
    
    // Limpa o display
    ssd1306_fill(&display, false);
    ssd1306_send_data(&display);
    
    // Loop principal
    int16_t joy_x, joy_y;
    while (true) {
        // Lê os valores do joystick
        read_joystick(&joy_x, &joy_y);
        
        // Atualiza a posição do cursor baseado no joystick
        update_cursor_position(joy_x, joy_y);
        
        // Trata os pressionamentos dos botões
        if (btn_b_pressed) {
            btn_b_pressed = false;
            paint_pixel();
            // Toca um beep ao desenhar
            play_buzzer(BUZZER_FREQ, 50);
            // Atualiza o LED RGB baseado na contagem de pixels
            update_rgb_led_by_pixel_count();
            print_grid_to_uart();
        }
        
        if (btn_a_pressed) {
            btn_a_pressed = false;
            clear_canvas();
            // Executa animação e som ao limpar
            play_clear_animation();
            // Reseta o LED RGB para branco após limpar
            gpio_put(LED_RED_PIN, 1);
            gpio_put(LED_GREEN_PIN, 1);
            gpio_put(LED_BLUE_PIN, 1);
            print_grid_to_uart();
        }
        
        // Desenha a grade de pixels e o cursor
        ssd1306_fill(&display, false);
        draw_pixel_grid();
        draw_cursor();
        ssd1306_send_data(&display);
        
        // Pequeno delay para evitar atualizações muito rápidas
        sleep_ms(50);
    }
    
    return 0;
}