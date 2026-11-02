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
// *  NOTA CONCEITUAL:
// *    Este módulo encapsula as funções de rede necessárias para o aplicativo 'alive_mqtt'.
// *    A função 'network_aguardar_ip()' consulta a interface 'wlan0' usando 'netlib_get_ipv4addr()'
// *    e aguarda até que um endereço IPv4 seja atribuído. A função 'network_abrir_socket_broker()'
// *    cria um socket TCP não-bloqueante e conecta ao broker MQTT configurado.
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

// Inclui a biblioteca padrão para funções como memset
#include <string.h>

/*
 * POSIX:
 */

// Inclui a estrutura in_addr para armazenamento do endereço IPv4
#include <netinet/in.h>

// Inclui funções de conversão de endereços IP, como inet_addr
#include <arpa/inet.h>

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


/*
 * Definições Gerais ---------------------------------------------------------------------------------------------------
 */

#define ENDERECO_BROKER_MQTT        "192.168.0.36"        // Define o endereço IP do broker MQTT (Mosquitto)

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
//*   Abre um socket TCP não-bloqueante e conecta ao broker MQTT definido por 'ENDERECO_BROKER_MQTT' e
//*   'PORTA_BROKER_MQTT'. O socket é criado com 'socket()', conectado com 'connect()' e configurado
//*   como não-bloqueante com 'ioctl()' e 'FIONBIO'.
//*
//* RETURNS:
//*   Descritor de socket em caso de sucesso; -1 em caso de falha.
//*
//**********************************************************************************************************************
int network_abrir_socket_broker(void) {

    // Estrutura para armazenar as informações do broker
    struct sockaddr_in broker;

    // Descritor do socket TCP
    int fd_socket;

    // Variável para configurar o modo não-bloqueante via ioctl
    int modo_nao_bloqueante = 1;

    /*
    * Inicializa endereço do broker:
    */

    // Inicializa a estrutura com zeros
    memset(&broker,                                             // Ponteiro para a estrutura do broker
           0,                                                   // Valor usado para preencher a memória
           sizeof(broker));                                     // Quantidade de bytes a serem preenchidos

    // Define a família de endereços como IPv4
    broker.sin_family = AF_INET;

    // Define a porta do broker em formato de rede
    broker.sin_port = htons(PORTA_BROKER_MQTT);

    // Converte o endereço IP textual para binário
    broker.sin_addr.s_addr = inet_addr(ENDERECO_BROKER_MQTT);


    /*
    * Trata a criação do socket:
    */

    // Cria o socket TCP
    fd_socket = socket(AF_INET,                               // Família de endereços IPv4
                       SOCK_STREAM,                             // Tipo de socket: fluxo de dados confiável
                       0);                                  // Protocolo padrão para TCP/IP

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
    if (connect(fd_socket,                                   // Descritor do socket
                (struct sockaddr *)&broker,                    // Ponteiro para o endereço do broker
                sizeof(broker)) < 0)                        // Tamanho da estrutura de endereço
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
    if (ioctl(fd_socket,                                         // Descritor do socket
              FIONBIO,                                          // Comando para alterar modo não-bloqueante
              &modo_nao_bloqueante) < 0)                             // Ponteiro para o valor de configuração
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