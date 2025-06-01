# ATIVU2C2_Protocolos_Media_e_Longa

## **Projeto: Ponto de Acesso Wi-Fi com Controle de LED e Sensor de Temperatura no Raspberry Pi Pico W**

### 🌐 **Visão Geral**
Este projeto transforma o **Raspberry Pi Pico W** em um **ponto de acesso (Access Point) Wi-Fi**. Uma vez conectado à rede Wi-Fi criada pelo Pico W, os usuários podem acessar uma **página web embarcada** para:
- Controlar um **LED físico** conectado ao **GPIO 13** (ligar/desligar).
- Visualizar a **temperatura interna** do microcontrolador **RP2040**.

O projeto também implementa:
- 🖥️ **Servidor DHCP** para atribuição automática de IPs.
- 🔍 **Servidor DNS**.
- 📡 **Mensagens de depuração** enviadas para o terminal USB.

---

## 🔧 **Funcionalidades Principais**

| Funcionalidade | Descrição |
|--------------|-----------|
| **📶 Ponto de Acesso Wi-Fi** | Cria uma rede Wi-Fi (SSID: `"PicoW_Info"`, Senha: `"picopass"`). |
| **🔄 Servidor DHCP** | Atribui endereços IP automaticamente aos clientes conectados. |
| **🔗 Servidor DNS** | Responde a consultas DNS, redirecionando para o próprio Pico W. |
| **🌍 Servidor HTTP Embarcado** | Serve uma página web na porta **80**. |
| **💡 Controle de LED Remoto** | Liga/desliga um LED no **GPIO 13** via interface web. |
| **🌡️ Monitoramento de Temperatura** | Exibe a temperatura interna do **RP2040** na web. |
| **🔄 Interface Web Dinâmica** | Gera HTML dinamicamente para refletir o estado do LED e temperatura. |
| **📡 Depuração via USB** | Envia logs para o terminal serial USB. |

---

## 📋 **Verificação das Instruções Implementadas**

### **1️⃣ Inicialização do Projeto com SDK do Raspberry Pi Pico**
- **📂 Arquivo:** `CMakeLists.txt`
- **🔧 Como foi implementado:**
  - Inicialização do SDK com `include(pico_sdk_import.cmake)` e `pico_sdk_init()`.
  - Bibliotecas vinculadas via `target_link_libraries`:
    - `pico_cyw43_arch_lwip_threadsafe_background`
    - `pico_stdlib`
    - `hardware_adc`
  - Habilitado `pico_stdio_usb` com:
    ```cmake
    pico_enable_stdio_usb(picow_access_point_background 1)
    pico_enable_stdio_uart(picow_access_point_background 0)
    ```

---

### **2️⃣ Leitura da Temperatura Interna via ADC**
- **📂 Arquivo:** `picow_access_point.c`
- **🔧 Como foi implementado:**
  - Inicialização do ADC com `adc_init()`.
  - Sensor de temperatura habilitado com `adc_set_temp_sensor_enabled(true)`.
  - Função `read_onboard_temperature()`:
    - Seleciona canal 4 (`adc_select_input(4)`).
    - Lê valor com `adc_read()`.
    - Converte para °C.

---

### **3️⃣ Configuração do Pico W como Access Point**
- **📂 Arquivo:** `picow_access_point.c`
- **🔧 Como foi implementado:**
  - SSID: `"PicoW_Info"`, Senha: `"picopass"`.
  - Ativado modo AP com:
    ```c
    cyw43_arch_enable_ap_mode(ap_name, password, CYW43_AUTH_WPA2_AES_PSK);
    ```
  - IP configurado como `192.168.4.1` (via `CMakeLists.txt` e código C).

---

### **4️⃣ Página HTML Embarcada**
- **📂 Arquivo:** `picow_access_point.c`
- **🔧 Como foi implementado:**
  - Macro `#define INFO_PAGE_BODY "<html>...</html>"` define a estrutura HTML.
  - Função `server_content_handler()` usa `snprintf()` para:
    - Inserir estado do LED (**LIGADO/DESLIGADO**).
    - Exibir temperatura formatada (`%.2f °C`).

---

### **5️⃣ Controle do LED e Leitura do ADC**
- **📂 Arquivo:** `picow_access_point.c`
- **🔧 Como foi implementado:**
  - LED no **GPIO 13** (`#define LED_PIN_EXTERNAL 13`).
  - Controle via `gpio_put(LED_PIN_EXTERNAL, true/false)`.
  - Leitura do sensor com `adc_read()`.

---

### **6️⃣ Depuração via Terminal USB**
- **📂 Arquivo:** `picow_access_point.c` e `CMakeLists.txt`
- **🔧 Como foi implementado:**
  - `#define DEBUG_printf printf` para logs.
  - `stdio_init_all()` no início do `main()`.
  - `sleep_ms(2000)` para estabilizar conexão USB.
  - Saída USB habilitada via `pico_enable_stdio_usb(NOMEDOEXECUTAVEL 1)`.

---

## 🎯 **Conclusão**
Este projeto demonstra a capacidade do **Raspberry Pi Pico W** de atuar como um **ponto de acesso Wi-Fi**, oferecendo controle remoto de dispositivos e monitoramento de sensores via interface web. Tudo isso com **depuração em tempo real** via USB.

## Propósito Educacional

Este projeto foi desenvolvido com fins estritamente educacionais e aprendizado durante a residência em sistemas embarcados pelo EmbarcaTech.
