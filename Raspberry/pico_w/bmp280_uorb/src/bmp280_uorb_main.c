//######################################################################################################################
// *
// *  Arquivo: "bmp280_uorb_main.c"
// *
// *  Título: Aplicativo BMP280 uORB — Leitura de Pressão e Temperatura via API uORB
// *
// *  Objetivo: Assinar o tópico uORB 'sensor_baro' e exibir pressão (hPa) e temperatura (°C)
// *            periodicamente, usando as funções da biblioteca uORB.
// *
// *  Design Pattern: Aplicativo de Demonstração — Acesso a sensor via framework uORB (pull).
// *
// *  Autor: DeepSeek
// *  Hardware: Raspberry Pi Pico W (RP2040)
// *  RTOS: Apache NuttX 13.0.0
// *
// *  Modelo: Aplicativo de Espaço de Usuário (Categoria 1 da Taxonomia).
// *
// *  NOTA CONCEITUAL — VIA DE ACESSO:
// *
// *    O aplicativo assina o tópico 'sensor_baro' com 'orb_subscribe_multi',
// *    aguarda eventos com 'poll' e copia os dados com 'orb_copy'.
// *
// *    Fluxo de chamadas (runtime):
// *
// *      'main()'
// *        │  (no arquivo do aplicativo, ex.: 'bmp280_uorb_main.c')
// *        │
// *        ├── 'configurar_tratamento_ctrl_c()'          [instala handler de SIGINT]
// *        │       (no arquivo do aplicativo, ex.: 'bmp280_uorb_main.c')
// *        │
// *        ├── 'ORB_ID(sensor_baro)'                     [obtém metadata do tópico]
// *        │       (macro em 'apps/system/uorb/uORB/uORB.h')
// *        │
// *        ├── 'orb_subscribe_multi(meta, 0)'            [assina o tópico]
// *        │       (em 'apps/system/uorb/uORB/uORB.c')
// *        │
// *        ├── Loop:
// *        │     ├── 'poll(&fds, 1, timeout)'            [aguarda evento]
// *        │     │       (chamada POSIX, implementada no NuttX em 'fs/poll')
// *        │     │
// *        │     ├── 'orb_copy(meta, fd, &dados)'        [copia dados do evento]
// *        │     │       (em 'apps/system/uorb/uORB/uORB.c')
// *        │     │
// *        │     ├── 'printf()'                          [exibe pressão e temperatura]
// *        │     │       (biblioteca C padrão, 'libc')
// *        │     │
// *        │     └── 'usleep(2000000)'                   [aguarda 2 segundos]
// *        │             (chamada POSIX, implementada no NuttX em 'libs/libc/unistd')
// *        │
// *        └── 'orb_unsubscribe(fd)'                     [cancela assinatura]
// *                (em 'apps/system/uorb/uORB/uORB.c')
// *
// ********************************************************************************************************************/
// #####################################################################################################################
// *
// *  Ver  | dd mmm aaaa | Who      | Descrição de alterações
// * ======|=============|==========|====================================================================================
// *  1.0.0| 09 set 2026 | DeepSeek | Versão inicial do leitor BMP280 uORB (Fase 2)
// *####################################################################################################################

/*
 * Inclusão de Diretivas de Compilação - Início ------------------------------------------------------------------------
 */

/*
 * Inclusão de Bibliotecas em Arquivos de Cabeçalho --------------------------------------------------------------------
 */

/*
 * NuttX — Configuração do Kernel:
 */

// Inclui macros globais de configuração do Kernel NuttX
#include <nuttx/config.h>

// Inclui definições de OK e ERROR do NuttX
#include <nuttx/compiler.h>

/*
 * C Libraries:
 */

// Inclui funções padrão de entrada e saída
#include <stdio.h>

// Inclui funções de manipulação de arquivos, como 'usleep'
#include <unistd.h>

// Inclui definições de tipos básicos
#include <stdint.h>

// Inclui o tipo booleano nativo (true/false)
#include <stdbool.h>

// Inclui funções e constantes para 'poll'
#include <poll.h>

// Inclui o cabeçalho de 'errno' para tratar erros
#include <errno.h>

// Inclui funções de manipulação de sinais POSIX, como 'signal()' e SIGINT
#include <signal.h>

/*
 * uORB:
 */

// Inclui a definição da metadata ORB e funções de assinatura/cópia
#include <uORB/uORB.h>

// Inclui a definição da estrutura 'sensor_baro' e do 'ORB_ID' correspondente
#include <sensor/baro.h>

/*
 * Definições Gerais ---------------------------------------------------------------------------------------------------
 */

// Define o tempo máximo de espera por evento em milissegundos
#define BMP280_SENSOR_TIMEOUT_MS                1000

// Define o intervalo entre leituras (2000 ms = 2 segundos)
#define BMP280_LOOP_INTERVAL_US                 2000000

/*
 * Variáveis Globais ---------------------------------------------------------------------------------------------------
 */

// Flag de execução: controla o loop principal (false quando Ctrl+C for recebido)
static volatile bool s_executando = true;

//______________________________________________________________________________________________________________________

/*
 ********************************************** Funções Privadas *******************************************************
 */

//**********************************************************************************************************************
//* FUNCTION: static void handler_ctrl_c(int sig)
//*
//* PARAMETERS:
//*   sig - Número do sinal recebido (não utilizado)
//*
//* DESCRIPTION: Handler de sinal para SIGINT. Apenas sinaliza a interrupção alterando a flag 's_executando' para
//*              false, sem executar ações complexas (seguro para ser chamado em contexto de sinal).
//*
//* RETURNS: void
//*
//**********************************************************************************************************************
static void handler_ctrl_c(int sig) {

    // Sinaliza que a execução deve parar
    s_executando = false;

} // fim static void handler_ctrl_c(int sig)



//______________________________________________________________________________________________________________________



//**********************************************************************************************************************
//* FUNCTION: static void configurar_tratamento_ctrl_c(void)
//*
//* PARAMETERS: void
//*
//* DESCRIPTION: Configura o tratamento do sinal SIGINT (Ctrl+C). O handler 'handler_ctrl_c' apenas altera a flag
//*              's_executando' para false, permitindo que o loop principal encerre de forma controlada.
//*
//* RETURNS: void
//*
//**********************************************************************************************************************
static void configurar_tratamento_ctrl_c(void) {

    // Instala o handler para SIGINT (Ctrl+C)
    signal(SIGINT,                                  // Sinal a ser tratado
           handler_ctrl_c);                          // Função handler a ser registrada

} // fim static void configurar_tratamento_ctrl_c(void)


//______________________________________________________________________________________________________________________



/*
 * ------------------------------------------------ Rotina Principal ----------------------------------------------------
 */



//**********************************************************************************************************************
//* FUNCTION: int main(int argc, FAR char *argv[])
//*
//* DESCRIPTION: Assina o tópico uORB do sensor barômetro e exibe pressão e temperatura.
//*
//**********************************************************************************************************************
int main(int argc, FAR char *argv[]) {


    // Ponteiro para a metadata do tópico uORB do barômetro
    FAR const struct orb_metadata *meta;

    // Estrutura que contém os dados de pressão e temperatura do sensor
    struct sensor_baro dados;

    // Estrutura para 'poll' no descritor do tópico
    struct pollfd fds;

    // Descritor da assinatura uORB
    int fd;

    // Variável para armazenar o retorno das chamadas uORB
    int ret;

    // Configura o tratamento de 'Ctrl+C'
    configurar_tratamento_ctrl_c();

    /*
     * API uORB Classe Subscribe:
     */

    // Obtém a metadata do tópico uORB do sensor barômetro
    meta = ORB_ID(sensor_baro);

    // Assina o tópico uORB, permitindo múltiplos assinantes
    fd = orb_subscribe_multi(meta, 0);

    // Verifica se a assinatura falhou
    if (fd < 0) {

        // Exibe mensagem de erro
        printf("Falha ao assinar tópico uORB: %d\n", fd);

        // Retorna erro
        return ERROR;

    } // fim if

    /*
     * Prepara a estrutura de 'poll' para o descritor do tópico:
     */

    // Atribui o descritor da assinatura uORB à estrutura de 'poll'
    fds.fd = fd;

    // Define o evento de interesse como leitura disponível
    fds.events = POLLIN;

    // Loop principal de leitura, controlado pela flag 's_executando'
    while (s_executando) {

        // Aguarda evento no tópico com timeout de 1 segundo
        ret = poll(&fds,                                     // Estrutura de descritores
                   1,                                       // Número de descritores
                   BMP280_SENSOR_TIMEOUT_MS);            // Timeout em milissegundos

        // Verifica se 'poll' falhou com erro diferente de interrupção
        if (ret < 0 && errno != EINTR) {

            // Exibe mensagem de erro
            printf("Falha no poll\n");

            // Sai do loop
            break;

        } // fim if

        // Verifica se o descritor está pronto para leitura
        if (ret > 0 && (fds.revents & POLLIN)) {

            /*
             * API uORB Classe Subscribe:
             */

            // Copia os dados do evento uORB para a estrutura local
            ret = orb_copy(meta,                                 // Metadata do tópico
                           fd,                                   // Descritor da assinatura
                           &dados);                              // Buffer de destino

            // Verifica se a cópia foi bem-sucedida
            if (ret != OK) {

                // Exibe mensagem de erro
                printf("Falha ao copiar dados do sensor: %d\n", ret);

                // Sai do loop
                break;

            } // fim if

            // Exibe os valores de pressão e temperatura lidos
            printf("Pressao: %.2f hPa | Temperatura: %.2f C\n",
                   dados.pressure,                       // Pressão em hPa
                   dados.temperature);                   // Temperatura em graus Celsius

            // Aguarda o intervalo antes da próxima leitura
            usleep(BMP280_LOOP_INTERVAL_US);

        } // fim if

    } // fim loop while

    /*
     * API uORB Classe Subscribe:
     */

    // Cancela a assinatura do tópico uORB
    orb_unsubscribe(fd);

    // Retorna sucesso ao chamador
    return OK;

} // fim função int main(int argc, FAR char *argv[])


//----------------------------------------------------------------------------------------------------------------------
// Fim do arquivo "bmp280_uorb_main.c"
//----------------------------------------------------------------------------------------------------------------------
