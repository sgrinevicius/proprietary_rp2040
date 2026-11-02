//######################################################################################################################
// *
// *  Arquivo: "network_utils.h"
// *
// *  Título: Interface de Utilitários de Rede para Aplicações RP2040
// *
// *  Design Pattern: Módulo de Suporte — Encapsulamento de funções de rede.
// *
// *  Objetivo: Declarar as funções de espera por IP e abertura de socket TCP para o broker MQTT.
// *
// *  Autor: DeepSeek
// *  Hardware: Raspberry Pi Pico W (RP2040)
// *  RTOS: Apache NuttX 13.0.0
// *
// *  Modelo: Módulo de Suporte (Categoria 1 da Taxonomia).
// *
// *  NOTA CONCEITUAL — SOBRE O PAPEL DESTE HEADER:
// *
// *    Este header expõe apenas duas funções, cada uma representando uma etapa do processo de conexão de rede:
// *
// *      - 'network_aguardar_ip()'       : sincroniza com o subsistema WiFi, aguardando um IP válido.
// *      - 'network_abrir_socket_broker()': entrega um socket TCP pronto para uso (resolvido, conectado, não-bloqueante).
// *
// *    O aplicativo cliente (main) não precisa saber COMO essas etapas são implementadas — apenas QUANDO
// *    chamá-las e O QUE esperar de cada uma. Esse é o propósito do encapsulamento: isolar o 'como' e expor
// *    o 'o quê'.
// *
// ********************************************************************************************************************/

#ifndef __NETWORK_UTILS_H
#define __NETWORK_UTILS_H

// Guarda de inclusão: impede múltiplas inclusões do mesmo header no mesmo arquivo de código.

/***********************************************************************************************************************
 * Public Function Prototypes
 **********************************************************************************************************************/

// Aguarda até que a interface wlan0 possua um endereço IPv4 diferente de zero, com timeout.
int network_aguardar_ip(void);

// Abre um socket TCP não-bloqueante e conecta ao broker MQTT.
int network_abrir_socket_broker(void);

#endif /* __NETWORK_UTILS_H */

//----------------------------------------------------------------------------------------------------------------------
// Fim do arquivo 'network_utils.h'
//----------------------------------------------------------------------------------------------------------------------
