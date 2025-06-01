#  ATIVU2C2_Protocolos_Media_e_Longa 

---

## 📌 **Projeto**: Ponto de Acesso Wi-Fi com Controle de LED, Sensor de Temperatura e Logs Sincronizados no Raspberry Pi Pico W

### **Visão Geral**
Este projeto transforma o Raspberry Pi Pico W em um **ponto de acesso (Access Point) Wi-Fi**. Uma vez conectado à rede Wi-Fi criada pelo Pico W, os usuários podem:

- Controlar um LED físico conectado ao GPIO 13 (ligar/desligar)
-  Visualizar a temperatura interna do microcontrolador RP2040

### ✨ **Funcionalidades Principais**
| Funcionalidade | Descrição |
|---------------|-----------|
| **📶 Ponto de Acesso Wi-Fi** | Cria rede `PicoW_Info` (Senha: `picopass`) |
| **🖥️ Servidor DHCP** | Atribui IPs automaticamente |
| **🔗 Servidor DNS** | Responde consultas DNS |
| **🌐 Servidor HTTP** | Serve página web em `http://192.168.4.1/info` |
| **💡 Controle de LED** | Gerencia LED no GPIO 13 via web |
| **🔘 Monitoramento** | Exibe temperatura do RP2040 |
| **🔄 Interface Dinâmica** | HTML atualizado em tempo real |
| **📊 Depuração Sincronizada** | Logs USB refletem ações em tempo real |

---

## 🛠️ **Requisitos de Hardware**
- Placa Bitdoglab

### 📂 **Arquivos do Projeto**
```
📁 projeto/
├── picow_access_point.c          (código principal)
├── dhcpserver/
│   ├── dhcpserver.c
│   └── dhcpserver.h
├── dnsserver/
│   ├── dnsserver.c
│   └── dnsserver.h
├── lwipopts.h                    (configuração LwIP)
└── CMakeLists.txt                (configuração build)
```

---

## 📲 **Como Usar**

### 🔌 **Conecte-se ao Access Point**
1. 🔍 Busque redes Wi-Fi
2. 📶 Conecte-se à `PicoW_Info` (senha: `picopass`)

### 🌐 **Acesse a Interface Web**
1. 🌍 Abra `http://192.168.4.1/info`
2. 🖱️ Interaja com:
   - ✅ LED (ligar/desligar)
   - 🌡️ Temperatura atual

### 🐛 **Depuração**
1. 🔌 Conecte via USB
2. 📟 Monitor serial (115200 bps)
3. 📝 Verifique logs sincronizados

---

## ✅ **Verificação dos Requisitos**

### 🔧 **Requisito 1** - Configuração SDK
- **Onde**: `CMakeLists.txt`
- **Implementação**: 
  ```cmake
  target_link_libraries(picow_access_point_background
      pico_cyw43_arch_lwip_threadsafe_background
      pico_stdlib
      hardware_adc
  )
  ```

### 🔘 **Requisito 2** - Leitura de Temperatura
- **Onde**: `picow_access_point.c`
- **Código**:
  ```c
  adc_init();
  adc_set_temp_sensor_enabled(true);
  ```

### 📶 **Requisito 3** - Access Point
- **Configuração**:
  ```c
  cyw43_arch_enable_ap_mode("PicoW_Info", "picopass", CYW43_AUTH_WPA2_AES_PSK);
  ```

### 🖥️ **Requisito 4** - Página HTML
- **Estrutura Dinâmica**:
  ```c
  #define INFO_PAGE_BODY "<html>...</html>"
  ```

### 💡 **Requisito 5** - Controle GPIO/ADC
- **LED**:
  ```c
  gpio_put(LED_PIN_EXTERNAL, estado);
  ```
- **Temperatura**:
  ```c
  adc_read();
  ```

### 📜 **Requisito 6** - Depuração
- **Logs Sincronizados**:
  ```c
  [LED_CTRL] LED no GPIO13: ON (via Web)
  [TEMP_READ] Temperatura: 25.6°C
  ```

### ⚙️ **Requisito 7** - Compilação
- O CMakeLists.txt está configurado para gerar os arquivos .uf2 necessários (via pico_add_extra_outputs) para serem gravados no Pico W no modo bootloader.

---

## Propósito

Este projeto foi desenvolvido com fins estritamente educacionais e aprendizdo durante a residência em sistemas embarcados pelo embarcatech
