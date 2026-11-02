//######################################################################################################################
// *
// *  Arquivo: "mqtt_wnology.c"
// *
// *  Título: Gerenciamento do Ciclo de Vida MQTT para a Plataforma WEGnology
// *
// *  Design Pattern: Módulo Gerenciador — Encapsulamento da sequência de conexão, reconexão e publicação.
// *
// *  Objetivo: Gerenciar o ciclo de vida da sessão MQTT (rede → socket → conexão → reconexão) para a plataforma
// *            WEGnology, utilizando os módulos auxiliares 'network_utils' e 'mqtt_utils'.
// *
// *  Hardware: Raspberry Pi Pico W (RP2040)
// *  RTOS: Apache NuttX 13.0.0
// *
// *  Modelo: Módulo Gerenciador (Categoria 1 da Taxonomia).
// *
// *  NOTA DIDÁTICA — NÍVEIS DE SEVERIDADE DO 'syslog':
// *
// *    O NuttX segue a convenção POSIX para severidades de 'syslog', em ordem crescente de gravidade:
// *
// *      LOG_DEBUG   — depuração detalhada
// *      LOG_INFO    — informação geral (progresso, etapas em andamento)
// *      LOG_NOTICE  — evento normal, mas digno de nota (mudança de estado, conexão estabelecida)
// *      LOG_WARNING — algo inesperado, mas recuperável (recurso opcional indisponível)
// *      LOG_ERR     — falha que compromete o funcionamento esperado
// *      LOG_CRIT    — falha crítica
// *      LOG_ALERT   — requer ação imediata
// *      LOG_EMERG   — sistema inutilizável
// *
// *  NOTA CONCEITUAL — ESTRATÉGIA DE RESILIÊNCIA EM CAMADAS:
// *
// *    O ambiente de rede noturno (provedor, roteador) sofre eventos de queda que derrubam a conexão MQTT e
// *    deixam o resolver DNS inoperante. Para sobreviver a isso sem intervenção humana, a aplicação usa uma
// *    escada de resiliência:
// *
// *      Nível 0 (Padrão): Tenta reconectar (fecha socket, aguarda IP, abre novo socket, conecta).
// *
// *      Nível 1 (Reset de Interface): Após 3 falhas, força 'netlib_ifdown' + 'netlib_ifup'. Isso derruba o
// *      Wi-Fi, reassocia e força um novo DHCP. Resolve 90% dos casos onde a placa perdeu a rota.
// *
// *      Nível 2 (Hardware Reset): Se o Nível 1 falhar duas vezes seguidas (6 falhas totais), o problema é
// *      mais profundo (firmware do chip Wi-Fi zumbi, por exemplo). A placa reinicia via 'boardctl'.
// *
// *    Além disso, a aplicação força o uso do DNS público do Google (8.8.8.8) na inicialização. O roteador
// *    doméstico, após eventos de rede, frequentemente repassa um servidor DNS do provedor que está morto.
// *    Forçar o 8.8.8.8 contorna a falha do provedor.
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

// Inclui o tipo booleano nativo (true/false)
#include <stdbool.h>

// Inclui funções padrão do sistema como close e sleep
#include <unistd.h>

// Inclui funções de memória, como memset
#include <string.h>


/*
 * POSIX:
 */

// Inclui estruturas de rede e a função inet_addr
#include <arpa/inet.h>
#include <netinet/in.h>


/*
 * NuttX — APIs e utilitários:
 */

// Inclui funções de debug e logging do NuttX, como syslog()
#include <nuttx/debug.h>

// Necessário para 'dns_add_nameserver()'
#include <nuttx/net/dns.h>

// Necessário para 'netlib_ifdown()' e 'netlib_ifup()'
#include <netutils/netlib.h>

// Necessário para 'boardctl(BOARDIOC_RESET, 0)'
#include <sys/boardctl.h>


/*
 * Módulos do aplicativo:
 */

// Inclui funções de utilitários de rede (aguardar IP, abrir socket)
#include "network_utils.h"

// Inclui funções de utilitários MQTT (inicializar, conectar, publicar, sincronizar, LED)
#include "mqtt_utils.h"

// Inclui a interface do próprio módulo gerenciador
#include "mqtt_wnology.h"


/*
 * Definições Gerais ---------------------------------------------------------------------------------------------------
 */

// Define o tempo extra de espera após obter IP, em segundos
#define TEMPO_ESTABILIZACAO_ROTA_S                  2

// Define o tempo de espera entre tentativas de conexão ou reconexão, em segundos
#define TEMPO_ENTRE_RETENTATIVAS_S                  5

// Define o número de tentativas para verificar CONNACK após conectar
#define TENTATIVAS_VERIFICACAO_CONNACK              50

// Define o intervalo, em microssegundos, entre tentativas de verificação de CONNACK
#define INTERVALO_VERIFICACAO_CONNACK_US            100000

// Limite de falhas consecutivas antes de forçar o reset da interface de rede
#define LIMITE_FALHAS_ANTES_RESET                   3

// Limite de resets de interface consecutivos antes de reiniciar o hardware
#define LIMITE_RESETS_ANTES_REBOOT                  2

// Define o Device ID da plataforma WEGnology (usado como 'client_id' no CONNECT).
#define DEVICE_ID_WNOLOGY                           "6ab965cd0272b530b40c35f4"

// Define a Access Key da plataforma WEGnology (usada como 'user_name' no CONNECT).
#define ACCESS_KEY_WNOLOGY                          "a31beecc-82b7-4a97-8b64-9750c9f2c250"

// Define a Access Secret da plataforma WEGnology (usada como 'password' no CONNECT).
#define ACCESS_SECRET_WNOLOGY                       "89dc9c17ac5f80c651856dafe034dd1a66a24c675c4b2842e32b1657c37ed943"


//______________________________________________________________________________________________________________________


/*
 * Variáveis Globais (Estáticas) ---------------------------------------------------------------------------------------
 */

// Descritor do socket conectado ao broker MQTT; -1 quando não há conexão ativa.
static int s_fd_socket = -1;


//______________________________________________________________________________________________________________________


/*
// -------------------------------------------- Funções Públicas -------------------------------------------------------
 */


//**********************************************************************************************************************
//* FUNCTION: int mqtt_wnology_iniciar(volatile bool *terminar)
//*
//* PARAMETERS: volatile bool *terminar = ponteiro para a flag de término por Ctrl+C do aplicativo cliente.
//*
//* DESCRIPTION:
//*
//*   Executa a sequência completa de inicialização:
//*
//*     1. Configura o DNS estático (Google) para contornar falhas do roteador local.
//*     2. Aguarda a interface 'wlan0' obter um endereço IPv4 válido.
//*     3. Aguarda um tempo extra para a rota ser estabelecida.
//*     4. Abre o dispositivo do LED externo (tolerante a falha).
//*     5. Abre um socket TCP com o broker MQTT, com retentativa e verificação da flag de término.
//*     6. Inicializa o cliente MQTT-C com o socket aberto.
//*     7. Estabelece a sessão MQTT usando as credenciais da plataforma WEGnology.
//*     8. Aguarda a confirmação de conexão (CONNACK).
//*
//* RETURNS: OK (0) em caso de sucesso; -1 em caso de falha ou interrupção.
//*
//**********************************************************************************************************************
int mqtt_wnology_iniciar(volatile bool *terminar) {

    // Estrutura para configurar o servidor DNS
    struct sockaddr_in dns_config;

    // Código de retorno da configuração do DNS
    int ret_dns;

    /*
    * Força o uso do DNS público do Google (8.8.8.8):
    *
    *    Roteadores domésticos frequentemente repassam servidores DNS do provedor que caem durante a madrugada.
    *    Como o DHCP é renovado com o mesmo IP e o mesmo DNS morto, o resolver do NuttX fica preso tentando
    *    consultar um servidor inacessível. Forçar o Google DNS na inicialização contorna essa falha.
    */

    // Zera a estrutura de configuração do DNS
    memset(&dns_config, 0, sizeof(dns_config));

    // Define a família de endereços como IPv4
    dns_config.sin_family = AF_INET;

    // Define a porta padrão do DNS (53)
    dns_config.sin_port = htons(53);

    // Define o endereço IP do Google DNS (8.8.8.8)
    dns_config.sin_addr.s_addr = inet_addr("8.8.8.8");

    // Registra o DNS estático no resolver do NuttX
    ret_dns = dns_add_nameserver((FAR struct sockaddr *)&dns_config, sizeof(dns_config));

    // Verifica se o registro do DNS falhou
    if (ret_dns < 0) {

        // Log de aviso (não impede a inicialização, mas pode causar falhas de conexão)
        syslog(LOG_WARNING, "[mqtt_wnology_iniciar]: Falha ao configurar DNS 8.8.8.8\n");

    } else {

        // Log informativo confirmando a configuração do DNS
        syslog(LOG_INFO, "[mqtt_wnology_iniciar]: DNS publico (8.8.8.8) configurado.\n");

    } // fim if


    /*
    * Aguarda endereço IP:
    */

    // Aguarda até que a interface 'wlan0' obtenha um endereço IPv4 válido
    if (network_aguardar_ip() != OK) {

        // Log de timeout
        syslog(LOG_ERR, "[mqtt_wnology_iniciar]: Tempo esgotado aguardando IP da 'wlan0'\n");

        // Retorna erro
        return -1;

    } // fim if

    // Aguarda um tempo extra para a rota da interface ser estabelecida
    sleep(TEMPO_ESTABILIZACAO_ROTA_S);

    /*
    * Abre o LED externo (tolerante a falha):
    */

    // Tenta abrir o dispositivo do LED externo; se falhar, segue apenas com MQTT
    mqtt_abrir_led();

    /*
    * Abre o socket TCP com o broker, com retentativa:
    */

    // Tenta abrir a conexão TCP com o broker MQTT; se falhar, aguarda e tenta novamente
    while ((s_fd_socket = network_abrir_socket_broker()) < 0) {

        // Se o aplicativo foi interrompido, encerra a inicialização
        if (*terminar) {

            // Log informativo (evento de estado: interrupção solicitada pelo usuário)
            syslog(LOG_NOTICE, "[mqtt_wnology_iniciar]: Interrompido durante abertura de socket\n");

            // Fecha o LED se estiver aberto
            mqtt_fechar_led();

            // Retorna erro
            return -1;

        } // fim if

        // Log de progresso (aguardando nova tentativa de conexão)
        syslog(LOG_INFO, "[mqtt_wnology_iniciar]: Aguardando conexão com o Broker MQTT...\n");

        // Aguarda antes da próxima tentativa
        sleep(TEMPO_ENTRE_RETENTATIVAS_S);

    } // fim while

    /*
    * Inicializa o cliente MQTT-C:
    */

    // Inicializa o cliente MQTT-C com o socket, buffers e callback
    mqtt_inicializar(s_fd_socket);

    /*
    * Estabelece a sessão MQTT com o broker:
    */

    // Conecta ao broker usando o Device ID e as credenciais da plataforma WEGnology
    if (mqtt_conectar(DEVICE_ID_WNOLOGY, ACCESS_KEY_WNOLOGY, ACCESS_SECRET_WNOLOGY) != MQTT_OK) {

        // Log de falha
        syslog(LOG_ERR, "[mqtt_wnology_iniciar]: Falha na conexão MQTT: %s\n", mqtt_erro_str());

        // Fecha o socket
        close(s_fd_socket);

        // Marca o socket como fechado
        s_fd_socket = -1;

        // Fecha o LED
        mqtt_fechar_led();

        // Retorna erro
        return -1;

    } // fim if

    /*
    * Aguarda confirmação de conexão (CONNACK):
    */

    // Confirma que o evento de conexão foi recebido
    if (!mqtt_verificar_conexao(TENTATIVAS_VERIFICACAO_CONNACK, INTERVALO_VERIFICACAO_CONNACK_US)) {

        // Log de falha
        syslog(LOG_ERR, "[mqtt_wnology_iniciar]: Tempo esgotado aguardando CONNACK\n");

        // Fecha o socket
        close(s_fd_socket);

        // Marca o socket como fechado
        s_fd_socket = -1;

        // Fecha o LED
        mqtt_fechar_led();

        // Retorna erro
        return -1;

    } // fim if

    // Log de sucesso (evento de estado: mudança de "desconectado" para "conectado")
    syslog(LOG_NOTICE, "[mqtt_wnology_iniciar]: Conexão com o Broker MQTT estabelecida!\n");

    // Retorna sucesso
    return OK;

} // fim função int mqtt_wnology_iniciar(volatile bool *terminar)


//______________________________________________________________________________________________________________________


//**********************************************************************************************************************
//* FUNCTION: void mqtt_wnology_finalizar(void)
//*
//* PARAMETERS: void
//*
//* DESCRIPTION: Encerra a sessão MQTT, fecha o socket TCP e o LED externo.
//*
//* RETURNS: void
//*
//**********************************************************************************************************************
void mqtt_wnology_finalizar(void) {

    // Verifica se o socket está aberto e o cliente MQTT está saudável
    if (s_fd_socket >= 0 && mqtt_ok()) {
        // Tenta enviar o pacote DISCONNECT ao broker
        mqtt_desconectar();
    } // fim if

    // Verifica se o socket está aberto
    if (s_fd_socket >= 0) {
        // Fecha o socket
        close(s_fd_socket);

        // Marca o socket como fechado
        s_fd_socket = -1;
    } // fim if

    // Fecha o LED externo
    mqtt_fechar_led();

} // fim função void mqtt_wnology_finalizar(void)


//______________________________________________________________________________________________________________________


//**********************************************************************************************************************
//* FUNCTION: void mqtt_wnology_sincronizar(void)
//*
//* DESCRIPTION: Wrapper para processar tráfego MQTT.
//*
//**********************************************************************************************************************
void mqtt_wnology_sincronizar(void) {
    // Processa tráfego MQTT de entrada e saída
    mqtt_sincronizar();
} // fim função void mqtt_wnology_sincronizar(void)


//______________________________________________________________________________________________________________________


//**********************************************************************************************************************
//* FUNCTION: bool mqtt_wnology_ok(void)
//*
//* DESCRIPTION: Wrapper para verificar o estado do cliente MQTT.
//*
//**********************************************************************************************************************
bool mqtt_wnology_ok(void) {
    // Retorna true se o cliente estiver saudável
    return mqtt_ok();
} // fim função bool mqtt_wnology_ok(void)


//______________________________________________________________________________________________________________________


//**********************************************************************************************************************
//* FUNCTION: int mqtt_wnology_reconectar(volatile bool *terminar)
//*
//* PARAMETERS: terminar: ponteiro para a flag de término por Ctrl+C do aplicativo cliente.
//*
//* DESCRIPTION:
//*
//*   Tenta restabelecer a conexão MQTT. Possui uma escada de resiliência:
//*     1. Após 3 falhas, reinicia a interface 'wlan0' (ifdown/ifup).
//*     2. Após 2 resets de interface falhos (6 falhas totais), reinicia o hardware (boardctl).
//*
//* RETURNS: OK (0) em caso de sucesso; -1 em caso de falha ou interrupção.
//*
//**********************************************************************************************************************
int mqtt_wnology_reconectar(volatile bool *terminar) {

    // Contador estático de falhas consecutivas (preserva o valor entre chamadas)
    static int s_falhas_consecutivas = 0;

    // Contador estático de resets de interface consecutivos
    static int s_contador_resets = 0;

    /*
    * Fecha o socket atual, se estiver aberto:
    */

    // Verifica se há socket aberto
    if (s_fd_socket >= 0) {
        // Fecha o socket antigo
        close(s_fd_socket);

        // Marca o socket como fechado
        s_fd_socket = -1;
    } // fim if

    /*
    * Reset da interface de rede após N falhas consecutivas:
    */

    // Verifica se o limite de falhas foi atingido
    if (s_falhas_consecutivas >= LIMITE_FALHAS_ANTES_RESET) {

        // Incrementa o contador de resets
        s_contador_resets++;

        // Se já tentou resetar a interface antes e o problema persistiu, reinicia o hardware
        if (s_contador_resets >= LIMITE_RESETS_ANTES_REBOOT) {

            // Log crítico avisando sobre o reboot iminente
            syslog(LOG_EMERG, "[mqtt_wnology_reconectar]: Falha persistente apos resets. Reiniciando sistema...\n");

            // Aguarda 2 segundos para garantir que a mensagem de log seja enviada pela serial
            sleep(2);

            // Reinicia o microcontrolador (NuttX Board Control Reset)
            boardctl(BOARDIOC_RESET, 0);

            // Nota: a execução não chega aqui. O sistema reinicia.

        } // fim if

        // Registra o reset de interface no log
        syslog(LOG_WARNING, "[mqtt_wnology_reconectar]: %d falhas, resetando wlan0 (tentativa %d)\n",
               s_falhas_consecutivas, s_contador_resets);

        // Derruba a interface Wi-Fi
        netlib_ifdown("wlan0");

        // Aguarda o firmware processar a desassociação
        sleep(2);

        // Reergue a interface Wi-Fi
        netlib_ifup("wlan0");

        // Aguarda reassociação e renovação DHCP
        sleep(5);

        // Zera o contador de falhas após o reset — a próxima tentativa será com estado limpo
        s_falhas_consecutivas = 0;

    } // fim if

    /*
    * Garante que a interface tenha IP antes de prosseguir:
    */

    // Verifica se o usuário pediu parada antes de esperar
    if (*terminar) {

        // Retorna erro sem tentar
        return -1;

    } // fim if

    // Aguarda IP válido na 'wlan0' (timeout interno de 60 s)
    if (network_aguardar_ip() != OK) {

        // Sem IP: incrementa o contador e retorna
        syslog(LOG_WARNING, "[mqtt_wnology_reconectar]: wlan0 sem IP após timeout\n");

		//
        s_falhas_consecutivas++;

		//
        return -1;

    } // fim if

    /*
    * Abre um novo socket TCP com o broker, com retentativa:
    */

    // Tenta abrir novo socket com o broker
    while ((s_fd_socket = network_abrir_socket_broker()) < 0) {

        // Se o aplicativo foi interrompido, encerra a reconexão
        if (*terminar) {

            // Retorna erro
            return -1;

        } // fim if

        // Registra a falha e incrementa o contador
        syslog(LOG_INFO, "[mqtt_wnology_reconectar]: Aguardando reconexão com o Broker...\n");

		//
        s_falhas_consecutivas++;

        // Aguarda antes da próxima tentativa
        sleep(TEMPO_ENTRE_RETENTATIVAS_S);

        // Se o limite foi atingido durante as retentativas, sai para o reset
        if (s_falhas_consecutivas >= LIMITE_FALHAS_ANTES_RESET) {

            // Retorna erro; o chamador tentará de novo e o reset acontecerá
            return -1;

        } // fim if

    } // fim while

    /*
    * Reinicializa o cliente MQTT-C com o novo socket:
    */

    // Reinicializa o cliente MQTT-C com o novo socket
    mqtt_reinicializar(s_fd_socket);

    /*
    * Reconecta ao broker:
    */

    // Conecta ao broker usando o Device ID e as credenciais da plataforma WEGnology
    if (mqtt_conectar(DEVICE_ID_WNOLOGY, ACCESS_KEY_WNOLOGY, ACCESS_SECRET_WNOLOGY) != MQTT_OK) {

        // Registra a falha no log
        syslog(LOG_ERR, "[mqtt_wnology_reconectar]: Falha na reconexão MQTT: %s\n", mqtt_erro_str());

        // Fecha o socket recém-aberto
        close(s_fd_socket);

		//
        s_fd_socket = -1;

        // Incrementa o contador de falhas
        s_falhas_consecutivas++;

        // Aguarda antes da próxima tentativa
        sleep(TEMPO_ENTRE_RETENTATIVAS_S);

        // Retorna erro
        return -1;

    } // fim if

    /*
    * Aguarda confirmação de conexão (CONNACK):
    */

    // Confirma que o evento de conexão foi recebido
    if (!mqtt_verificar_conexao(TENTATIVAS_VERIFICACAO_CONNACK, INTERVALO_VERIFICACAO_CONNACK_US)) {

        // Registra a falha no log
        syslog(LOG_ERR, "[mqtt_wnology_reconectar]: Tempo esgotado aguardando CONNACK\n");

        // Fecha o socket recém-aberto
        close(s_fd_socket);

		//
        s_fd_socket = -1;

        // Incrementa o contador de falhas
        s_falhas_consecutivas++;

        // Aguarda antes da próxima tentativa
        sleep(TEMPO_ENTRE_RETENTATIVAS_S);

        // Retorna erro
        return -1;

    } // fim if

    /*
    * Sucesso: zera os contadores de falhas e resets:
    */

    // Zera o contador de falhas
    s_falhas_consecutivas = 0;

    // Zera o contador de resets (a conexão foi restabelecida com sucesso)
    s_contador_resets = 0;

    // Registra o sucesso no log
    syslog(LOG_NOTICE, "[mqtt_wnology_reconectar]: Reconexão com o Broker MQTT estabelecida!\n");

    // Retorna sucesso
    return OK;

} // fim função int mqtt_wnology_reconectar(volatile bool *terminar)


//______________________________________________________________________________________________________________________


//**********************************************************************************************************************
//* FUNCTION: int mqtt_wnology_publicar(const char *payload, size_t tamanho)
//*
//* DESCRIPTION: Wrapper para publicar dados no broker MQTT.
//*
//**********************************************************************************************************************
int mqtt_wnology_publicar(const char *payload, size_t tamanho) {

    // Publica o payload via módulo MQTT
    return mqtt_publicar(payload, tamanho);

} // fim função int mqtt_wnology_publicar(const char *payload, size_t tamanho)


//______________________________________________________________________________________________________________________


//**********************************************************************************************************************
//* FUNCTION: const char *mqtt_wnology_erro_str(void)
//*
//* DESCRIPTION: Wrapper para obter a string de erro do cliente MQTT.
//*
//**********************************************************************************************************************
const char *mqtt_wnology_erro_str(void) {

    // Retorna a string de erro do cliente MQTT
    return mqtt_erro_str();

} // fim função const char *mqtt_wnology_erro_str(void)


//______________________________________________________________________________________________________________________


//----------------------------------------------------------------------------------------------------------------------
// Fim do arquivo 'mqtt_wnology.c'
//----------------------------------------------------------------------------------------------------------------------
