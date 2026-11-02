//######################################################################################################################
// *
// *  Arquivo: "i2c_scanner_main.c"
// *
// *  Título: Scanner de Barramento I2C — Ferramenta de Diagnóstico
// *
// *  Design Pattern: Aplicativo de Diagnóstico — Varredura de endereços I2C.
// *
// *  Objetivo: Varrer um barramento I2C em busca de dispositivos, testando endereços de 7 bits e exibindo os
// *            endereços que responderam. Útil para detectar sensores, EEPROMs e outros periféricos.
// *
// *--------------------------------------------------------------------------------------------------------------------
// * RP2040_I2C(=y) "I2C Master"
// * RP2040_I2C0(=y) "I2C0"
// * RP2040_I2C0_SCL_GPIO(=5) "GPIO pin for I2C0 SCL (1, 5, 9, 13, 17, 21, 25, or 29)"
// * RP2040_I2C0_SDA_GPIO(=4) "GPIO pin for I2C0 SDA (0, 4, 8, 12, 16, 20, 24, or 28)"
// * RP2040_I2C0_SLAVE(=n) "I2C0"
// * RP2040_I2C0_SLAVE_10BIT(=n) "Enable 10-bit slave address"
// * 
// * RP2040_I2C0_SLAVE_ADDRESS(=) "Slave Address (in decimal)"
// * RP2040_I2C1(=n) "I2C1"
// * RP2040_I2C1_SCL_GPIO(=) "GPIO pin for I2C1 SCL (3, 7, 11, 15, 19, 23, or 27)"
// * RP2040_I2C1_SDA_GPIO(=) "GPIO pin for I2C1 SDA (2, 6, 10, 14, 18, 22, or 26)"
// * RP2040_I2C1_SLAVE(=n) "I2C1"
// * RP2040_I2C1_SLAVE_10BIT(=n) "Enable 10-bit slave address"
// * RP2040_I2C1_SLAVE_ADDRESS(=) "Slave Address (in decimal)"
// * RP2040_I2C_DRIVER(=y) "I2C character driver"
// * RP2040_I2C_SLAVE(=n) "I2C Slave"
// *
// *--------------------------------------------------------------------------------------------------------------------
// * Uso:
// *
// * nsh> i2c_scanner /dev/i2c0 (Pinos 8-SDA e 9-SCL)
// *
// * Endereços encontrados:
// * AHT10 = 0x38
// * Display Oled 128x64 Gráfico I2C Serial 0.96" = 0x3C
// * BMP180 = 0x77
// * BMP280 = 0x76
// *
// * nsh> i2c_scanner /dev/i2c1 (Pinos 4-SDA e 5-SCL)
// *
// * Endereços encontrados:
// * AHT10 = 0x38
// * Display Oled 128x64 Gráfico I2C Serial 0.96" = 0x3C
// * BMP180 = 0x77
// * BMP280 = 0x76
// *
// *--------------------------------------------------------------------------------------------------------------------
// *
// *  Autor: DeepSeek
// *  Hardware: Raspberry Pi Pico W
// *  RTOS: Apache NuttX 13.0.0
// *
// *  Modelo: Aplicativo de Espaço de Usuário (User Space)
// *
// *  Licença: Apache License 2.0
// *
// ********************************************************************************************************************/
// #####################################################################################################################
// *
// *  Ver  | dd mmm aaaa | Who      | Descrição de alterações
// * ======|=============|==========|====================================================================================
// *  1.0.0| dd mmm aaaa | DeepSeek | Versão inicial do scanner I2C
// *  1.1.0| dd mmm aaaa | DeepSeek | Corrigido uso de struct i2c_msg_s dentro de i2c_transfer_s
// *  1.2.0| 13 set 2026 | DeepSeek | Adaptado para RP2040 e comentários completados
// *####################################################################################################################

/*
 * Inclusão de Diretivas de Compilação - Início ------------------------------------------------------------------------
 */

/*
 * Inclusão de Bibliotecas em Arquivos de Cabeçalho --------------------------------------------------------------------
 */

/*
 * C Libraries:
 */

// Inclui o arquivo de configuração global do NuttX, gerado pelo Kconfig
#include <nuttx/config.h>

// Inclui funções padrão de entrada e saída
#include <stdio.h>

// Inclui funções de manipulação de arquivos, como open, close
#include <fcntl.h>

// Inclui funções de controle de dispositivos, como ioctl
#include <sys/ioctl.h>

// Inclui funções utilitárias, como usleep
#include <unistd.h>

// Inclui definições de tipos básicos
#include <stdint.h>

// Inclui funções de cópia de memória, se necessário
#include <string.h>

/*
 * NuttX:
 */

// Inclui as definições do driver de usuário I2C (ioctl I2CIOC_TRANSFER)
#include <nuttx/i2c/i2c_master.h>

/*
 * Definições Gerais ---------------------------------------------------------------------------------------------------
 */

// Endereço inicial da varredura (7 bits)
#define I2C_ADDR_INICIAL             0x03

// Endereço final da varredura (7 bits)
#define I2C_ADDR_FINAL               0x77

// Frequência I2C usada nas transações (100 kHz)
#define I2C_FREQUENCIA_PADRAO        100000

// Registrador de ID comum para muitos sensores (0xD0)
#define I2C_REG_ID                   0xD0

// Dispositivo I2C padrão no RP2040 (barramento I2C0)
#define I2C_DISPOSITIVO_PADRAO       "/dev/i2c0"

//______________________________________________________________________________________________________________________



/*
 ********************************************** Funções Privadas ********************************************************
 */



//**********************************************************************************************************************
//* FUNCTION: static int i2c_testar_endereco(int fd_i2c, uint8_t endereco, uint8_t *id_lido)
//*
//* PARAMETERS:
//*   fd_i2c   - Descritor de arquivo do dispositivo I2C aberto
//*   endereco - Endereço de 7 bits a ser testado
//*   id_lido  - Ponteiro para armazenar o ID lido (se disponível)
//*
//* DESCRIPTION: Tenta realizar uma leitura de 1 byte no endereço fornecido. Se a transação for bem-sucedida,
//*              o dispositivo está presente nesse endereço. Em seguida, tenta ler o registrador de ID e
//*              armazenar o valor em id_lido.
//*
//* RETURNS: OK (0) se o dispositivo responder; ERROR (-1) caso contrário.
//*
//**********************************************************************************************************************
static int i2c_testar_endereco(int fd_i2c, uint8_t endereco, uint8_t *id_lido) {

    // Buffer para armazenar o byte lido na sondagem
    uint8_t dado_lido;

    // Mensagem I2C: contém endereço, flags, buffer e comprimento
    struct i2c_msg_s mensagem;

    // Estrutura de transferência que agrupa as mensagens
    struct i2c_transfer_s transferencia;

    // Retorno das chamadas ioctl
    int ret_ioctl;

    // Registrador de ID a ser lido
    uint8_t reg_id = I2C_REG_ID;

    /*
     * Preenche a mensagem I2C de leitura de sondagem:
     */

    // Frequência da transação
    mensagem.frequency = I2C_FREQUENCIA_PADRAO;

    // Endereço do dispositivo (7 bits)
    mensagem.addr      = endereco;

    // Operação de leitura
    mensagem.flags     = I2C_M_READ;

    // Ponteiro para o buffer de leitura
    mensagem.buffer    = &dado_lido;

    // Tamanho: 1 byte
    mensagem.length    = 1;

    /*
     * Prepara a transferência com a mensagem:
     */

    // Array de mensagens
    transferencia.msgv = &mensagem;

    // Número de mensagens
    transferencia.msgc = 1;

    // Envia a transação via ioctl
    ret_ioctl = ioctl(fd_i2c,                                // Descritor do dispositivo I2C
                      I2CIOC_TRANSFER,                      // Comando de transferência I2C
                      (unsigned long)&transferencia);            // Estrutura de transferência

    // Verifica se a transação foi bem-sucedida
    if (ret_ioctl < 0) {

        // Nenhum dispositivo respondeu nesse endereço
        return ERROR;

        } // fim if

    /*
     * Dispositivo detectado; agora tenta ler o registrador de ID (0xD0):
     */

    /*
     * Primeiro, envia o número do registrador usando uma escrita:
     */

    // Frequência da transação
    mensagem.frequency = I2C_FREQUENCIA_PADRAO;

    // Endereço do dispositivo
    mensagem.addr      = endereco;

    // Operação de escrita
    mensagem.flags     = 0;

    // Buffer com o número do registrador
    mensagem.buffer    = &reg_id;

    // Tamanho: 1 byte
    mensagem.length    = 1;

    // Array de mensagens
    transferencia.msgv = &mensagem;

    // Número de mensagens
    transferencia.msgc = 1;

    // Envia a transação via ioctl
    ret_ioctl = ioctl(fd_i2c,                                // Descritor do dispositivo I2C
                      I2CIOC_TRANSFER,                      // Comando de transferência I2C
                      (unsigned long)&transferencia);            // Estrutura de transferência

    // Se a escrita do registrador falhou, retorna OK mas sem ID
    if (ret_ioctl < 0) {

        // ID indisponível
        *id_lido = 0x00;

        // Retorna sucesso, pois o dispositivo respondeu
        return OK;

        } // fim if

    /*
     * Agora lê 1 byte do registrador de ID:
     */

    // Frequência da transação
    mensagem.frequency = I2C_FREQUENCIA_PADRAO;

    // Endereço do dispositivo
    mensagem.addr      = endereco;

    // Operação de leitura
    mensagem.flags     = I2C_M_READ;

    // Buffer de destino do ID
    mensagem.buffer    = id_lido;

    // Tamanho: 1 byte
    mensagem.length    = 1;

    // Array de mensagens
    transferencia.msgv = &mensagem;

    // Número de mensagens
    transferencia.msgc = 1;

    // Envia a transação via ioctl
    ret_ioctl = ioctl(fd_i2c,                                // Descritor do dispositivo I2C
                      I2CIOC_TRANSFER,                      // Comando de transferência I2C
                      (unsigned long)&transferencia);            // Estrutura de transferência

    // Verifica se a leitura do ID falhou
    if (ret_ioctl < 0) {

        // ID indisponível
        *id_lido = 0x00;

        } // fim if

    // Retorna sucesso, pois o dispositivo respondeu
    return OK;

} // fim static int i2c_testar_endereco(int fd_i2c, uint8_t endereco, uint8_t *id_lido)



//______________________________________________________________________________________________________________________



/*
 ********************************************** Rotina Principal *******************************************************
 */



//**********************************************************************************************************************
//* FUNCTION: int main(int argc, FAR char *argv[])
//*
//* PARAMETERS:
//*   argc - Número de argumentos da linha de comando
//*   argv - Vetor de argumentos da linha de comando
//*
//* DESCRIPTION: Função principal do scanner I2C. Abre o dispositivo I2C fornecido como argumento (ou /dev/i2c0 por
//*              padrão) e varre os endereços de 7 bits, exibindo os que responderem e seus IDs quando disponíveis.
//*
//* RETURNS: OK (0) em caso de sucesso; ERROR (-1) em caso de falha.
//*
//**********************************************************************************************************************
int main(int argc, FAR char *argv[]) {

    // Descritor de arquivo do dispositivo I2C
    int fd_i2c;

    // Endereço atual em teste
    uint8_t endereco;

    // Contador de dispositivos encontrados
    int dispositivos_encontrados = 0;

    // Variável para armazenar o ID lido
    uint8_t id_lido = 0;

    // Caminho do dispositivo I2C (padrão: /dev/i2c0)
    const char *caminho_i2c = I2C_DISPOSITIVO_PADRAO;

    // Verifica se o usuário passou outro dispositivo como argumento
    if (argc > 1) {

        // Usa o primeiro argumento como caminho do dispositivo
        caminho_i2c = argv[1];

        } // fim if

    // Abre o dispositivo I2C em modo leitura/escrita
    fd_i2c = open(caminho_i2c, O_RDWR);

    // Verifica se a abertura falhou
    if (fd_i2c < 0) {

        // Informa o erro de abertura
        printf("Erro ao abrir %s\n", caminho_i2c);

        // Retorna erro
        return ERROR;

        } // fim if

    // Exibe o cabeçalho da varredura
    printf("Varredura I2C em %s\n", caminho_i2c);

    // Exibe o cabeçalho dos endereços
    printf("Endereços encontrados (ID):\n");

    // Percorre a faixa de endereços
    for (endereco = I2C_ADDR_INICIAL; endereco <= I2C_ADDR_FINAL; ++endereco) {

        // Testa o endereço atual e tenta obter o ID
        if (i2c_testar_endereco(fd_i2c, endereco, &id_lido) == OK) {

            // Verifica se o ID foi lido com sucesso
            if (id_lido != 0) {

                // Exibe o endereço e o ID em hexadecimal
                printf("  0x%02X  (ID: 0x%02X)\n", endereco, id_lido);

                }
            else {

                // Exibe o endereço sem ID
                printf("  0x%02X  (ID: indisponível)\n", endereco);

                } // fim if

            // Incrementa o contador de dispositivos encontrados
            dispositivos_encontrados++;

            } // fim if

        } // fim for

    // Verifica se nenhum dispositivo foi encontrado
    if (dispositivos_encontrados == 0) {

        // Informa que nenhum dispositivo respondeu
        printf("  Nenhum dispositivo encontrado.\n");

        } // fim if

    // Fecha o dispositivo
    close(fd_i2c);

    // Retorna sucesso
    return OK;

} // fim função int main(int argc, FAR char *argv[])


//----------------------------------------------------------------------------------------------------------------------
//  Fim do arquivo "i2c_scanner_main.c"
//----------------------------------------------------------------------------------------------------------------------