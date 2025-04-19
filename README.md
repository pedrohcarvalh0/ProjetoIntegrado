# 🎨 BitPaint: Arte em Pixels com Joystick

Projeto prático desenvolvido com o microcontrolador **RP2040** na placa **BitDogLab**, que transforma a placa em uma mini plataforma de pintura estilo pixel art. O projeto foi criado com o objetivo de consolidar os conhecimentos sobre periféricos, interrupções e controle de hardware.

---

## 📌 Funcionalidades

- **Movimentação do cursor** utilizando joystick analógico (leitura via ADC)
- **Pintura e apagamento de pixels** com botão físico
- **Limpeza total da tela** com botão dedicado
- **Display OLED** com grade de pixels e cursor
- **LED RGB** varia sua cor conforme a quantidade de pixels pintados
- **Matriz de LEDs WS2812** exibe animação ao limpar
- **Buzzer** emite sons ao pintar ou apagar
- **Saída UART** exibe a grade de pixels em tempo real

---

## 🧠 Objetivo do Projeto

Permitir que o usuário interaja com um ambiente de arte digital pixelada, controlando um cursor via joystick e manipulando pixels individualmente. O projeto visa aplicar os conceitos estudados sobre:

- ADC e movimentação analógica
- PWM para controle de buzzer
- I2C para comunicação com o display OLED
- PIO para controle da matriz de LEDs WS2812
- Interrupções e debounce em botões
- Comunicação UART

---

## 🛠️ Componentes Utilizados

| Componente         | Função                                      |
|--------------------|---------------------------------------------|
| RP2040 (BitDogLab) | Microcontrolador principal                  |
| Joystick analógico | Movimentação do cursor                      |
| Display OLED SSD1306 | Exibição da arte em pixel                 |
| Botões A e B       | Pintar/apagar e limpar tela                 |
| LED RGB            | Indicação visual do progresso               |
| Buzzer             | Sinalização sonora                          |
| WS2812 (5x5)       | Animação ao limpar                          |
| UART               | Visualização da grade via terminal serial   |

---

## 🧩 Organização do Código

- `setup_gpio()` – Inicializa botões e LEDs
- `setup_adc()` – Configura ADC para joystick
- `setup_i2c()` – Comunicação com o display OLED
- `setup_pwm_buzzer()` – PWM para o buzzer
- `setup_ws2812()` – Inicializa PIO para WS2812
- `gpio_callback()` – Interrupções com debounce
- `draw_cursor()` / `draw_pixel_grid()` – Interface gráfica
- `paint_pixel()` / `clear_canvas()` – Lógica de pintura
- `update_rgb_led_by_pixel_count()` – Controle de cor RGB
- `print_grid_to_uart()` – Impressão da grade no terminal
- `play_clear_animation()` – Animação da matriz WS2812

---
