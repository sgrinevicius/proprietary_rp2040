//######################################################################################################################
// *
// *  Arquivo: "network_utils.c"
// *
// *  Título: Utilitários de Rede para Aplicações RP2040
// *
// *  Design Pattern: Módulo de Suporte — Encapsulamento de funções de rede.
// *
// *  Objetivo: Implementar funções de espera por IP e abertura de socket TCP para o broker MQTT.
// *
// *  Autor: DeepSeek
// *  Hardware: Raspberry Pi Pico W (RP2040)
// *  RTOS: Apache NuttX 13.0.0
// *
// *  Modelo: Módulo de Suporte (Categoria 1 da Taxonomia).
// *
// *  NOTA CONCEITUAL — RESOLUÇÃO DE NOME DO BROKER:
// *
// *    Este módulo encapsula as funções de rede necessárias para o aplicativo 'bmp280_mqtt'.
// *    A função 'network_aguardar_ip()' consulta a interface 'wlan0' usando 'netlib_get_ipv4addr()'
// *    e aguarda até que um endereço IPv4 seja atribuído. A função 'network_abrir_socket_broker()'
// *    resolve o nome do broker via DNS, cria um socket TCP não-bloqueante e conecta ao broker MQTT
// *    configurado.
// *
// *    A resolução de nome usa 'gethostbyname' em vez de 'getaddrinfo'. Justificativa didática:
// *
// *      - 'gethostbyname' é simples: recebe um nome, retorna uma estrutura 'hostent' com o endereço IP
// *        já resolvido em formato binário. É bloqueante — retorna somente após obter resposta (ou timeout)
// *        do servidor DNS. Isso é aceitável aqui, pois a resolução ocorre uma única vez por abertura de
// *        socket, e não há outra atividade concorrente no momento.
// *
// *      - 'getaddrinfo' é a API moderna, que suporta IPv4 e IPv6, 'hints' de família e tipo de socket, e
// *        múltiplos resultados em uma lista. É mais flexível, porém consideravelmente mais verbosa, e exige
// *        mais cuidado com a liberação da lista resultante ('freeaddrinfo').
// *
// *      - Como este aplicativo só precisa de um endereço IPv4 simples, 'gethostbyname' é o suficiente.
// *
// ********************************************************************************************************************/

/*
 * Inclusão de Diretivas de Compilação - Início ------------------------------------------------------------------------
 */

/*
 * Inclusão de Bibliotecas em Arquivos de Cabeçalho --------------------------------------------------------------------
 */
/*
 * Inclusão de Diretivas de Compilação - Início ------------------------------------------------------------------------
 */

/*
 * NuttX — Configuração do Kernel:
 */

// Inclui macros globais de configuração do Kernel NuttX, geradas pelo Kconfig.
#include <nuttx/config.h>

/*
 * C Libraries:
 */

// Inclui funções padrão do sistema como close e sleep
#include <unistd.h>

// Inclui a biblioteca padrão para funções como memset e memcpy
#include <string.h>

/*
 * POSIX:
 */

// Inclui a estrutura in_addr para armazenamento do endereço IPv4
#include <netinet/in.h>

// Inclui funções de conversão de endereços IP, como inet_addr.
// Nota didática: após a mudança para 'gethostbyname', o uso de 'inet_addr' deixou de existir neste arquivo.
// O include é mantido porque 'htons' (conversão de porta para network order) ainda é necessário, e pode ser
// declarado tanto em '<arpa/inet.h>' quanto em '<netinet/in.h>', dependendo da implementação.
#include <arpa/inet.h>

// Inclui a interface de resolução de nomes (gethostbyname, struct hostent)
#include <netdb.h>

// Inclui a interface de controle de dispositivos, necessária para a função ioctl()
#include <sys/ioctl.h>

/*
 * NuttX — APIs e utilitários:
 */

// Inclui funções de debug e logging do NuttX, como syslog()
#include <nuttx/debug.h>

// Inclui as funções netlib para consulta de status e endereço IPv4 da interface
#include <netutils/netlib.h>

/*
 * Módulos do aplicativo:
 */

// Inclui o header do módulo de rede
#include "network_utils.h"

// Necessário para dns_getserver()
#include <nuttx/net/dns.h>    

// Necessário para netlib_get_ipv4addr() — já presente
#include <netutils/netlib.h>   


/*
 * Definições Gerais ---------------------------------------------------------------------------------------------------
 */

// Nome DNS do broker MQTT (plataforma WEGnology).
// Nota didática: o nome anterior era 'ENDERECO_BROKER_MQTT', quando o valor era um IP literal ("192.168.0.36").
// Como agora o valor é um nome DNS, o identificador foi renomeado para 'NOME_BROKER_MQTT', refletindo com
// precisão o que a constante contém. Nomes que mentem sobre o conteúdo envelhecem mal.
#define NOME_BROKER_MQTT            "broker.app.wnology.io"

#define PORTA_BROKER_MQTT           1883                  // Define a porta TCP do broker MQTT

#define TEMPO_MAX_ESPERA_IP_S       60                    // Define o tempo máximo de espera para obter IP, em segundos

//______________________________________________________________________________________________________________________




// ------------------------------------------- Funções Públicas --------------------------------------------------------
 


//**********************************************************************************************************************
//* FUNCTION: int network_aguardar_ip(void)
//*
//* PARAMETERS: void
//*
//* DESCRIPTION:
//*   Aguarda até que a interface 'wlan0' possua um endereço IPv4 diferente de zero, com timeout de 60 segundos.
//*   A função consulta periodicamente o endereço usando 'netlib_get_ipv4addr()' e retorna sucesso assim que
//*   o endereço deixar de ser zero.
//*
//* RETURNS:
//*   OK (0) em caso de sucesso; -1 se o tempo de espera for excedido.
//*
//**********************************************************************************************************************
int network_aguardar_ip(void) {


    // Estrutura para armazenar o endereço IPv4 da interface
    struct in_addr endereco_ip;

    // Contador de tentativas de verificação de IP
    int tentativas = 0;

    // Número máximo de tentativas baseado no timeout definido
    int max_tentativas = TEMPO_MAX_ESPERA_IP_S;

    // Loop de espera pelo IP
    while (tentativas < max_tentativas) {

        // Inicializa o endereço com zero a cada iteração
        endereco_ip.s_addr = 0;

        // Salva código de retorno da função de obtenção do endereço IPv4 da interface 'wlan0'
        int ret = netlib_get_ipv4addr("wlan0",                        // Nome da interface de rede
                                      &endereco_ip);                    // Ponteiro para o endereço IPv4

        // Verifica se a consulta teve sucesso e se o IP é diferente de zero
        if (ret == OK && endereco_ip.s_addr != 0) {

            // Retorna sucesso ao chamador
            return OK;

        } // fim if

        // Aguarda 1 segundo antes da próxima tentativa
        sleep(1);

        // Incrementa o contador de tentativas
        tentativas++;

    } // fim while

    // Se chegou aqui, o tempo esgotou sem obter IP
    syslog(LOG_ERR, "[network_aguardar_ip]: Tempo esgotado aguardando IP da 'wlan0'\n");

    // Retorna erro indicando timeout
    return -1;

} // fim função int network_aguardar_ip(void)



//______________________________________________________________________________________________________________________




//**********************************************************************************************************************
//* FUNCTION: int network_abrir_socket_broker(void)
//*
//* PARAMETERS: void
//*
//* DESCRIPTION:
//*   Resolve o nome DNS do broker MQTT, abre um socket TCP não-bloqueante e conecta ao broker na porta definida
//*   por 'PORTA_BROKER_MQTT'. O socket é criado com 'socket()', conectado com 'connect()' e configurado como
//*   não-bloqueante com 'ioctl()' e 'FIONBIO'.
//*
//*   Diagnóstico: antes da resolução DNS, o endereço IPv4 atual da interface 'wlan0' é registrado no log. Este
//*   log existe apenas para diagnosticar a falha noturna de reconexão e pode ser removido quando o problema
//*   for resolvido.
//*
//* RETURNS:
//*   Descritor de socket em caso de sucesso; -1 em caso de falha.
//*
//**********************************************************************************************************************
int network_abrir_socket_broker(void) {

    // Estrutura para armazenar as informações do broker
    struct sockaddr_in broker;

    // Estrutura com o resultado da resolução DNS do broker
    struct hostent *host;

    // Descritor do socket TCP
    int fd_socket;

    // Variável para configurar o modo não-bloqueante via ioctl
    int modo_nao_bloqueante = 1;

    // Cópia local em stack do nome do broker (protege contra corrupção de .rodata)
    char nome_broker[64];

    // Estrutura para o diagnóstico do endereço IPv4 da interface
    struct in_addr ip_check;

    // Código de retorno da consulta de diagnóstico
    int ip_ok;

    /*
     * Cópia local do nome do broker:
     */

    // Copia o nome DNS do broker para a stack
    strncpy(nome_broker, NOME_BROKER_MQTT, sizeof(nome_broker) - 1);

    // Garante terminação nula explícita
    nome_broker[sizeof(nome_broker) - 1] = '\0';

    /*
     * Diagnóstico de rede:
     *
     * Registra o endereço IPv4 atual da interface Wi-Fi antes de tentar resolver o nome do broker. Se a
     * 'wlan0' estiver sem IP, o 'gethostbyname' abaixo falhará — e este log identifica a causa raiz.
     *
     * Este log é temporário: existe apenas para diagnosticar a falha noturna de reconexão. Pode ser
     * removido quando o problema for resolvido ou quando a escada de resiliência estiver implementada.
     */

    // Consulta o endereço IPv4 atual da interface Wi-Fi
    ip_check.s_addr = 0;
    ip_ok = netlib_get_ipv4addr("wlan0", &ip_check);
    syslog(LOG_INFO, "[DBG] wlan0: ret=%d ip=0x%08x\n", ip_ok, (unsigned)ip_check.s_addr);

    /*
     * Resolve o nome do broker via DNS:
     */

    // Log antes da resolução (mostra a cópia em stack)
    syslog(LOG_INFO, "[DBG] antes gethostbyname (stack):  '%s'\n", nome_broker);

    // Converte o nome DNS do broker em endereço IP.
    // Nota didática: 'gethostbyname' é bloqueante (espera resposta do DNS ou timeout) e retorna um ponteiro
    // para uma estrutura 'hostent' de armazenamento estático interno — ou seja, uma chamada subsequente a
    // 'gethostbyname' sobrescreveria o resultado anterior. Também não é thread-safe. Como neste arquivo
    // chamamos a função uma única vez, imediatamente copiamos o endereço para a estrutura local 'broker' e
    // nunca mais usamos 'host', o uso é seguro.
    host = gethostbyname(nome_broker);

    // Log depois da resolução (mostra as duas strings: cópia em stack e .rodata original)
    syslog(LOG_INFO, "[DBG] depois gethostbyname (stack):  '%s'\n", nome_broker);
    syslog(LOG_INFO, "[DBG] depois gethostbyname (rodata): '%s'\n", NOME_BROKER_MQTT);

    // Verifica se a resolução DNS falhou
    if (host == NULL) {

        // Registra o erro no syslog usando a cópia em stack (sempre íntegra)
        syslog(LOG_ERR, "[network_abrir_socket_broker]: Falha ao resolver DNS de '%s'\n", nome_broker);

        // Retorna erro
        return -1;

    } // fim if

    /*
     * Inicializa endereço do broker:
     */

    // Inicializa a estrutura com zeros
    memset(&broker,                                             // Ponteiro para a estrutura do broker
           0,                                                   // Valor usado para preencher a memória
           sizeof(broker));                                     // Quantidade de bytes a serem preenchidos

    // Define a família de endereços como IPv4
    broker.sin_family = AF_INET;

    // Define a porta do broker em formato de rede (big-endian)
    broker.sin_port = htons(PORTA_BROKER_MQTT);

    // Copia o endereço IP resolvido para a estrutura do broker.
    // Nota didática: 'h_addr' é um macro de compatibilidade definido em '<netdb.h>' do NuttX como
    // '#define h_addr h_addr_list[0]'. Ele existe para código legado escrito antes da padronização POSIX
    // que introduziu 'h_addr_list'. Ambas as formas são equivalentes.
    memcpy(&broker.sin_addr,                                    // Destino: campo de endereço do broker
           host->h_addr,                                        // Origem: primeiro endereço IPv4 retornado
           host->h_length);                                     // Tamanho do endereço em bytes

    /*
     * Trata a criação do socket:
     */

    // Cria o socket TCP
    fd_socket = socket(AF_INET,                                 // Família de endereços IPv4
                       SOCK_STREAM,                             // Tipo de socket: fluxo de dados confiável
                       0);                                      // Protocolo padrão para TCP/IP

    // Verifica se a criação do socket falhou
    if (fd_socket < 0) {

        // Registra o erro no syslog
        syslog(LOG_ERR, "[network_abrir_socket_broker]: Falha ao criar socket: %d\n", fd_socket);

        // Retorna erro
        return -1;

    } // fim if

    /*
     * Trata a conexão:
     */

    // Conecta ao broker
    if (connect(fd_socket,                                      // Descritor do socket
                (struct sockaddr *)&broker,                     // Ponteiro para o endereço do broker
                sizeof(broker)) < 0)                            // Tamanho da estrutura de endereço
    {

        // Fecha o socket em caso de falha na conexão
        close(fd_socket);

        // Registra o erro no syslog
        syslog(LOG_ERR, "[network_abrir_socket_broker]: Falha ao conectar ao broker\n");

        // Retorna erro
        return -1;

    } // fim if

    /*
     * Configura modo não-bloqueante do socket:
     */

    // Torna o socket não-bloqueante usando ioctl (compatível com NuttX)
    if (ioctl(fd_socket,                                        // Descritor do socket
              FIONBIO,                                          // Comando para alterar modo não-bloqueante
              &modo_nao_bloqueante) < 0)                        // Ponteiro para o valor de configuração
    {

        // Se falhar, registra o erro e fecha o socket
        close(fd_socket);

        // Registra o erro no syslog
        syslog(LOG_ERR, "[network_abrir_socket_broker]: Falha ao definir socket não-bloqueante\n");

        // Retorna erro
        return -1;

    } // fim if

    // Retorna o descritor do socket conectado e configurado
    return fd_socket;

} // fim função int network_abrir_socket_broker(void)


//----------------------------------------------------------------------------------------------------------------------
// Fim do arquivo 'network_utils.c'
//----------------------------------------------------------------------------------------------------------------------
