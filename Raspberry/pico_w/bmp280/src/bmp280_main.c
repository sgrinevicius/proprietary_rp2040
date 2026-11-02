//######################################################################################################################
// *
// *  Arquivo: "bmp280_main.c"
// *
// *  Título: Aplicativo BMP280 — Leitura de Pressão e Temperatura via 'read()'
// *
// *  Objetivo: Ler os dados do sensor BMP280 registrado no dispositivo de caractere
// *            "/dev/uorb/sensor_baro0" e exibi-los no console periodicamente.
// *
// *--------------------------------------------------------------------------------------------------------------------
// *
// * Notas:
// *
// * O diretório `/dev/uorb` é definido pelo subsistema de sensores do NuttX, especificamente pela camada que 
// * registra os dispositivos via `sensor_register()`. 
// *
// * Esse caminho é escolhido pelo núcleo do sensor uORB, não pela aplicação. <====================== !!!!!!!!!!!!
// *
// * Portanto, mesmo usando apenas `read()`, o nó do sensor aparece em `/dev/uorb`. <================ !!!!!!!!!!!!
// * Para mudar isso, seria necessário alterar o código do framework, o que foge a este escopo.
// *
// *--------------------------------------------------------------------------------------------------------------------
// *  Design Pattern: Aplicativo de Demonstração — Acesso a sensor via driver de caractere (pull).
// *
// *  Autor: DeepSeek
// *  Hardware: Raspberry Pi Pico W (RP2040)
// *  RTOS: Apache NuttX 13.0.0
// *
// *  Modelo: Aplicativo de Espaço de Usuário (Categoria 1 da Taxonomia).
// *
// ********************************************************************************************************************/
// #####################################################################################################################
// *
// *  Ver  | dd mmm aaaa | Who      | Descrição de alterações
// * ======|=============|==========|===================================================================================
// *  1.0.0| 09 set 2026 | DeepSeek | Versão inicial do leitor BMP280 via read()
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

// Inclui funções de manipulação de arquivos
#include <unistd.h>

// Inclui funções de abertura de arquivos e flags de acesso
#include <fcntl.h>

// Inclui definições de tipos básicos
#include <stdint.h>

// Inclui códigos de erro
#include <errno.h>

// Inclui funções de manipulação de sinais POSIX
#include <signal.h>

/*
    * NuttX Sensors:
    */

// Inclui a definição da estrutura 'sensor_baro'
#include <nuttx/sensors/sensor.h>


/*
    * Definições Gerais ---------------------------------------------------------------------------------------------------
    */

// Define o caminho do dispositivo de caractere do barômetro
#define BMP280_DEV_PATH                         "/dev/uorb/sensor_baro0"

// Define o intervalo entre leituras (2 segundos)
#define BMP280_READ_INTERVAL_US                 2000000


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
    signal(SIGINT,                                              // Sinal a ser tratado
           handler_ctrl_c);                                 // Função handler a ser registrada

} // fim static void configurar_tratamento_ctrl_c(void)

//______________________________________________________________________________________________________________________



/*
* ------------------------------------------------ Rotina Principal ----------------------------------------------------
*/



//**********************************************************************************************************************
//* FUNCTION: int main(int argc, FAR char *argv[])
//*
//* DESCRIPTION: Abre o dispositivo de caractere do BMP280 e lê continuamente os dados
//*              de pressão e temperatura via 'read()'.
//*
//**********************************************************************************************************************
int main(int argc, FAR char *argv[]) {


    // Descritor de arquivo para o dispositivo do sensor
    int fd;

    // Variável para armazenar o retorno das chamadas
    int ret;

    // Estrutura que contém os dados do sensor
    struct sensor_baro dados;

    // Configura o tratamento de 'Ctrl+C'
    configurar_tratamento_ctrl_c();

    // Abre o dispositivo de caractere do barômetro no modo somente leitura
    fd = open(BMP280_DEV_PATH,                             // Caminho do dispositivo do sensor
              O_RDONLY |                                  // Modo de acesso: somente leitura
		      O_NONBLOCK);								         //

    // Verifica se a abertura do dispositivo falhou
    if (fd < 0) {

        // Exibe mensagem de erro indicando falha na abertura
        printf("Falha ao abrir %s\n", BMP280_DEV_PATH);

        // Retorna erro para o chamador
        return ERROR;

    } // fim if

    // Loop infinito de leitura e exibição dos dados do sensor
    while (s_executando) {

        // Lê os dados do sensor no formato 'struct sensor_baro'
        ret = read(fd,                                         // Descritor do dispositivo
                   &dados,                                 // Buffer de destino
                   sizeof(dados));                      // Tamanho da estrutura

        // Verifica se a leitura não retornou exatamente o tamanho esperado
        if (ret != sizeof(dados)) {

            // Exibe mensagem de erro indicando falha na leitura
            printf("Falha na leitura do sensor\n");

            // Sai do loop em caso de erro de leitura
            break;

        } // fim if

        // Exibe pressão e temperatura lidas do sensor
        printf("Pressao: %.2f hPa | Temperatura: %.2f C\n",
                dados.pressure,                                // Pressão em hPa
                dados.temperature);                            // Temperatura em graus Celsius

        // Aguarda o intervalo definido antes da próxima leitura
        usleep(BMP280_READ_INTERVAL_US);

    } // fim loop while

    // Fecha o dispositivo de caractere
    close(fd);

    // Retorna sucesso ao chamador
    return OK;


} // fim função int main(int argc, FAR char *argv[])


//----------------------------------------------------------------------------------------------------------------------
// Fim do arquivo "bmp280_main.c"
//----------------------------------------------------------------------------------------------------------------------
