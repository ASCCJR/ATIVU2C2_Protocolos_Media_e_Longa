# ATIVU2C2_Protocolos_Media_e_Longa

---

# Projeto: Ponto de Acesso Wi-Fi com Controle de LED, Sensor de Temperatura e Logs Sincronizados no Raspberry Pi Pico W

## Visão Geral
Este projeto transforma o Raspberry Pi Pico W em um ponto de acesso (Access Point) Wi-Fi. Uma vez conectado à rede Wi-Fi criada pelo Pico W, os usuários podem acessar uma página web embarcada para:
- Controlar um LED físico conectado ao GPIO 13 (ligar/desligar)
- Visualizar a temperatura interna do microcontrolador RP2040

O projeto também implementa:
- Servidor DHCP para atribuição automática de IPs
- Servidor DNS
- Mensagens de depuração enviadas para o terminal USB, sincronizadas com as interações na página HTML

## Funcionalidades Principais
- **Ponto de Acesso Wi-Fi**: Cria uma rede Wi-Fi própria (SSID: "PicoW_Info", Senha: "picopass")
- **Servidor DHCP**: Atribui endereços IP automaticamente aos clientes conectados
- **Servidor DNS**: Responde a consultas DNS, geralmente redirecionando para o próprio Pico W
- **Servidor HTTP Embarcado**: Serve uma página web na porta 80, acessível através do caminho `/info`
- **Controle de LED Remoto**: Permite ligar e desligar um LED conectado ao GPIO 13 através da interface web
- **Monitoramento de Temperatura**: Exibe a temperatura interna do chip RP2040 na interface web
- **Interface Web Dinâmica**: A página HTML é gerada dinamicamente para refletir o estado atual do LED e da temperatura
- **Depuração via USB Sincronizada**: Envia mensagens de log para o terminal serial USB que refletem as ações de controle do LED e a leitura da temperatura no momento em que a página web é carregada/atualizada

## Requisitos de Hardware
- Raspberry Pi Pico W
- Um LED físico conectado ao GPIO 13 (com resistor apropriado, por exemplo, 220Ω se for um LED vermelho padrão na placa bitdoglab)

### Arquivos do Projeto
- `picow_access_point.c` (código principal da aplicação)
- `dhcpserver/dhcpserver.c` e `dhcpserver/dhcpserver.h`
- `dnsserver/dnsserver.c` e `dnsserver/dnsserver.h`
- `lwipopts.h` (para configuração do LwIP)
- `CMakeLists.txt` (arquivo de configuração do build)

## Como Usar

### Conecte-se ao Access Point
1. Após o Pico W reiniciar, procure por redes Wi-Fi no seu dispositivo (computador, smartphone)
2. Conecte-se à rede com SSID: `PicoW_Info` e senha: `picopass`

### Acesse a Interface Web
1. Abra um navegador web
2. Navegue para `http://192.168.4.1/info` (O IP padrão do AP é 192.168.4.1)

### Interaja
- Você verá o estado atual do LED (GPIO 13) e a temperatura interna do RP2040
- Clique nos links/botões para ligar ou desligar o LED. A página será atualizada mostrando o novo estado

### Depuração
1. Conecte o Pico W ao seu computador via USB
2. Abra um monitor serial (ex: PuTTY, minicom, Thonny, Arduino IDE Serial Monitor) conectado à porta serial USB do Pico W (taxa de 115200 bps)
3. Você verá mensagens de depuração sobre o status do servidor, conexões de clientes, e o estado do LED e a temperatura no momento em que a página `/info` é carregada ou uma ação de controle do LED é realizada

## Verificação dos Requisitos Implementados

### Requisito 1
**Descrição**: Inicialize o projeto utilizando o SDK do Raspberry Pi Pico e configure o CMakeLists.txt com as bibliotecas necessárias: hardware_adc, pico_stdio_usb, pico_stdlib e pico_cyw43_arch_lwip_threadsafe_background.

**Onde**: Arquivo CMakeLists.txt

**Como**:
- O CMakeLists.txt inclui `pico_sdk_import.cmake` e chama `pico_sdk_init()`
- Para o alvo `picow_access_point_background`, as bibliotecas `pico_cyw43_arch_lwip_threadsafe_background`, `pico_stdlib`, e `hardware_adc` são vinculadas via `target_link_libraries`
- A funcionalidade `pico_stdio_usb` é habilitada com `pico_enable_stdio_usb(picow_access_point_background 1)`

### Requisito 2
**Descrição**: No código, implemente a leitura da temperatura interna do microcontrolador usando o canal 4 do ADC.

**Onde**: Arquivo picow_access_point.c

**Como**:
- No `main()`, `adc_init()` e `adc_set_temp_sensor_enabled(true)` são chamados
- A função `read_onboard_temperature()` seleciona o canal 4 (`adc_select_input(4)`), lê o valor com `adc_read()`, e o converte para Celsius

### Requisito 3
**Descrição**: Configure o Pico W como Access Point com SSID 'PicoW_Info' e senha 'picopass'.

**Onde**: Arquivo picow_access_point.c

**Como**:
- No `main()`, `const char *ap_name = "PicoW_Info";` e `const char *password = "picopass";` são definidos
- `cyw43_arch_enable_ap_mode(ap_name, password, CYW43_AUTH_WPA2_AES_PSK);` ativa o modo AP

### Requisito 4
**Descrição**: Crie uma página HTML embarcada com informações sobre o estado do LED e a temperatura.

**Onde**: Arquivo picow_access_point.c

**Como**:
- A macro `#define INFO_PAGE_BODY "<html>...</html>"` define a estrutura HTML
- A função `server_content_handler()` usa `snprintf()` para popular dinamicamente esta string com o estado do LED e a temperatura atual antes de enviá-la ao cliente

### Requisito 5
**Descrição**: Utilize a função gpio_put() para controlar um LED no GPIO 13 e adc_read() para ler o sensor.

**Onde**: Arquivo picow_access_point.c

**Como**:
- `#define LED_PIN_EXTERNAL 13` define o pino
- No `main()`, `gpio_init(LED_PIN_EXTERNAL)` e `gpio_set_dir(LED_PIN_EXTERNAL, GPIO_OUT)` configuram o pino
- Em `server_content_handler()`, `gpio_put(LED_PIN_EXTERNAL, ...)` é usado para alterar o estado do LED com base nos parâmetros da URL
- A função `read_onboard_temperature()` usa `adc_read()` para obter o valor do sensor

### Requisito 6
**Descrição**: Exiba mensagens de depuração no terminal USB com printf() para confirmar conexões e leituras.

**Onde**: Arquivo picow_access_point.c e CMakeLists.txt

**Como**:
- `#define DEBUG_printf printf` é usado no código C
- `stdio_init_all();` é chamado no `main()`, e um `sleep_ms(2000);` é adicionado para estabilização da conexão serial USB
- O CMakeLists.txt configura `pico_enable_stdio_usb(NOMEDOEXECUTAVEL 1)`

**Especificamente para a sincronização com a página HTML**:
- Em `server_content_handler()`, quando o estado do LED é alterado via web, uma mensagem `[LED_CTRL] LED no GPIOX: ESTADO (via Web)` é impressa
- Em `server_content_handler()`, toda vez que a página `/info` é gerada, o estado atual do LED (`[LED_STATUS] Estado do LED para página HTML: ESTADO`) e a temperatura lida (`[TEMP_READ] Temperatura para página HTML: XX.YY C`) são impressos no monitor serial

### Requisito 7
**Descrição**: Compile o projeto e grave no Pico W.

**Onde**: Ambiente de desenvolvimento do usuário

**Como**: Este é um passo manual do usuário. O CMakeLists.txt está configurado para gerar os arquivos .uf2 necessários (via `pico_add_extra_outputs`) para serem gravados no Pico W no modo bootloader

--- 

## Propósito Educacional

Este projeto foi desenvolvido com fins estritamente educacionais e aprendizado durante a residência em sistemas embarcados pelo EmbarcaTech.
