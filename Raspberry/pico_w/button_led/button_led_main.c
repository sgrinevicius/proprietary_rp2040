//######################################################################################################################
// *
// *  Arquivo: "button_led_main.c"
// *
// *  Título: Aplicação Button-LED — Botão GPIO2 ativo a terra controlando LED no GPIO16
// *
// *  Design Pattern: Demonstração de Conceito — Polling de entrada, controle de saída e detecção de Ctrl+C.
// *
// *  Objetivo: Ler um botão conectado ao GPIO2 (ativo a terra) e, enquanto pressionado, ligar o LED externo conectado
// *            ao GPIO16 do Raspberry Pi Pico W. O encerramento limpo é feito via Ctrl+C detectado por 'poll()'.
// *
// *  Autor: DeepSeek
// *  Hardware: Raspberry Pi Pico W (RP2040)
// *  RTOS: Apache NuttX 13.0.0
// *
// *  Modelo: Aplicação de Usuário (Categoria 1 da Taxonomia).
// *
// *  NOTA CONCEITUAL — VIA DE ACESSO:
// *
// *    Esta aplicação utiliza a VIA 1 (VFS + Driver de Dispositivo de Caractere). O LED é acessado via '/dev/gpio16'
// *    e o botão via '/dev/gpio2', ambos registrados pelo BSP 'raspberrypi-pico-w' por meio de 'rp2040_dev_gpio_init()'.
// *    A aplicação NÃO inclui 'board.h' e NÃO utiliza callbacks diretos — toda comunicação é mediada pelo VFS.
// *
// *    Fluxo de chamadas (runtime):
// *
// *      APLICAÇÃO (este arquivo)
// *        │
// *        ├── open("/dev/gpio16", O_RDWR)                 [abre LED — Via 1]
// *        ├── open("/dev/gpio2", O_RDWR)                  [abre botão — Via 1]
// *        │
// *        ├── ioctl(fd_botao, GPIOC_READ, &estado_botao)  [lê botão — Via 1]
// *        └── ioctl(fd_led, GPIOC_WRITE, estado_led)      [escreve no LED — Via 1]
// *              │
// *              └── 'rp2040_gpio.c' → 'rp2040_gpio_put()' / 'rp2040_gpio_get()'
// *
// *    O driver 'rp2040_gpio.c' é inicializado em 'rp2040_bringup.c' durante o boot, registrando os pinos GPIO16 e
// *    GPIO2 como dispositivos de caractere no VFS.
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
 * O botão é controlado via '/dev/gpio2', configurado como entrada.
 */

// Define o caminho do dispositivo de caractere GPIO16 (LED)
#define LED_GPIO_DEVICE_PATH        "/dev/gpio16"

// Define o caminho do dispositivo de caractere GPIO2 (botão)
#define BUTTON_GPIO_DEVICE_PATH     "/dev/gpio2"

// Define o intervalo de varredura do botão (em microssegundos)
#define BUTTON_POLL_INTERVAL_US     10000       // 10ms

//______________________________________________________________________________________________________________________



/*
 ********************************************** Rotina Principal *******************************************************
 */


 
//**********************************************************************************************************************
//* FUNCTION: int main(int argc, char *argv[])
//*
//* PARAMETERS: int argc = número de argumentos, char *argv[] = vetor de argumentos
//*
//* DESCRIPTION: Função principal da aplicação. Abre os dispositivos '/dev/gpio16' (LED) e '/dev/gpio2' (botão).
//*              Em loop, lê o estado do botão e liga o LED enquanto o botão estiver pressionado (nível baixo).
//*              Encerra de forma limpa ao detectar Ctrl+C via 'poll()'.
//*
//* RETURNS: 0 em caso de sucesso, -1 em caso de erro.
//*
//*********************************************************************************************************************/
int main(int argc, char *argv[]) {

    // Descritor de arquivo para o dispositivo LED (GPIO16)
    int fd_led;

    // Descritor de arquivo para o dispositivo botão (GPIO2)
    int fd_botao;

    // Estado lógico atual do botão (true = solto, false = pressionado, pois é ativo a terra)
    bool estado_botao;

    // Estado lógico atual do LED (true = ligado, false = desligado)
    bool estado_led;

    // Imprime no Terminal (NSH) a linha superior do cabeçalho
    printf("\n==========================================================================\n");
    printf("  Botão GPIO2 controla LED GPIO16 (Via 1 - Driver de Dispositivo) \n");
    printf("==========================================================================\n\n");

    /*
     * ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────
     * VIA 1 — ABERTURA DOS DISPOSITIVOS GPIO (RUNTIME)
     *
     * 'open()' abre os dispositivos de caractere '/dev/gpio16' (LED) e '/dev/gpio2' (botão) para leitura e escrita.
     * Estes dispositivos foram registrados pelo BSP 'raspberrypi-pico-w' durante o boot.
     * ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────
     */

    // Abre o dispositivo de caractere do LED (GPIO16) para leitura e escrita
    fd_led = open(LED_GPIO_DEVICE_PATH,                                  // Caminho do dispositivo
                  O_RDWR);                                              // Abre para leitura e escrita

    // Verifica se a abertura do LED falhou
    if (fd_led < 0) {

        // Informa o usuário sobre a falha na abertura do LED
        printf("Erro ao abrir %s\n", LED_GPIO_DEVICE_PATH);

        // Retorna -1 para indicar término anormal do aplicativo
        return -1;

    } // fim if

    // Abre o dispositivo de caractere do botão (GPIO2) para leitura e escrita
    fd_botao = open(BUTTON_GPIO_DEVICE_PATH,                            // Caminho do dispositivo
                    O_RDWR);                                            // Abre para leitura e escrita

    // Verifica se a abertura do botão falhou
    if (fd_botao < 0) {

        // Informa o usuário sobre a falha na abertura do botão
        printf("Erro ao abrir %s\n", BUTTON_GPIO_DEVICE_PATH);

        // Fecha o descritor do LED antes de sair
        close(fd_led);

        // Retorna -1 para indicar término anormal do aplicativo
        return -1;

    } // fim if

    // Inicializa o estado do LED como apagado
    estado_led = false;

    // Imprime no Terminal (NSH) mensagem confirmando o início da operação
    printf("Pressione o botão para ligar o LED. CTRL+C para sair.\n");

    /*
     * Prepara array de pollfd para monitorar 'stdin':
     */

    // Array de descritores para 'poll()'
    struct pollfd fds[1];

    // Monitora entrada do terminal (stdin)
    fds[0].fd = STDIN_FILENO;

    // Acorda quando houver dados para ler (ex: tecla pressionada)
    fds[0].events = POLLIN;

    // Zera os eventos retornados para este slot
    fds[0].revents = 0;

    /*
     * Loop principal de varredura do botão:
     */

    // Loop infinito controlado por Ctrl+C
    while (true) {

        /*
         * ──────────────────────────────────────────────────────────────────────────────────────────────────────────────
         * VIA 1 — LEITURA DO BOTÃO (RUNTIME)
         *
         * Chamada 'ioctl()' ao dispositivo '/dev/gpio2':
         *   ioctl(fd_botao, GPIOC_READ, &estado_botao)
         *     → 'rp2040_gpio.c' → 'rp2040_gpio_get()'
         *
         * O botão é ativo a terra: quando pressionado, o pino lê nível baixo (false).
         * ──────────────────────────────────────────────────────────────────────────────────────────────────────────────
         */

        // Lê o estado atual do botão usando GPIOC_READ
        ioctl(fd_botao,                            // Descritor do dispositivo
              GPIOC_READ,                         // Comando: ler valor do pino
              &estado_botao);                         // Ponteiro para armazenar o valor lido

        /*
         * Lógica de controle: botão pressionado (nível baixo) liga o LED.
         */

        // Determina o estado do Led.
        estado_led = estado_botao;                    // ligado se botão pressionado (true), desligado se solto (false)

        /*
         * ──────────────────────────────────────────────────────────────────────────────────────────────────────────────
         * VIA 1 — ESCRITA NO LED (RUNTIME)
         *
         * Chamada 'ioctl()' ao dispositivo '/dev/gpio16':
         *   ioctl(fd_led, GPIOC_WRITE, estado_led)
         *     → 'rp2040_gpio.c' → 'rp2040_gpio_put()'
         * ──────────────────────────────────────────────────────────────────────────────────────────────────────────────
         */

        // Escreve o estado do LED no dispositivo GPIO16
        ioctl(fd_led,                                               // Descritor do dispositivo
              GPIOC_WRITE,                                         // Comando: escrever valor no pino
              (unsigned long)estado_led);                              // Valor: true = HIGH, false = LOW

        // Aguarda um curto intervalo para evitar leituras muito rápidas (debounce simples)
        usleep(BUTTON_POLL_INTERVAL_US);

        /*
         * Verifica 'Ctrl+C' no 'stdin' (modo não-bloqueante via poll):
         */

        // Bloqueia por 0ms — apenas verifica se há dados disponíveis
        int poll_ret = poll(fds,                                        // Array de descritores
                            1,                                    // Número de descritores (apenas stdin)
                            0);                                // Timeout: 0 = não bloqueia

        // Testa se 'poll()' retornou com erro
        if (poll_ret < 0) {

            // Informa que poll() falhou e sai do loop
            printf("[DEBUG]: 'poll()' falhou! Saindo...\n");

            // Sai do loop em caso de erro
            break;

        } // fim if

        // Verifica se há dados disponíveis no stdin
        if (fds[0].revents & POLLIN) {

            // Buffer para armazenar o caractere lido do terminal
            char ch = 0;

            // Lê um caractere do terminal (função POSIX localizada em 'nuttx/fs/vfs/fs_read.c')
            if (read(STDIN_FILENO,                                    // Descritor de arquivo (stdin)
                     &ch,                                            // Ponteiro para o buffer de leitura
                     1) > 0) {                                    // Número de bytes a ler

                // Testa se o caractere recebido é o 'Ctrl+C' (ASCII 0x03)
                if (ch == 0x03) {

                    // Informa que Ctrl+C foi detectado
                    printf("\nDEBUG: 'Ctrl+C' detectado! Saindo do loop...\n");

                    // Sai do loop imediatamente
                    break;

                } // fim if (ch == 0x03)

            } // fim if (read > 0)

        } // fim if (fds[0].revents & POLLIN)

    } // fim while

    /*
     * Encerramento:
     */

    // Imprime mensagem no Terminal (NSH) após a interceptação do Ctrl+C
    printf("\nEncerrando aplicativo de forma segura...\n");

    /*
     * VIA 1 — ESCRITA FINAL NO LED (RUNTIME):
     *
     * Força o LED a apagar antes de devolver o terminal.
     */

    // Escreve nível lógico baixo no GPIO16 para apagar o Led
    ioctl(fd_led,                                                   // Descritor do dispositivo
          GPIOC_WRITE,                                             // Comando: escrever valor no pino
          (unsigned long)false);                                       // Valor: false = LOW

    // Confirma que LED foi apagado no Terminal (NSH)
    printf("DEBUG: LED forçado para DESLIGADO antes de fechar.\n");

    // Fecha o descritor do LED
    close(fd_led);

    // Fecha o descritor do botão
    close(fd_botao);

    // Retorna sucesso para o Kernel
    return 0;

} // fim função int main(...)

//----------------------------------------------------------------------------------------------------------------------
// Fim do arquivo 'button_led_main.c'
//----------------------------------------------------------------------------------------------------------------------
