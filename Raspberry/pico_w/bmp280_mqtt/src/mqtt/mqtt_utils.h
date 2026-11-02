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

// Inclui a definição de 'size_t' para os parâmetros de payload.
// Nota didática: optou-se por '<stddef.h>' em vez de '<string.h>'. O primeiro é o header canônico de 'size_t' na
// biblioteca C padrão e não traz protótipos de funções de string que este header não usa. '<string.h>' também
// define 'size_t', mas por consequência, não por intenção.
#include <stddef.h>

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

// Estabelece a sessão MQTT com o broker, usando as credenciais informadas.
//
// Parâmetros:
//   client_id = identificador do cliente (Device ID na plataforma WEGnology).
//   usuario   = Access Key da plataforma WEGnology (ou NULL para conexão anônima).
//   senha     = Access Secret da plataforma WEGnology (ou NULL para conexão anônima).
//
// Nota didática: a plataforma WEGnology exige que o 'client_id' seja o Device ID registrado. Passar NULL ou string
// vazia faz o broker descartar o CONNECT sem responder — o cliente MQTT nunca recebe CONNACK e o 
// 'mqtt_verificar_conexao' expira. Brokers locais como Mosquitto aceitam client_id anônimo, o que torna esta exigência 
// um ponto de confusão comum ao migrar de um broker local para a nuvem.
int mqtt_conectar(const char *client_id, const char *usuario, const char *senha);

// Encerra a sessão MQTT de forma graciosa, enviando o pacote DISCONNECT ao broker.
// Retorna MQTT_OK em caso de sucesso; outro valor em caso de erro.
int mqtt_desconectar(void);

// Publica o payload informado no tópico de estado (state) da plataforma WEGnology, com QoS 0.
int mqtt_publicar(const char *payload, size_t tamanho);

// Processa tráfego MQTT de entrada e saída.
//
// Nota didática: além de processar o tráfego de entrada e saída (via 'mqtt_sync'), esta função também
// descarta da fila interna da MQTT-C as mensagens já concluídas (via 'mqtt_mq_clean'). Em QoS 0, a
// biblioteca só limpa a fila automaticamente quando um novo 'pack' falha por falta de espaço — ou seja,
// em sessões longas sem nunca encher o buffer, a fila cresce indefinidamente até estourar. Chamar
// 'mqtt_mq_clean' a cada iteração evita esse acúmulo.
void mqtt_sincronizar(void);

// Reinicializa o cliente MQTT com um novo socket.
void mqtt_reinicializar(int fd_socket);

// Retorna true se o cliente MQTT está sem erro.
bool mqtt_ok(void);

// Verifica se o evento de conexão foi confirmado.
bool mqtt_verificar_conexao(int tentativas, unsigned int intervalo_us);

// Retorna a string de erro do cliente MQTT.
const char *mqtt_erro_str(void);

#endif /* __MQTT_UTILS_H */

//----------------------------------------------------------------------------------------------------------------------
// Fim do arquivo 'mqtt_utils.h'
//----------------------------------------------------------------------------------------------------------------------
