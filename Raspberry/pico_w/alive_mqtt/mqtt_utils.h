//######################################################################################################################
// *
// *  Arquivo: "mqtt_utils.h"
// *
// *  Título: Interface de Utilitários MQTT para Aplicações RP2040
// *
// *  Design Pattern: Módulo de Suporte — Encapsulamento de funções MQTT.
// *
// *  Objetivo: Declarar as funções de inicialização, conexão, publicação, sincronização e controle de LED externo.
// *
// *  Autor: DeepSeek
// *  Hardware: Raspberry Pi Pico W (RP2040)
// *  RTOS: Apache NuttX 13.0.0
// *
// *  Modelo: Módulo de Suporte (Categoria 1 da Taxonomia).
// *
// ********************************************************************************************************************/

#ifndef __MQTT_UTILS_H
#define __MQTT_UTILS_H

// Guarda de inclusão: impede múltiplas inclusões do mesmo header no mesmo arquivo de código.

// Inclui o tipo booleano nativo (true/false) do C23
#include <stdbool.h>

// Inclui a API do cliente MQTT-C, com funções de conexão, publicação e sincronização
#include <mqtt.h>

/***********************************************************************************************************************
 * Public Function Prototypes
 **********************************************************************************************************************/

// Abre o dispositivo do LED externo.
void mqtt_abrir_led(void);

// Fecha o dispositivo do LED externo.
void mqtt_fechar_led(void);

// Acende o LED externo por 100 ms e depois apaga.
void mqtt_flash_led(void);

// Inicializa o cliente MQTT-C com socket, buffers e callback.
void mqtt_inicializar(int fd_socket);

// Estabelece a sessão MQTT com o broker.
int mqtt_conectar(void);

// Publica a mensagem "alive" no tópico "pico/status".
int mqtt_publicar(void);

// Processa tráfego MQTT de entrada e saída.
void mqtt_sincronizar(void);

// Reinicializa o cliente MQTT com um novo socket.
void mqtt_reinicializar(int fd_socket);

// Retorna true se o cliente MQTT está sem erro.
bool mqtt_esta_ok(void);

// Verifica se o evento de conexão foi confirmado.
bool mqtt_verificar_conexao(int tentativas, unsigned int intervalo_us);

// Retorna a string de erro do cliente MQTT.
const char *mqtt_erro_str(void);

#endif /* __MQTT_UTILS_H */

//----------------------------------------------------------------------------------------------------------------------
// Fim do arquivo 'mqtt_utils.h'
//----------------------------------------------------------------------------------------------------------------------
