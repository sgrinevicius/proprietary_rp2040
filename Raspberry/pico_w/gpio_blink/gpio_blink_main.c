//######################################################################################################################
// *
// *  Arquivo: "gpio_blink_main.c"
// *
// *  Título: Aplicação de Flash de LED via GPIO 'rp2040_dev_gpio_init' (Via 1)
// *
// *  Design Pattern: Demonstração de Conceito — Loop periódico com verificação de Ctrl+C em função separada.
// *
// *  Objetivo: Produzir um flash de 100 ms no LED conectado ao GPIO16 do Raspberry Pi Pico W a cada segundo,
// *            utilizando o driver de dispositivo '/dev/gpio16' exposto pelo BSP out-of-tree.
// *
// *  Autor: DeepSeek
// *  Hardware: Raspberry Pi Pico W (RP2040)
// *  RTOS: Apache NuttX 13.0.0
// *
// *  Modelo: Aplicação de Usuário (Categoria 1 da Taxonomia).
// *
// *  NOTA CONCEITUAL — VIA DE ACESSO:
// *
// *    Esta aplicação utiliza a VIA 1 (VFS + Driver de Dispositivo de Caractere). O LED é acessado através do
// *    dispositivo '/dev/gpio16', registrado pelo BSP 'raspberrypi-pico-w' por meio da função
// *    'rp2040_dev_gpio_init()'. A aplicação NÃO inclui 'board.h' e NÃO utiliza callbacks diretos.
// *
// *    Fluxo de chamadas (runtime):
// *
// *      APLICAÇÃO (este arquivo)
// *        │
// *        │ open("/dev/gpio16")                          [abre o dispositivo GPIO — Via 1]
// *        │ ioctl(fd, GPIOC_WRITE, estado_led)           [escreve no pino GPIO — Via 1]
// *        │
// *        └── 'rp2040_gpio.c' [em 'proprietary/boards/.../src/']
// *              │
// *              └── 'rp2040_gpio_put()' [em 'arch/arm/src/rp2040/rp2040_gpio.c']
// *                    │
// *                    └── Registrador de saída GPIO do RP2040
// *
// *    O driver 'rp2040_gpio.c' é inicializado em 'rp2040_bringup.c' durante o boot, chamando
// *    'rp2040_dev_gpio_init()' que registra o pino GPIO16 como dispositivo de caractere.
// *
// ********************************************************************************************************************/

/*
 * Inclusão de Diretivas de Compilação - Início ------------------------------------------------------------------------
 */

/*
 * Inclusão de Bibliotecas em Arquivos de Cabeçalho --------------------------------------------------------------------
 */

/*
 * C Libraries:
 */

// Inclui o tipo booleano nativo (true/false) do C23
#include <stdbool.h>

// Inclui tipos de dados com largura de bits fixa (uintptr_t)
#include <stdint.h>

// Inclui a biblioteca padrão de entrada e saída em C
#include <stdio.h>

// Inclui funções padrão do sistema como close e usleep
#include <unistd.h>

/*
 * POSIX:
 */

// Inclui a função 'poll()' para espera assíncrona por eventos ('nuttx/include/poll.h')
#include <poll.h>

/*
 * NuttX:
 */

// Inclui macros globais de configuração do Kernel NuttX ('nuttx/build/include/nuttx/config.h')
#include <nuttx/config.h>

/*
 * Driver de Dispositivo GPIO (Via 1):
 *
 * Este cabeçalho define os comandos 'ioctl' para acesso a GPIO, como 'GPIOC_WRITE' e 'GPIOC_READ'.
 * A inclusão deste cabeçalho mantém a portabilidade da aplicação, pois não depende de 'board.h'.
 */
#include <nuttx/ioexpander/gpio.h>

// Inclui a interface de controle de arquivos, necessária para a função 'open()' e a flag 'O_RDWR'
#include <fcntl.h>

// Inclui a interface de controle de dispositivos, necessária para a função 'ioctl()'
#include <sys/ioctl.h>

/*
 * Definições Gerais ---------------------------------------------------------------------------------------------------
 */

/*
 * Driver de Dispositivo de Saída (GPIO):
 *
 * O LED externo é controlado via dispositivo '/dev/gpio16', registrado pelo BSP
 * 'raspberrypi-pico-w'. O pino GPIO16 é configurado como saída pelo driver durante o boot.
 *
 * ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
 * VIA 1 — CONTEXTO DE REGISTRO:
 *
 * O dispositivo '/dev/gpio16' é registrado em 'rp2040_bringup.c' pela chamada:
 *   'rp2040_dev_gpio_init()' → registra o GPIO16 como dispositivo de caractere.
 *
 * A aplicação acessa o GPIO através do VFS, usando 'open()' e 'ioctl()'.
 * ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
 */

// Define o caminho do dispositivo de caractere GPIO16
#define GPIO_DEVICE_PATH            "/dev/gpio16"

// Define o tempo em microssegundos em que o LED permanece aceso (flash de 100 ms)
#define LED_ON_TIME_US              100000

// Define o tempo em microssegundos em que o LED permanece apagado (complemento para 1 s)
#define LED_OFF_TIME_US             900000

//______________________________________________________________________________________________________________________

/*
 * Funções Privadas ----------------------------------------------------------------------------------------------------
 */

//**********************************************************************************************************************
//*  FUNCTION: static bool check_stdin_ctrl_c(void)
//*
//*  PARAMETERS: void
//*
//*  DESCRIPTION: Verifica, de forma não-bloqueante, se o usuário pressionou Ctrl+C no terminal.
//*               Utiliza 'poll()' para monitorar o 'stdin' e retorna true se o caractere lido for 0x03.
//*
//*  RETURNS: true se Ctrl+C foi detectado; false caso contrário.
//*
//**********************************************************************************************************************
static bool check_stdin_ctrl_c(void) {

    // Array de descritores para 'poll()'
    struct pollfd fds[1];

    // Monitora entrada do terminal (stdin)
    fds[0].fd = STDIN_FILENO;

    // Acorda quando houver dados para ler (ex: tecla pressionada)
    fds[0].events = POLLIN;

    // Zera os eventos retornados para este slot
    fds[0].revents = 0;

    // Bloqueia por 0ms — apenas verifica se há dados disponíveis
    int poll_ret = poll(fds,                                         // Array de descritores
                        1,                                      // Número de descritores (apenas stdin)
                        0);                                  // Timeout: 0 = não bloqueia

    // Testa se 'poll()' retornou com erro
    if (poll_ret < 0) {

        // Informa que poll() falhou
        printf("[DEBUG]: 'poll()' falhou!\n");

        // Retorna true para indicar que o loop deve ser encerrado
        return true;

    } // fim if

    // Verifica se há dados disponíveis no stdin
    if (fds[0].revents & POLLIN) {

        // Buffer para armazenar o caractere lido do terminal
        char ch = 0;

        // Lê um caractere do terminal (função POSIX localizada em 'nuttx/fs/vfs/fs_read.c')
        if (read(STDIN_FILENO,                                     // Descritor de arquivo (stdin)
                 &ch,                                             // Ponteiro para o buffer de leitura
                 1) > 0) {                                     // Número de bytes a ler

            // Testa se o caractere recebido é o 'Ctrl+C' (ASCII 0x03)
            if (ch == 0x03) {

                // Informa que Ctrl+C foi detectado
                printf("\nDEBUG: 'Ctrl+C' detectado! Saindo do loop...\n");

                // Retorna true para indicar que o loop deve ser encerrado
                return true;

            } // fim if (ch == 0x03)

        } // fim if (read > 0)

    } // fim if (fds[0].revents & POLLIN)

    // Retorna false para indicar que o loop deve continuar
    return false;

} // fim função static bool check_stdin_ctrl_c(void)

//______________________________________________________________________________________________________________________

/*
 ********************************************** Rotina Principal *******************************************************
 */

//**********************************************************************************************************************
//* FUNCTION: int main(int argc, char *argv[])
//*
//* PARAMETERS: int argc = número de argumentos, char *argv[] = vetor de argumentos
//*
//* DESCRIPTION: Função principal da aplicação. Abre o dispositivo '/dev/gpio16', e em loop infinito:
//*               liga o LED por 100 ms, desliga por 900 ms, repetindo o ciclo a cada segundo.
//*               Encerra de forma limpa ao detectar Ctrl+C via 'check_stdin_ctrl_c()'.
//*
//* RETURNS: 0 em caso de sucesso, -1 em caso de erro.
//*
//*********************************************************************************************************************/
int main(int argc, char *argv[]) {

    // Descritor de arquivo para o dispositivo GPIO16
    int fd;

    // Imprime no Terminal (NSH) a linha superior do cabeçalho
    printf("\n==========================================================================\n");
    printf("  Flash de LED no GPIO16 — 100 ms a cada segundo (Via 1) \n");
    printf("==========================================================================\n\n");

    /*
     * ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────
     * VIA 1 — ABERTURA DO DISPOSITIVO GPIO (RUNTIME)
     *
     * 'open()' abre o dispositivo de caractere '/dev/gpio16' para leitura e escrita.
     * Este dispositivo foi registrado pelo BSP 'raspberrypi-pico-w' durante o boot.
     * ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────
     */

    // Abre o dispositivo de caractere GPIO16 para leitura e escrita
    fd = open(GPIO_DEVICE_PATH,                                        // Caminho do dispositivo
              O_RDWR);                                                // Abre para leitura e escrita

    // Verifica se a abertura do dispositivo falhou
    if (fd < 0) {

        // Informa o usuário sobre a falha na abertura do dispositivo
        printf("Erro ao abrir %s\n", GPIO_DEVICE_PATH);

        // Retorna -1 para indicar término anormal do aplicativo
        return -1;

    } // fim if

    // Imprime no Terminal (NSH) mensagem confirmando o início do flash
    printf("Flash iniciado! Pressione CTRL+C para sair.\n");

    /*
     * Loop principal de flash:
     */

    // Loop infinito controlado por Ctrl+C
    while (true) {

        /*
         * ──────────────────────────────────────────────────────────────────────────────────────────────────────────────
         * VIA 1 — ESCRITA NO GPIO (RUNTIME)
         *
         * Liga o LED escrevendo 'true' no dispositivo '/dev/gpio16'.
         * ──────────────────────────────────────────────────────────────────────────────────────────────────────────────
         */

        // Escreve nível lógico alto no GPIO16 para ligar o LED
        ioctl(fd,                                                       // Descritor do dispositivo
              GPIOC_WRITE,                                             // Comando: escrever valor no pino
              (unsigned long)true);                                    // Valor: true = HIGH

        // Mantém o LED aceso por 100 ms
        usleep(LED_ON_TIME_US);

        /*
         * ──────────────────────────────────────────────────────────────────────────────────────────────────────────────
         * VIA 1 — ESCRITA NO GPIO (RUNTIME)
         *
         * Desliga o LED escrevendo 'false' no dispositivo '/dev/gpio16'.
         * ──────────────────────────────────────────────────────────────────────────────────────────────────────────────
         */

        // Escreve nível lógico baixo no GPIO16 para desligar o LED
        ioctl(fd,                                                       // Descritor do dispositivo
              GPIOC_WRITE,                                             // Comando: escrever valor no pino
              (unsigned long)false);                                   // Valor: false = LOW

        // Mantém o LED apagado por 900 ms
        usleep(LED_OFF_TIME_US);

        /*
         * Verifica 'Ctrl+C' no 'stdin' (modo não-bloqueante):
         */

        // Chama a função de verificação de Ctrl+C; se true, sai do loop
        if (check_stdin_ctrl_c()) {

            // Sai do loop imediatamente
            break;

        } // fim if

    } // fim while

    // Imprime mensagem no Terminal (NSH) após a interceptação do Ctrl+C
    printf("\nEncerrando aplicativo de forma segura...\n");

    /*
     * VIA 1 — ESCRITA FINAL NO GPIO (RUNTIME):
     *
     * Força o LED a apagar antes de devolver o terminal.
     */

    // Escreve nível lógico baixo no GPIO16 para apagar o LED
    ioctl(fd,                                                          // Descritor do dispositivo
          GPIOC_WRITE,                                                // Comando: escrever valor no pino
          (unsigned long)false);                                      // Valor: false = LOW

    // Confirma que LED foi apagado no Terminal (NSH)
    printf("DEBUG: LED forçado para DESLIGADO antes de fechar.\n");

    // Fecha o descritor do dispositivo GPIO16
    close(fd);

    // Retorna sucesso para o Kernel
    return 0;

} // fim função int main(...)

//----------------------------------------------------------------------------------------------------------------------
// Fim do arquivo 'gpio_blink_main.c'
//----------------------------------------------------------------------------------------------------------------------