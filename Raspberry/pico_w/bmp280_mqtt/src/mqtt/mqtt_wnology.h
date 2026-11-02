//######################################################################################################################
// *
// *  Arquivo: "mqtt_wnology.h"
// *
// *  Título: Interface de Gerenciamento do Ciclo de Vida MQTT para a Plataforma WEGnology
// *
// *  Design Pattern: Módulo Gerenciador — Encapsulamento do ciclo de vida MQTT.
// *
// *  Objetivo: Declarar as funções que gerenciam a conexão, reconexão e publicação MQTT para a plataforma WEGnology,
// *            abstraindo de rede, credenciais e retentativas do aplicativo cliente.
// *
// *  Autor: Sérgio Grinevicius
// *  Hardware: Raspberry Pi Pico W (RP2040)
// *  RTOS: Apache NuttX 13.0.0
// *
// *  Modelo: Módulo Gerenciador (Categoria 1 da Taxonomia).
// *
// *  NOTA CONCEITUAL — SOBRE A FLAG 'terminar':
// *
// *    Todas as funções deste módulo que contêm laços de retentativa — 'mqtt_wnology_iniciar' e
// *    'mqtt_wnology_reconectar' — recebem um ponteiro para uma flag booleana de término ('terminar').
// *
// *    Por que passar a flag como parâmetro, em vez de usar uma variável global dentro deste módulo?
// *
// *      - A flag pertence ao APLICATIVO, não a este módulo. É o aplicativo que decide o que significa
// *        "terminar" (Ctrl+C, comando remoto, timeout, etc.). Passar por parâmetro mantém este módulo
// *        agnóstico de como o aplicativo decide encerrar.
// *
// *      - Também permite testes futuros: um teste unitário pode chamar 'mqtt_wnology_iniciar' com uma
// *        flag local e simulada, sem depender do estado global do aplicativo.
// *
// *    O ponteiro é 'volatile' porque pode ser modificado de forma assíncrona por um handler de sinal
// *    (SIGINT). O qualificador instrui o compilador a recarregar o valor a cada leitura, em vez de
// *    mantê-lo em registrador.
// *
// ********************************************************************************************************************/

#ifndef __MQTT_WNOLOGY_H
#define __MQTT_WNOLOGY_H

// Guarda de inclusão: impede múltiplas inclusões do mesmo header no mesmo arquivo de código.

// Inclui o tipo booleano nativo (true/false) do C23
#include <stdbool.h>

// Inclui a definição de 'size_t' para o parâmetro de tamanho do payload
#include <stddef.h>

/***********************************************************************************************************************
 * Public Function Prototypes
 **********************************************************************************************************************/

// Aguarda IP, abre LED, abre socket, inicializa e conecta o cliente MQTT ao broker WEGnology.
// Retorna OK em caso de sucesso; -1 em caso de falha.
int mqtt_wnology_iniciar(volatile bool *terminar);

// Encerra a sessão MQTT de forma graciosa, fecha o socket e o LED externo.
void mqtt_wnology_finalizar(void);

// Processa tráfego MQTT de entrada e saída (wrapper de 'mqtt_sincronizar').
void mqtt_wnology_sincronizar(void);

// Retorna true se o cliente MQTT está sem erro (wrapper de 'mqtt_esta_ok').
bool mqtt_wnology_ok(void);

// Fecha o socket atual e tenta reconectar ao broker MQTT, respeitando a flag de término.
// Retorna OK em caso de sucesso; -1 em caso de falha.
int mqtt_wnology_reconectar(volatile bool *terminar);

// Publica o payload informado no tópico de estado (state) da plataforma WEGnology, com QoS 0.
int mqtt_wnology_publicar(const char *payload, size_t tamanho);

// Retorna a string de erro do cliente MQTT (wrapper de 'mqtt_erro_str').
const char *mqtt_wnology_erro_str(void);

#endif /* __MQTT_WNOLOGY_H */

//----------------------------------------------------------------------------------------------------------------------
// Fim do arquivo 'mqtt_wnology.h'
//----------------------------------------------------------------------------------------------------------------------
