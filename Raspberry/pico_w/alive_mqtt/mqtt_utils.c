//######################################################################################################################
// *
// *  Arquivo: "mqtt_utils.c"
// *
// *  Título: Utilitários MQTT para Aplicações RP2040
// *
// *  Design Pattern: Módulo de Suporte — Encapsulamento de funções MQTT.
// *
// *  Objetivo: Implementar funções de inicialização, conexão, publicação, sincronização e controle de Led externo.
// *
// *  Autor: DeepSeek
// *  Hardware: Raspberry Pi Pico W (RP2040)
// *  RTOS: Apache NuttX 13.0.0
// *
// *  Modelo: Módulo de Suporte (Categoria 1 da Taxonomia).
// *
// *  NOTA CONCEITUAL:
// *    Este módulo concentra toda a comunicação MQTT e o controle do Led externo associado à publicação.
// *    A separação facilita a manutenção e permite reutilização em outros projetos.
// *
// ********************************************************************************************************************/

/*
 * Inclusão de Diretivas de Compilação - Início ------------------------------------------------------------------------
 */

/*
 * Inclusão de Bibliotecas em Arquivos de Cabeçalho --------------------------------------------------------------------
 */
/*
 * NuttX — Configuração do Kernel:
 */

// Inclui macros globais de configuração do Kernel NuttX, geradas pelo Kconfig
#include <nuttx/config.h>

/*
 * C Libraries:
 */

// Inclui a biblioteca padrão para funções como strlen
//#include <string.h>

// Inclui funções padrão do sistema como usleep
#include <unistd.h>

/*
 * POSIX:
 */

// Inclui a interface de controle de arquivos, necessária para a função open()
#include <fcntl.h>

// Inclui a interface de controle de dispositivos, necessária para a função ioctl()
#include <sys/ioctl.h>

/*
 * NuttX — APIs e utilitários:
 */

// Inclui funções de debug e logging do NuttX, como syslog()
#include <nuttx/debug.h>

// Inclui as definições de comandos ioctl para GPIO user-space, como GPIOC_WRITE
#include <nuttx/ioexpander/gpio.h>

/*
 * Módulos do aplicativo:
 */

// Inclui o header do módulo MQTT
#include "mqtt_utils.h"


/*
 * Definições Gerais ---------------------------------------------------------------------------------------------------
 */

#define TOPICO_MQTT           "pico/status"   // Define o tópico MQTT para publicação do sinal de vida


#define PAYLOAD_MQTT          "alive"         // Define o payload da mensagem de sinal de vida


#define TAMANHO_PAYLOAD_MQTT  5               // Define o tamanho do payload em bytes (5 caracteres + terminador)


#define MQTT_KEEP_ALIVE_S     400             // Define o keep-alive MQTT em segundos


#define CAMINHO_LED_EXTERNO   "/dev/gpio16"   // Define o caminho do dispositivo de caractere GPIO16 (Led externo)


#define TEMPO_FLASH_LED_US    100000          // Define o tempo em microssegundos que o Led fica aceso (flash de 100 ms)

//______________________________________________________________________________________________________________________

/*
 * Variáveis Globais ---------------------------------------------------------------------------------------------------
 */

// Cliente MQTT-C estático, armazenando o contexto da conexão
static struct mqtt_client s_cliente_mqtt;

// Buffer de transmissão do MQTT-C, alinhado a 4 bytes
static uint8_t s_buffer_envio[2048] __attribute__((aligned(4)));

// Buffer de recepção do MQTT-C, alinhado a 4 bytes
static uint8_t s_buffer_recepcao[1024] __attribute__((aligned(4)));

// Descritor de arquivo para o dispositivo GPIO16 (Led externo)
static int s_fd_led = -1;

// Protótipo da função callback de mensagens recebidas (definida como static abaixo)
static void callback_mensagem_recebida(void **estado, struct mqtt_response_publish *publicacao);

//______________________________________________________________________________________________________________________


/*
// -------------------------------------------- Funções Públicas -------------------------------------------------------
 */


//**********************************************************************************************************************
//* FUNCTION: void mqtt_abrir_led(void)
//*
//* PARAMETERS: void
//*
//* DESCRIPTION: Abre o dispositivo do Led externo para permitir o flash a cada publicação.
//*
//* RETURNS: void
//*
//**********************************************************************************************************************
void 
mqtt_abrir_led(void) {

    // Tenta abrir o dispositivo do Led externo
    s_fd_led = open(CAMINHO_LED_EXTERNO,                               // Caminho do dispositivo GPIO
                    O_RDWR);                                          // Modo de abertura: leitura/escrita

    // Verifica se a abertura falhou
    if (s_fd_led < 0) {

        // Registra o erro no syslog
        syslog(LOG_ERR, "[mqtt_abrir_led]: Falha ao abrir dispositivo Led %s\n", CAMINHO_LED_EXTERNO);

    } // fim if

} // fim função void mqtt_abrir_led(void)



//______________________________________________________________________________________________________________________



//**********************************************************************************************************************
//* FUNCTION: void mqtt_fechar_led(void)
//*
//* PARAMETERS: void
//*
//* DESCRIPTION: Fecha o dispositivo do Led externo, se estiver aberto.
//*
//* RETURNS: void
//*
//**********************************************************************************************************************
void mqtt_fechar_led(void) {

    // Verifica se o Led externo foi aberto
    if (s_fd_led >= 0) {

        // Fecha o descritor do Led
        close(s_fd_led);

        // Reseta o descritor para evitar fechamento duplo
        s_fd_led = -1;

    } // fim if

} // fim função void mqtt_fechar_led(void)



//______________________________________________________________________________________________________________________



//**********************************************************************************************************************
//* FUNCTION: void mqtt_flash_led(void)
//*
//* PARAMETERS: void
//*
//* DESCRIPTION: Acende o Led externo conectado ao GPIO16 por 100 ms e depois apaga.
//*
//* RETURNS: void
//*
//**********************************************************************************************************************
void mqtt_flash_led(void) {

    // Verifica se o dispositivo do Led externo está aberto
    if (s_fd_led < 0) {

        // Se não estiver, retorna sem ação
        return;

    } // fim if

    // Escreve nível lógico alto no GPIO16 para acender o Led externo
    ioctl(s_fd_led,                                                   // Descritor do Led
          GPIOC_WRITE,                                               // Comando: escrever valor no pino
          (unsigned long)true);                                           // Valor: true = HIGH

    // Mantém o Led aceso por 100 ms
    usleep(TEMPO_FLASH_LED_US);

    // Escreve nível lógico baixo no GPIO16 para apagar o Led externo
    ioctl(s_fd_led,                                                   // Descritor do Led
          GPIOC_WRITE,                                               // Comando: escrever valor no pino
          (unsigned long)false);                                          // Valor: false = LOW

} // fim função void mqtt_flash_led(void)



//______________________________________________________________________________________________________________________



//**********************************************************************************************************************
//* FUNCTION: void mqtt_inicializar(int fd_socket)
//*
//* PARAMETERS: int fd_socket = socket conectado ao broker MQTT.
//*
//* DESCRIPTION: Inicializa o cliente MQTT-C com socket, buffers e callback.
//*
//* RETURNS: void
//*
//**********************************************************************************************************************
void mqtt_inicializar(int fd_socket) {

    // Inicializa o cliente MQTT-C com socket, buffers e callback
    mqtt_init(&s_cliente_mqtt,                                       // Cliente MQTT
              fd_socket,                                             // Socket conectado
              s_buffer_envio,                                       // Buffer de transmissão
              sizeof(s_buffer_envio),                             // Tamanho do buffer de transmissão
              s_buffer_recepcao,                                    // Buffer de recepção
              sizeof(s_buffer_recepcao),                          // Tamanho do buffer de recepção
              callback_mensagem_recebida);        // Callback de mensagens recebidas

} // fim função void mqtt_inicializar(int fd_socket)



//______________________________________________________________________________________________________________________



//**********************************************************************************************************************
//* FUNCTION: int mqtt_conectar(void)
//*
//* PARAMETERS: void
//*
//* DESCRIPTION: Estabelece a sessão MQTT com o broker.
//*
//* RETURNS: MQTT_OK em caso de sucesso; outro valor em caso de erro.
//*
//**********************************************************************************************************************
int mqtt_conectar(void) {

    // Estabelece a sessão MQTT com o broker
    return mqtt_connect(&s_cliente_mqtt,
                        NULL,                               // client_id: anônimo
                        NULL,                              // will_topic: nenhum
                        NULL,                            // will_message: nenhum
                        0,                          // will_message_size: 0
                        NULL,                               // user_name: sem autenticação
                        NULL,                                // password: sem autenticação
                        MQTT_CONNECT_CLEAN_SESSION,     // connect_flags: sessão limpa
                        MQTT_KEEP_ALIVE_S);                // keep_alive: 400 segundos

} // fim função int mqtt_conectar(void)



//______________________________________________________________________________________________________________________



//**********************************************************************************************************************
//* FUNCTION: int mqtt_publicar(void)
//*
//* PARAMETERS: void
//*
//* DESCRIPTION: Publica a mensagem "alive" no tópico "pico/status" com QoS 0.
//*
//* RETURNS: MQTT_OK em caso de sucesso; outro valor em caso de erro.
//*
//**********************************************************************************************************************
int mqtt_publicar(void) {

    // Publica o payload no tópico definido com QoS 0
    return mqtt_publish(&s_cliente_mqtt,                          // Cliente MQTT
                        TOPICO_MQTT,                          // Tópico de publicação
                        PAYLOAD_MQTT,                // Payload da mensagem
                        TAMANHO_PAYLOAD_MQTT,   // Tamanho do payload
                        MQTT_PUBLISH_QOS_0);               // QoS 0

} // fim função int mqtt_publicar(void)



//______________________________________________________________________________________________________________________



//**********************************************************************************************************************
//* FUNCTION: void mqtt_sincronizar(void)
//*
//* PARAMETERS: void
//*
//* DESCRIPTION: Processa tráfego MQTT de entrada e saída.
//*
//* RETURNS: void
//*
//**********************************************************************************************************************
void mqtt_sincronizar(void) {

    // Processa tráfego MQTT de entrada e saída
    mqtt_sync(&s_cliente_mqtt);

} // fim função void mqtt_sincronizar(void)



//______________________________________________________________________________________________________________________



//**********************************************************************************************************************
//* FUNCTION: void mqtt_reinicializar(int fd_socket)
//*
//* PARAMETERS: int fd_socket = novo socket conectado.
//*
//* DESCRIPTION: Reinicializa o cliente MQTT com um novo socket.
//*
//* RETURNS: void
//*
//**********************************************************************************************************************
void mqtt_reinicializar(int fd_socket) {

    // Reinicializa o cliente MQTT com o novo socket
    mqtt_reinit(&s_cliente_mqtt,                                  // Cliente MQTT
                fd_socket,                                      // Novo socket conectado
                s_buffer_envio,                                  // Buffer de transmissão
                sizeof(s_buffer_envio),                        // Tamanho do buffer de transmissão
                s_buffer_recepcao,                               // Buffer de recepção
                sizeof(s_buffer_recepcao));                    // Tamanho do buffer de recepção

} // fim função void mqtt_reinicializar(int fd_socket)



//______________________________________________________________________________________________________________________



//**********************************************************************************************************************
//* FUNCTION: bool mqtt_esta_ok(void)
//*
//* PARAMETERS: void
//*
//* DESCRIPTION: Retorna true se o cliente MQTT está sem erro.
//*
//* RETURNS: true se MQTT_OK; false caso contrário.
//*
//**********************************************************************************************************************
bool mqtt_esta_ok(void) {

    // Retorna true se o cliente MQTT está sem erro
    return s_cliente_mqtt.error == MQTT_OK;

} // fim função bool mqtt_esta_ok(void)



//______________________________________________________________________________________________________________________



//**********************************************************************************************************************
//* FUNCTION: bool mqtt_verificar_conexao(int tentativas, unsigned int intervalo_us)
//*
//* PARAMETERS: int tentativas = número máximo de tentativas, unsigned int intervalo_us = intervalo em microssegundos.
//*
//* DESCRIPTION: Verifica se o evento de conexão foi confirmado.
//*
//* RETURNS: true se conectado; false caso contrário.
//*
//**********************************************************************************************************************
bool mqtt_verificar_conexao(int tentativas, unsigned int intervalo_us) {

    // Loop de verificação do evento de conexão
    for (int i = 0; i < tentativas; i++) {

        // Processa tráfego MQTT
        mqtt_sync(&s_cliente_mqtt);

        // Se o evento de conexão for verdadeiro, interrompe o loop
        if (s_cliente_mqtt.event_connect) {

            // Retorna true indicando que a conexão foi confirmada
            return true;

        } // fim if

        // Aguarda o intervalo especificado antes da próxima tentativa
        usleep(intervalo_us);

    } // fim for

    // Se chegou aqui, a conexão não foi confirmada
    return false;

} // fim função bool mqtt_verificar_conexao(int tentativas, unsigned int intervalo_us)



//______________________________________________________________________________________________________________________



//**********************************************************************************************************************
//* FUNCTION: const char *mqtt_erro_str(void)
//*
//* PARAMETERS: void
//*
//* DESCRIPTION: Retorna a string de erro do cliente MQTT.
//*
//* RETURNS: Ponteiro para a string de erro.
//*
//**********************************************************************************************************************
const char *mqtt_erro_str(void) {

    // Retorna a string de erro do cliente MQTT
    return mqtt_error_str(s_cliente_mqtt.error);

} // fim função const char *mqtt_erro_str(void)



//______________________________________________________________________________________________________________________



//**********************************************************************************************************************
//* FUNCTION: static void callback_mensagem_recebida(void **estado, struct mqtt_response_publish *publicacao)
//*
//* PARAMETERS: void **estado = estado do callback, struct mqtt_response_publish *publicacao = mensagem recebida.
//*
//* DESCRIPTION: Callback vazio usado apenas para satisfazer a API do MQTT-C.
//*
//* RETURNS: void
//*
//**********************************************************************************************************************
static void callback_mensagem_recebida(void **estado, struct mqtt_response_publish *publicacao) {

    // Nenhuma ação necessária

} // fim função static void callback_mensagem_recebida(void **estado, struct mqtt_response_publish *publicacao)


//______________________________________________________________________________________________________________________


//----------------------------------------------------------------------------------------------------------------------
// Fim do arquivo 'mqtt_utils.c'
//----------------------------------------------------------------------------------------------------------------------