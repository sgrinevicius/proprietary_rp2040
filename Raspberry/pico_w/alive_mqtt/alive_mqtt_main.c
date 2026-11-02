//######################################################################################################################
// *
// *  Arquivo: "alive_mqtt_main.c"
// *
// *  Título: Orquestração do Sinal de Vida MQTT para Raspberry Pi Pico W
// *
// *  Design Pattern: Serviço de Supervisão — Orquestração da publicação periódica com cliente MQTT-C e controle de LED.
// *
// *  Objetivo: Gerenciar o fluxo principal do aplicativo, aguardando rede, abrindo conexão MQTT e executando o loop
// *            de publicação. Utiliza módulos auxiliares para rede e MQTT.
// *
// *  Autor: DeepSeek
// *  Hardware: Raspberry Pi Pico W (RP2040)
// *  RTOS: Apache NuttX 13.0.0
// *
// *  Modelo: Aplicação de Usuário (Categoria 1 da Taxonomia).
// *
// *  NOTA CONCEITUAL — VIA DE ACESSO:
// *
// *    Este arquivo é o ponto de entrada do aplicativo. Ele chama funções de 'network_utils' e 'mqtt_utils',
// *    mantendo a separação de responsabilidades.
// *
// *--------------------------------------------------------------------------------------------------------------------
// *
// *    Fluxo de chamadas (runtime):
// *
// *      APLICAÇÃO (este arquivo)
// *        │
// *        ├── network_aguardar_ip()                    [network_utils]
// *        │
// *        ├── network_abrir_socket_broker()            [network_utils]
// *        │
// *        ├── mqtt_inicializar()                       [mqtt_utils]
// *        │
// *        ├── mqtt_conectar()                          [mqtt_utils]
// *        │
// *        ├── mqtt_publicar()                          [mqtt_utils]
// *        │
// *        └── mqtt_verificar_conexao()                 [mqtt_utils]
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

// Inclui macros globais de configuração do Kernel NuttX, geradas pelo Kconfig
#include <nuttx/config.h>

/*
 * C Libraries:
 */

// Inclui o tipo booleano nativo (true/false) do C23
#include <stdbool.h>

// Inclui funções padrão do sistema como close e sleep
#include <unistd.h>

/*
 * POSIX:
 */

// Inclui o tratamento de sinais para interrupção graciosa com Ctrl+C
#include <signal.h>

/*
 * NuttX — APIs e utilitários:
 */

// Inclui funções de debug e logging do NuttX, como syslog()
#include <nuttx/debug.h>

/*
 * Módulos do aplicativo:
 */

// Inclui funções de utilitários de rede (aguardar IP, abrir socket)
#include "network_utils.h"

// Inclui funções de utilitários MQTT (inicializar, conectar, publicar)
#include "mqtt_utils.h"


/*
 * Definições Gerais ---------------------------------------------------------------------------------------------------
 */

#define INTERVALO_PUBLICACAO_S              10              // Define o intervalo em segundos entre as publicações

#define TEMPO_ESTABILIZACAO_ROTA_S          2               // Define o tempo extra de espera após obter IP, em segundos

//______________________________________________________________________________________________________________________


/*
 * Variáveis Globais ---------------------------------------------------------------------------------------------------
 */

// Flag de término por Ctrl+C definida pelo handler de SIGINT para encerrar o loop principal de forma graciosa.
static volatile bool s_terminar_execucao = false;

//______________________________________________________________________________________________________________________



// --------------------------------------------- Funções Privadas ------------------------------------------------------




//**********************************************************************************************************************
//* FUNCTION: static void tratador_sigint(int sinal)
//*
//* PARAMETERS: int sinal = número do sinal recebido (esperado SIGINT).
//*
//* DESCRIPTION:
//*
//* Handler registrado para o sinal SIGINT (Ctrl+C).
//*
//* Por que esta função existe:
//*
//* A API 'signal()' exige um ponteiro para função. Não é possível registrar diretamente uma variável ou uma expressão.
//* Portanto, mesmo que a ação desejada seja apenas alterar uma flag, é necessário envolver essa ação em uma função.
//*
//* Por que o corpo é tão pequeno:
//*
//* Handlers de sinal devem executar o mínimo possível. Eles podem interromper o fluxo do programa em um momento
//* arbitrário. Por isso, é inseguro realizar operações complexas, como I/O, alocação de memória ou travamento de mutex.
//* A prática segura é apenas sinalizar o evento por meio de uma variável volatile e deixar que o loop principal trate o
//* término de forma graciosa.
//*
//* RETURNS:
//* void
//*
//**********************************************************************************************************************
static void tratador_sigint(int sinal) {

    // Sinaliza ao loop principal que a execução deve terminar. É volatile bool para garantir visibilidade imediata.
    s_terminar_execucao = true;

} // fim função static void tratador_sigint(int sinal)


//______________________________________________________________________________________________________________________



/*
 ********************************************** Rotina Principal *******************************************************
 */



//**********************************************************************************************************************
//* FUNCTION: int main(int argc, FAR char *argv[])
//*
//* PARAMETERS: int argc = número de argumentos, FAR char *argv[] = vetor de argumentos
//*
//* DESCRIPTION: Função principal. Aguarda a interface wlan0 obter IP, abre o LED externo, conecta ao broker MQTT e
//*              publica "alive" no tópico "pico/status" a cada 10 segundos. A cada publicação bem-sucedida, acende
//*              o LED externo por 100 ms.
//*
//* RETURNS: 0 em caso de sucesso; -1 em caso de erro fatal.
//*
//*********************************************************************************************************************/
int main(int argc, FAR char *argv[]) {


    // Descritor do socket conectado ao broker
    int fd_socket = -1;

    // Registra o handler para SIGINT (Ctrl+C)
    signal(SIGINT,                                      // Sinal a ser tratado (Ctrl+C)
           tratador_sigint);                             // Função handler do sinal


    /*
    * Trata conexão IP:
    */       
 
    // Aguarda até que a interface 'wlan0' obtenha um endereço IPv4 válido
    if (network_aguardar_ip() != OK) {

        // Se não obteve IP em 60 segundos, encerra com erro
        syslog(LOG_ERR, "[main]: Tempo esgotado aguardando IP da 'wlan0'\n");

        //
        return -1;

    } // fim if

    // Aguarda um tempo extra para a rota da interface ser estabelecida
    sleep(TEMPO_ESTABILIZACAO_ROTA_S);


    /*
    * Trata Led:
    */        

    // Tenta abrir o dispositivo do LED externo; se falhar, segue apenas com MQTT
    mqtt_abrir_led();


    /*
    * Trata conexão MQTT:
    */ 

    // Tenta abrir a conexão TCP com o broker MQTT; se falhar, aguarda e tenta novamente.
    while ((fd_socket = network_abrir_socket_broker()) < 0) {

        // Se o programa foi interrompido, sai do loop
        if (s_terminar_execucao) {

            // Fecha o Led se estiver aberto
            mqtt_fechar_led();

            // Retorna sucesso para término gracioso
            return 0;

        } // fim if

        // Aguarda 5 segundos antes da próxima tentativa
        sleep(5);

        // Log de espera de conexão
        syslog(LOG_ERR, "[main]: Aguardando conexão com o Broker MQTT...\n");

    } // fim loop while

    // Inicializa o cliente MQTT-C com socket, buffers e callback
    mqtt_inicializar(fd_socket);


    // Estabelece a sessão MQTT com o broker
    if (mqtt_conectar() != MQTT_OK) {

        // Se a conexão MQTT falhou, encerra com erro
        syslog(LOG_ERR, "[main]: Falha na conexão MQTT: %s\n", mqtt_erro_str());

        // Fecha o socket
        close(fd_socket);

        // Fecha o Led
        mqtt_fechar_led();

        // Retorna erro fatal
        return -1;

    } // fim if

    // Confirma que o evento de conexão foi recebido
    if (!mqtt_verificar_conexao(50, 100000)) {

        // Log de timeout de CONNACK
        syslog(LOG_ERR, "[main]: Tempo esgotado aguardando CONNACK\n");

        // Fecha o socket
        close(fd_socket);

        // Fecha o Led
        mqtt_fechar_led();

        // Retorna erro fatal
        return -1;

    } // fim if

    // Log de sucesso de conexão
    syslog(LOG_ERR, "[main]: Conexão com o Broker MQTT estabelecida!\n");


    /*
    * Trata Publicação:
    */ 

    // Loop principal de publicação
    while (!s_terminar_execucao) {

        // Processa tráfego MQTT de entrada e saída
        mqtt_sincronizar();

        // Verifica se o cliente está com erro
        if (!mqtt_esta_ok()) {

            // Log de erro MQTT
            syslog(LOG_ERR, "[main]: Erro MQTT: %s\n", mqtt_erro_str());

            // Fecha o socket antigo
            close(fd_socket);

            // Tenta abrir novo socket
            while ((fd_socket = network_abrir_socket_broker()) < 0) {

                // Se o programa foi interrompido, sai do loop
                if (s_terminar_execucao) {

                    // Fecha o Led
                    mqtt_fechar_led();

                    // Retorna sucesso para término gracioso
                    return 0;

                } // fim if

                // Aguarda 5 segundos antes da próxima tentativa
                sleep(5);

                // Log de espera de reconexão
                syslog(LOG_ERR, "[main]: Aguardando reconexão com o Broker...\n");

            } // fim while interno

            // Reinicializa o cliente MQTT com o novo socket
            mqtt_reinicializar(fd_socket);

            // Reconecta ao broker
            if (mqtt_conectar() != MQTT_OK) {

                // Log de falha de reconexão
                syslog(LOG_ERR, "[main]: Falha na reconexão MQTT: %s\n", mqtt_erro_str());

                // Aguarda 5 segundos antes de continuar
                sleep(5);

                // Volta ao início do loop principal
                continue;

            } // fim if

            // Confirma reconexão
            if (!mqtt_verificar_conexao(50, 100000)) {

                // Log de timeout de CONNACK após reconexão
                syslog(LOG_ERR, "[main]: Tempo esgotado aguardando CONNACK após reconexão\n");

                // Volta ao início do loop principal
                continue;

            } // fim if

        } // fim if externo

        // Publica o payload no tópico definido com QoS 0
        if (mqtt_publicar() == MQTT_OK) {

            // Acende o Led externo por 100 ms
            mqtt_flash_led();

        } else {

            // Log de falha de publicação
            syslog(LOG_ERR, "[main]: Falha na publicação: %s\n", mqtt_erro_str());

        } // fim else

        // Aguarda o intervalo de publicação
        sleep(INTERVALO_PUBLICACAO_S);

    } // fim loop while


   /*
    * Trata fechamentos:
    */ 

    // Fecha o socket e o Led ao sair do loop
    close(fd_socket);

    // Fecha o LED
    mqtt_fechar_led();

    // Retorna sucesso
    return 0;

} // fim função main


//----------------------------------------------------------------------------------------------------------------------
// Fim do arquivo 'alive_mqtt_main.c'
//----------------------------------------------------------------------------------------------------------------------