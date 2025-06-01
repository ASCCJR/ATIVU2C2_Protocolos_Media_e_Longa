/**
 * Projeto: Servidor HTTP com controle de LED (GPIO13) e Leitura de Temperatura via Access Point - Raspberry Pi Pico W
 *
 * Objetivos:
 * - Configurar o Raspberry Pi Pico W como um ponto de acesso (Access Point) Wi-Fi.
 * - Iniciar servidores DHCP e DNS locais para permitir a conexão de dispositivos clientes.
 * - Criar um servidor HTTP embarcado que disponibiliza uma página HTML de controle.
 * - Permitir o controle remoto de um LED conectado ao GPIO13 através de comandos HTTP.
 * - Exibir a temperatura interna do microcontrolador RP2040 na página web.
 * - Exibir mensagens de depuração no terminal USB sincronizadas com as interações da página HTML,
 * incluindo o estado do LED e a temperatura sempre que a página de informações for carregada.
 *
 * Requisitos:
 * Requisito 1: Inicialize o projeto utilizando o SDK do Raspberry Pi Pico e configure o
 * CMakeLists.txt com as bibliotecas necessárias: `hardware_adc`,
 * `pico_stdio_usb`, `pico_stdlib` e
 * `pico_cyw43_arch_lwip_threadsafe_background`.
 * Requisito 2: No código, implemente a leitura da temperatura interna do
 * microcontrolador usando o canal 4 do ADC.
 * Requisito 3: Configure o Pico W como Access Point, utilizei SSID 'PicoW_Info' e senha
 * 'picopass'.
 * Requisito 4: Crie uma página HTML embarcada com informações sobre o estado do LED
 * e a temperatura.
 * Requisito 5: Utilize a função `gpio_put()` para controlar um LED no GPIO 13 e
 * `adc_read()` para ler o sensor.
 * Requisito 6: Exiba mensagens de depuração no terminal USB com `printf()` para
 * confirmar conexões e leituras.
 * Requisito 7: Compile o projeto e grave no Pico W.
 */

#include <string.h>
#include <stdio.h> 

// REQUISITO 1: Uso de bibliotecas do SDK (pico_stdlib, hardware_adc, pico_cyw43_arch)
#include "pico/cyw43_arch.h" 
#include "pico/stdlib.h"    
#include "hardware/gpio.h"  
#include "hardware/adc.h"   

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "dhcpserver.h"
#include "dnsserver.h"

#define TCP_PORT 80
// REQUISITO 6: Uso de printf para debug (DEBUG_printf é apenas uma outra maneira de se referir à função printf...)
#define DEBUG_printf printf 
#define POLL_TIME_S 5
#define HTTP_GET "GET"
#define HTTP_RESPONSE_HEADERS "HTTP/1.1 %d OK\nContent-Length: %d\nContent-Type: text/html; charset=utf-8\nConnection: close\n\n"

// REQUISITO 5: Definição do pino do LED (GPIO 13)
#define LED_PIN_EXTERNAL 13

// REQUISITO 4: Criação da página HTML embarcada (estrutura base)
#define INFO_PAGE_BODY "<html><body style=\"font-family: sans-serif; text-align: center;\">" \
                       "<h1>Controle do Pico W</h1>" \
                       "<div style=\"margin-bottom: 20px;\">" \
                       "<h2>LED (GPIO %d)</h2>" \
                       "<p style=\"font-size: 1.2em;\">O LED está atualmente: <span style=\"font-weight: bold; color: %s;\">%s</span></p>" \
                       "<p><a href=\"%s?led=%d\" style=\"display: inline-block; padding: 10px 20px; font-size: 1em; color: white; background-color: %s; text-decoration: none; border-radius: 5px;\">%s o LED</a></p>" \
                       "</div>" \
                       "<div style=\"margin-top: 20px; padding-top: 20px; border-top: 1px solid #ccc;\">" \
                       "<h2>Temperatura Interna</h2>" \
                       "<p style=\"font-size: 1.2em;\">Temperatura do RP2040: <span style=\"font-weight: bold; color: #337ab7;\">%.2f &deg;C</span></p>" \
                       "</div>" \
                       "</body></html>"

#define LED_PARAM "led=%d"       
#define INFO_PATH "/info"      

#define HTTP_RESPONSE_REDIRECT "HTTP/1.1 302 Redirect\nLocation: http://%s" INFO_PATH "\n\n"

const float ADC_CONVERSION_FACTOR = 3.3f / (1 << 12); 

typedef struct TCP_SERVER_T_ {
    struct tcp_pcb *server_pcb;
    bool complete;
    ip_addr_t gw;
} TCP_SERVER_T;

typedef struct TCP_CONNECT_STATE_T_ {
    struct tcp_pcb *pcb;
    int sent_len;
    char headers[128];
    char result[768]; 
    int header_len;
    int result_len;
    ip_addr_t *gw;
} TCP_CONNECT_STATE_T;


// REQUISITO 2: Implementação da leitura da temperatura interna usando o canal 4 do ADC.
// REQUISITO 5: Uso de adc_read() para ler o sensor.
float read_onboard_temperature() {
    adc_select_input(4); // REQUISITO 2 (uso do canal 4)
    uint16_t raw_adc_value = adc_read(); // REQUISITO 2 e REQUISITO 5 (uso de adc_read)
    float voltage = raw_adc_value * ADC_CONVERSION_FACTOR;
    float temperature = 27.0f - (voltage - 0.706f) / 0.001721f;
    return temperature;
}


static err_t tcp_close_client_connection(TCP_CONNECT_STATE_T *con_state, struct tcp_pcb *client_pcb, err_t close_err) {
    if (client_pcb) {
        tcp_arg(client_pcb, NULL);
        tcp_poll(client_pcb, NULL, 0);
        tcp_sent(client_pcb, NULL);
        tcp_recv(client_pcb, NULL);
        tcp_err(client_pcb, NULL);
        err_t err = tcp_close(client_pcb);
        if (err != ERR_OK) {
            DEBUG_printf("Falha ao fechar conexão, chamando abort. Erro: %d\n", err); // REQUISITO 6
            tcp_abort(client_pcb);
            close_err = ERR_ABRT;
        }
        if (con_state) {
            free(con_state);
        }
    }
    return close_err;
}

static void tcp_server_close(TCP_SERVER_T *state) {
    if (state->server_pcb) {
        tcp_arg(state->server_pcb, NULL);
        tcp_close(state->server_pcb);
        state->server_pcb = NULL;
    }
}

static err_t tcp_server_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;
    if (!con_state) return ERR_OK; 
    DEBUG_printf("tcp_server_sent %u\n", len); // REQUISITO 6
    con_state->sent_len += len;
    if (con_state->sent_len >= con_state->header_len + con_state->result_len) {
        DEBUG_printf("Todos os dados enviados.\n"); // REQUISITO 6
        return tcp_close_client_connection(con_state, pcb, ERR_OK);
    }
    return ERR_OK;
}

// REQUISITO 4: Página HTML embarcada é gerada aqui com informações do LED e temperatura.
static int server_content_handler(const char *request_uri_path, const char *params, char *result_buffer, size_t max_result_len) {
    int content_len = 0;
    
    if (strncmp(request_uri_path, INFO_PATH, strlen(INFO_PATH)) == 0 &&
        (request_uri_path[strlen(INFO_PATH)] == ' ' || request_uri_path[strlen(INFO_PATH)] == '?' || request_uri_path[strlen(INFO_PATH)] == '\0')) {
        
        // Pega o estado do LED que será usado para a página.
        // Este estado pode ser modificado abaixo se houver parâmetros.
        bool current_led_state_bool = gpio_get(LED_PIN_EXTERNAL); // REQUISITO 5 (leitura inicial do estado do LED)

        if (params) {
            int scanned_led_param;
            if (sscanf(params, LED_PARAM, &scanned_led_param) == 1) {
                bool new_state_requested = (scanned_led_param == 1);
                if (new_state_requested != current_led_state_bool) { 
                    // REQUISITO 5: Uso de gpio_put() para controlar o LED no GPIO 13.
                    gpio_put(LED_PIN_EXTERNAL, new_state_requested);
                    current_led_state_bool = new_state_requested; // Atualiza o estado que será mostrado na página
                    // Log da mudança de estado do LED no monitor serial
                    DEBUG_printf("[LED_CTRL] LED no GPIO%d: %s (via Web)\n", 
                                 LED_PIN_EXTERNAL, current_led_state_bool ? "LIGADO" : "DESLIGADO"); // REQUISITO 6 
                } else {
                    DEBUG_printf("[LED_CTRL] LED no GPIO%d já está: %s (solicitação Web)\n",
                                 LED_PIN_EXTERNAL, current_led_state_bool ? "LIGADO" : "DESLIGADO"); // REQUISITO 6 
                }
            }
        }
        
        // Loga o estado final do LED que será enviado para a página HTML
        DEBUG_printf("[LED_STATUS] Estado do LED para página HTML: %s\n", current_led_state_bool ? "LIGADO" : "DESLIGADO"); // REQUISITO 6 

        // REQUISITO 2: Chamada para ler a temperatura.
        float temperature = read_onboard_temperature();
        // Log da temperatura lida para a página HTML no monitor serial
        DEBUG_printf("[TEMP_READ] Temperatura para página HTML: %.2f C\n", temperature); // REQUISITO 6 


        const char *status_text, *status_color, *action_text, *action_button_color;
        int next_led_val;

        if (current_led_state_bool) {
            status_text = "LIGADO"; status_color = "green"; action_text = "Desligar"; action_button_color = "red"; next_led_val = 0;
        } else {
            status_text = "DESLIGADO"; status_color = "red"; action_text = "Ligar"; action_button_color = "green"; next_led_val = 1;
        }

        // REQUISITO 4: snprintf() preenche a página HTML com dados dinâmicos (estado do LED, temperatura).
        content_len = snprintf(result_buffer, max_result_len, INFO_PAGE_BODY,
                               LED_PIN_EXTERNAL, status_color, status_text,
                               INFO_PATH, next_led_val, action_button_color, action_text,
                               temperature);
    }
    return content_len;
}


err_t tcp_server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;

    if (!p) { 
        DEBUG_printf("Conexão fechada pelo cliente ou erro fatal.\n"); // REQUISITO 6
        return tcp_close_client_connection(con_state, pcb, ERR_OK); 
    }

    if (!con_state) {
        DEBUG_printf("Estado da conexão é NULL em tcp_server_recv. Liberando pbuf e retornando.\n"); // REQUISITO 6
        pbuf_free(p);
        return ERR_ABRT; 
    }
    
    assert(con_state->pcb == pcb); 

    if (p->tot_len > 0) {
        DEBUG_printf("tcp_server_recv: %d bytes, erro: %d\n", p->tot_len, err); // REQUISITO 6

        size_t req_len = p->tot_len > sizeof(con_state->headers) - 1 ? sizeof(con_state->headers) - 1 : p->tot_len;
        pbuf_copy_partial(p, con_state->headers, req_len, 0);
        con_state->headers[req_len] = '\0';

        if (strncmp(HTTP_GET, con_state->headers, strlen(HTTP_GET)) == 0) {
            char *full_request_line = con_state->headers;
            char *method = strtok(full_request_line, " ");
            char *uri_with_params = strtok(NULL, " ");

            if (method && uri_with_params) {
                char *request_uri_path = strtok(uri_with_params, "?");
                char *params_str = strtok(NULL, ""); 

                con_state->result_len = server_content_handler(request_uri_path, params_str, con_state->result, sizeof(con_state->result));
                DEBUG_printf("Requisição: %s?%s\n", request_uri_path, params_str ? params_str : ""); // REQUISITO 6
                DEBUG_printf("Tamanho do resultado: %d\n", con_state->result_len); // REQUISITO 6

                if (con_state->result_len >= sizeof(con_state->result)) {
                    DEBUG_printf("Buffer de resultado muito pequeno: %d necessário.\n", con_state->result_len); // REQUISITO 6
                    return tcp_close_client_connection(con_state, pcb, ERR_CLSD);
                }

                if (con_state->result_len > 0) {
                    con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_HEADERS,
                        200, con_state->result_len);
                } else {
                    con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_REDIRECT,
                        ipaddr_ntoa(con_state->gw));
                    DEBUG_printf("Enviando redirecionamento: %s", con_state->headers); // REQUISITO 6
                }
                
                if (con_state->header_len >= sizeof(con_state->headers)) {
                     DEBUG_printf("Buffer de cabeçalho muito pequeno: %d necessário.\n", con_state->header_len); // REQUISITO 6
                    return tcp_close_client_connection(con_state, pcb, ERR_CLSD);
                }

                con_state->sent_len = 0;
                err_t write_err = tcp_write(pcb, con_state->headers, con_state->header_len, TCP_WRITE_FLAG_COPY);
                if (write_err != ERR_OK) {
                    DEBUG_printf("Falha ao escrever cabeçalhos: %d\n", write_err); // REQUISITO 6
                    return tcp_close_client_connection(con_state, pcb, write_err);
                }

                if (con_state->result_len > 0) {
                    write_err = tcp_write(pcb, con_state->result, con_state->result_len, TCP_WRITE_FLAG_COPY);
                    if (write_err != ERR_OK) {
                        DEBUG_printf("Falha ao escrever corpo: %d\n", write_err); // REQUISITO 6
                        return tcp_close_client_connection(con_state, pcb, write_err);
                    }
                }
            } else {
                 DEBUG_printf("Requisição GET mal formada.\n"); // REQUISITO 6
                 return tcp_close_client_connection(con_state, pcb, ERR_ARG);
            }
        }
        tcp_recved(pcb, p->tot_len);
    }
    pbuf_free(p);
    return ERR_OK;
}

static err_t tcp_server_poll(void *arg, struct tcp_pcb *pcb) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;
    DEBUG_printf("tcp_server_poll_fn chamado.\n"); // REQUISITO 6
    if (!con_state) {
        DEBUG_printf("Estado da conexão NULL no poll. Abortando PCB.\n"); // REQUISITO 6
        tcp_abort(pcb); 
        return ERR_ABRT;
    }
    return tcp_close_client_connection(con_state, pcb, ERR_OK); 
}

static void tcp_server_err(void *arg, err_t err) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;
    DEBUG_printf("tcp_server_err_fn chamado com erro: %d\n", err); // REQUISITO 6
    if (err != ERR_ABRT) { 
        if (con_state) {
            DEBUG_printf("Liberando estado da conexão devido a erro.\n"); // REQUISITO 6
            free(con_state);
        }
    }
}

static err_t tcp_server_accept(void *arg, struct tcp_pcb *client_pcb, err_t err) {
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    if (err != ERR_OK || client_pcb == NULL) {
        DEBUG_printf("Falha no accept, erro: %d\n", err); // REQUISITO 6
        return ERR_VAL;
    }
    DEBUG_printf("Cliente conectado: %s:%u\n", ipaddr_ntoa(&(client_pcb->remote_ip)), client_pcb->remote_port); // REQUISITO 6 


    TCP_CONNECT_STATE_T *con_state = calloc(1, sizeof(TCP_CONNECT_STATE_T));
    if (!con_state) {
        DEBUG_printf("Falha ao alocar estado de conexão.\n"); // REQUISITO 6
        tcp_close(client_pcb); 
        return ERR_MEM;
    }
    con_state->pcb = client_pcb; 
    con_state->gw = &state->gw;

    tcp_arg(client_pcb, con_state);
    tcp_sent(client_pcb, tcp_server_sent);
    tcp_recv(client_pcb, tcp_server_recv);
    tcp_poll(client_pcb, tcp_server_poll, POLL_TIME_S * 2); 
    tcp_err(client_pcb, tcp_server_err);

    return ERR_OK;
}

static bool tcp_server_open(void *arg, const char *ap_name) {
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    DEBUG_printf("Iniciando servidor na porta %d\n", TCP_PORT); // REQUISITO 6

    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!pcb) {
        DEBUG_printf("Falha ao criar pcb.\n"); // REQUISITO 6
        return false;
    }

    err_t err = tcp_bind(pcb, IP_ANY_TYPE, TCP_PORT);
    if (err != ERR_OK) { 
        DEBUG_printf("Falha ao fazer bind na porta %d, erro %d\n",TCP_PORT, err); // REQUISITO 6
        tcp_close(pcb); 
        return false;
    }

    state->server_pcb = tcp_listen_with_backlog(pcb, 1); 
    if (!state->server_pcb) {
        DEBUG_printf("Falha ao escutar.\n"); // REQUISITO 6
        if (pcb) { 
            tcp_close(pcb);
        }
        return false;
    }

    tcp_arg(state->server_pcb, state); 
    tcp_accept(state->server_pcb, tcp_server_accept);

    // REQUISITO 6: Mensagem de feedback para o usuário no terminal
    printf("Tente conectar a '%s' (pressione 'd' para desabilitar o ponto de acesso)\n", ap_name);
    printf("Acesse http://%s%s para controlar o LED no GPIO %d e ver a temperatura.\n", ipaddr_ntoa(&state->gw), INFO_PATH, LED_PIN_EXTERNAL);
    return true;
}

void key_pressed_func(void *param) {
    assert(param);
    TCP_SERVER_T *state = (TCP_SERVER_T*)param;
    int key = getchar_timeout_us(0); 
    if (key == 'd' || key == 'D') {
        cyw43_arch_lwip_begin();
        cyw43_arch_disable_ap_mode();
        cyw43_arch_lwip_end();
        state->complete = true;
        DEBUG_printf("Modo AP desabilitado pelo usuário.\n"); // REQUISITO 6
    }
}

int main() {
    // REQUISITO 1: Uso de stdio_init_all (parte da infraestrutura para pico_stdio_usb)
    // REQUISITO 6: Inicialização do stdio para permitir printf via USB
    stdio_init_all();
    sleep_ms(2000); 

    DEBUG_printf("Inicializando...\n"); // REQUISITO 6
    DEBUG_printf("Este projeto configura um Access Point Wi-Fi.\n"); // REQUISITO 6
    DEBUG_printf("Controle de LED no GPIO%d e leitura de temperatura interna.\n", LED_PIN_EXTERNAL); // REQUISITO 6

    // REQUISITO 1 e 2: Inicialização do ADC para leitura da temperatura
    adc_init();
    adc_set_temp_sensor_enabled(true); 

    // REQUISITO 5: Inicialização do GPIO 13 para o LED
    gpio_init(LED_PIN_EXTERNAL);
    gpio_set_dir(LED_PIN_EXTERNAL, GPIO_OUT);
    gpio_put(LED_PIN_EXTERNAL, 0); 

    TCP_SERVER_T *state = calloc(1, sizeof(TCP_SERVER_T));
    if (!state) {
        DEBUG_printf("Falha ao alocar estado.\n"); // REQUISITO 6
        return 1;
    }

    // REQUISITO 1: Inicialização do cyw43 
    if (cyw43_arch_init()) {
        DEBUG_printf("Falha ao inicializar cyw43.\n"); // REQUISITO 6
        free(state);
        return 1;
    }
    DEBUG_printf("CYW43 inicializado.\n"); // REQUISITO 6

    stdio_set_chars_available_callback(key_pressed_func, state);

    // REQUISITO 3: Configuração do Pico W como Access Point (SSID e senha)
    const char *ap_name = "PicoW_Info"; 
    const char *password = "picopass"; 

    cyw43_arch_enable_ap_mode(ap_name, password, CYW43_AUTH_WPA2_AES_PSK);
    DEBUG_printf("Modo AP habilitado: SSID='%s'\n", ap_name); // REQUISITO 6

    ip4_addr_t mask;
    IP4_ADDR(ip_2_ip4(&state->gw), 192, 168, 4, 1); 
    IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);    
    DEBUG_printf("IP do AP: %s\n", ipaddr_ntoa(&state->gw)); // REQUISITO 6

    dhcp_server_t dhcp_server;
    dhcp_server_init(&dhcp_server, &state->gw, &mask);
    DEBUG_printf("Servidor DHCP iniciado.\n"); // REQUISITO 6

    dns_server_t dns_server;
    dns_server_init(&dns_server, &state->gw);
    DEBUG_printf("Servidor DNS iniciado.\n"); // REQUISITO 6

    if (!tcp_server_open(state, ap_name)) {
        DEBUG_printf("Falha ao abrir servidor TCP.\n"); // REQUISITO 6
        dhcp_server_deinit(&dhcp_server);
        dns_server_deinit(&dns_server);
        cyw43_arch_deinit();
        free(state);
        return 1;
    }
    DEBUG_printf("Servidor TCP aberto e escutando na porta %d.\n", TCP_PORT); // REQUISITO 6

    state->complete = false;
    
    while(!state->complete) {
#if PICO_CYW43_ARCH_POLL
        cyw43_arch_poll();
        busy_wait_ms(1); 
#else
        sleep_ms(100); 
#endif
    }

    DEBUG_printf("Fechando servidores...\n"); // REQUISITO 6
    tcp_server_close(state);
    dns_server_deinit(&dns_server);
    dhcp_server_deinit(&dhcp_server);
    
    cyw43_arch_deinit();
    DEBUG_printf("CYW43 desinicializado.\n"); // REQUISITO 6
    free(state);
    // REQUISITO 6: Mensagem final no terminal
    printf("Teste completo. Pico W desligando AP.\n"); 
    // REQUISITO 7: Compile o projeto e grave no Pico W.
    return 0;
}
