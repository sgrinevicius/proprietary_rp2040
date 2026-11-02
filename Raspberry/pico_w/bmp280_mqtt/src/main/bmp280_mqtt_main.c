//######################################################################################################################
// *
// *  Arquivo: "bmp280_mqtt_main.c"
// *
// *  Título: Aplicativo BMP280 MQTT — Leitura de Pressão e Temperatura com publicação na Plataforma WEGnology
// *
// *  Objetivos:
// *
// *  - Assinar o tópico uORB 'sensor_baro' e manter internamente a última amostra de pressão e temperatura.
// *
// *  - Publicar periodicamente os valores na Plataforma WEGnology via MQTT, no formato JSON esperado pelo tópico
// *    de estado (state) da plataforma.
// *
// *  - Piscar o Led externo (GPIO16) a cada publicação bem-sucedida, servindo como indicador visual de que o
// *    sistema está operacional (WiFi, MQTT e aquisição de sensor).
// *
// *  Design Pattern: Acesso a sensor via framework uORB (pull) + publicação MQTT gerenciada por módulo.
// *
// *  Autor: Sérgio Grinevicius/DeepSeek
// *  Hardware: Raspberry Pi Pico W (RP2040)
// *  RTOS: Apache NuttX 13.0.0
// *
// *  Modelo: Aplicativo de Espaço de Usuário.
// *
// *--------------------------------------------------------------------------------------------------------------------
// * 
// *  - NOTA CONCEITUAL — VIA DE ACESSO E CADÊNCIA:
// *
// *    O aplicativo assina o tópico 'sensor_baro' com 'orb_subscribe_multi', aguarda eventos com 'poll' e copia
// *    os dados com 'orb_copy'. Uma cópia é feita a cada segundo, mas a publicação no MQTT só ocorre quando o intervalo 
// *    configurado decorre desde a última publicação bem-sucedida. Essa separação permite que a taxa do sensor (que 
// *    pode ser de dezenas de milissegundos) seja desacoplada da taxa de publicação (60 segundos, por padrão).
// *
// *    Se nenhuma amostra nova chegou entre duas publicações, a publicação é pulada e um aviso é registrado.
// *    Isso evita enviar dado "stale" para a nuvem quando o driver do sensor parar de publicar.
// *
// *
// *  - NOTA CONCEITUAL — SOBRE 'printf' vs 'syslog':
// *
// *    Este arquivo (o 'main', a interface com o usuário) usa 'printf' para mensagens de progresso, e os módulos de 
// *    apoio ('mqtt_utils', 'mqtt_wnology', 'network_utils') usam 'syslog'. A distinção é intencional:
// *
// *      - 'printf' escreve na saída padrão do processo (o console do NSH, quando executado em primeiro plano).
// *        Mensagens destinadas ao operador humano ficam aqui.
// *
// *      - 'syslog' registra eventos de sistema, que podem ser capturados por um 'daemon' de log, redirecionados
// *        para um arquivo, ou analisados com severidade ('LOG_ERR', 'LOG_INFO', etc). Mensagens internas dos
// *        módulos de suporte ficam aqui.
// *
// *    Em um sistema embarcado de produção, essa separação ajuda a filtrar o que é "diálogo com o operador" do que é 
// *    "diagnóstico do sistema".
// *
// * 
// *  - NOTA CONCEITUAL — CORREÇÃO DE PRESSÃO PARA O NÍVEL DO MAR (QNH):
// *
// *    O driver uORB publica a pressão absoluta lida pelo BMP280 — ou seja, a pressão real no local, que
// *    varia com a altitude. Em Ponta Grossa (~950-1000m), isso resulta em leituras da ordem de 900-920hPa.
// *
// *    As estações meteorológicas e a maioria dos dashboards de plataformas IoT, por outro lado, publicam a chamada 
// *    "pressão relativa ao nível do mar" (QNH), que é a pressão absoluta corrigida pela altitude do ponto de medição. 
// *    O QNH fica tipicamente entre 1000 e 1030 hPa, independentemente da altitude.
// *
// *    A correção usa o modelo padrão ISA (International Standard Atmosphere):
// *
// *      P0 = P * (1-h/44330)^(-5.255)
// *
// *    Neste aplicativo, a correção é feita com um fator constante, previamente calculado, para evitar dependência de 
// *    funções de ponto flutuante mais pesadas ('powf'). O valor de 'FATOR_CORRECAO_QNH' foi calibrado empiricamente 
// *    para igualar à leitura do BME280 do projeto irmão (Raspberry Pi 4) e corresponde a uma altitude barométrica 
// *    efetiva de ~914 m — não à cota geográfica oficial da cidade. Trata-se de uma decisão de consistência entre os 
// *    dois projetos, não de exatidão absoluta.
// *
// *--------------------------------------------------------------------------------------------------------------------
// *
// *  Formato do payload WEGnology:
// *
// *    {"data":{"pressao":1013.25,"temperatura":25.30}}
// *
// *    A formatação usa apenas inteiros (parte inteira + centésimos separados), sem depender de ponto flutuante na 
// *    biblioteca C (que pode não estar disponível no 'printf'/'snprintf' de alguns builds NuttX).
// *
// *  Argumento de linha de comando:
// *
// *    bmp280_mqtt           -> publica a cada 60 segundos (padrão)
// *    bmp280_mqtt 10        -> publica a cada 10 segundos (para testes)
// *
// *    O valor mínimo aceito é 'INTERVALO_MINIMO_PUBLICACAO_S'. Se um valor menor for informado, o mínimo é aplicado, 
// *    para evitar sobrecarregar o broker com publicações muito frequentes.
// *
// *  Fluxo de chamadas (runtime):
// *
// *    'main()'
// *      │
// *      ├── 'tratador_sigint()'                 [instala handler de SIGINT]
// *      │
// *      ├── 'ORB_ID(sensor_baro)'               [obtém metadata do tópico uORB]
// *      │
// *      ├── 'orb_subscribe_multi(meta, 0)'      [assina o tópico]
// *      │
// *      ├── 'mqtt_wnology_iniciar()'            [rede + LED + socket + MQTT]
// *      │
// *      ├── Loop:
// *      │     ├── 'poll(&fds, 1, 1000)'         [aguarda evento, timeout de 1 s]
// *      │     │
// *      │     ├── 'orb_copy()'                  [copia dados do evento]
// *      │     │
// *      │     ├── 'mqtt_wnology_sincronizar()'  [processa tráfego MQTT + limpa fila]
// *      │     │
// *      │     ├── 'mqtt_wnology_reconectar()'   [se o cliente está em erro]
// *      │     │
// *      │     ├── 'snprintf()'                  [monta o JSON com os dados do sensor]
// *      │     │
// *      │     ├── 'mqtt_wnology_publicar()'     [publica no tópico state]
// *      │     │
// *      │     └── 'mqtt_flash_led()'            [pisca o LED em caso de sucesso]
// *      │
// *      ├── 'mqtt_wnology_finalizar()'          [encerra MQTT + fecha socket + fecha LED]
// *      │
// *      └── 'orb_unsubscribe(fd_uorb)'          [cancela assinatura uORB]
// *
// *
// ********************************************************************************************************************/
// #####################################################################################################################
// *
// *  Ver  | dd mmm aaaa | Who      | Descrição de alterações
// * ======|=============|==========|===================================================================================
// *  1.0.0| 26 set 2026 |   S.G.   | Versão inicial: integração BMP280 uORB + MQTT WEGnology
// *  1.1.0| 27 set 2026 |   S.G.   | Adiciona correção QNH (fator calibrado) na pressão publicada
// *  1.2.0| 28 set 2026 |   S.G.   | Migração de Losant para WEGnology (nomes e comentários)
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

// Inclui funções utilitárias padrão: 'atoi' (conversão de string em inteiro) e 'abs' (valor absoluto).
// Nota didática: este único include cobre tanto 'atoi' quanto 'abs'; ambos estão declarados em '<stdlib.h>'.
#include <stdlib.h>

// Inclui funções de manipulação de arquivos, como 'usleep'
#include <unistd.h>

// Inclui definições de tipos básicos
#include <stdint.h>

// Inclui o tipo booleano nativo (true/false)
#include <stdbool.h>

// Inclui a função 'time' para o controle do intervalo entre publicações
#include <time.h>

// Inclui funções e constantes para 'poll'
#include <poll.h>

// Inclui o cabeçalho de 'errno' para tratar erros
#include <errno.h>

// Inclui funções de manipulação de sinais POSIX, como 'signal()' e SIGINT
#include <signal.h>


/*
 * NuttX — APIs e utilitários:
 */

// Inclui funções de debug e logging do NuttX, como syslog()
#include <nuttx/debug.h>


/*
 * uORB:
 */

// Inclui a definição da metadata ORB e funções de assinatura/cópia
#include <uORB/uORB.h>

// Inclui a definição da estrutura 'sensor_baro' e do 'ORB_ID' correspondente
#include <sensor/baro.h>


/*
 * Módulos do aplicativo:
 */

// Inclui as funções de utilitários MQTT (flash do LED)
#include "mqtt_utils.h"

// Inclui as funções do gerenciador MQTT (iniciar, publicar, reconectar, finalizar)
#include "mqtt_wnology.h"


/*
 * Definições Gerais ---------------------------------------------------------------------------------------------------
 */

// Define o tempo máximo de espera por evento uORB em milissegundos
#define BMP280_SENSOR_TIMEOUT_MS                   1000

// Define o intervalo padrão entre publicações MQTT, em segundos (60 seg = 1 minuto)
#define INTERVALO_PUBLICACAO_S                     60

// Define o intervalo mínimo aceito para publicação, em segundos (evita 'flood' no broker)
#define INTERVALO_MINIMO_PUBLICACAO_S              5

// Define o tamanho do buffer do payload JSON
#define TAMANHO_BUFFER_JSON                        128


/*
 * Define o fator de correção QNH (relação entre pressão ao nível do mar e pressão absoluta local).
 *
 * Nota didática: o driver uORB publica pressão absoluta (leitura bruta do BMP280). Para obter a pressão QNH — a que
 * aparece nas estações meteorológicas e nos dashboards de plataformas como a WEGnology - é preciso aplicar a fórmula
 * ISA:
 *
 *     P0 = P*(1-h/44330)^(-5.255)
 *
 * onde 'h' é a altitude do local em metros. Para Ponta Grossa (cota geográfica ~975 m), o fator resultante seria 
 * ~1,124 e a leitura seria ~1029 hPa.
 *
 * O valor abaixo, porém, foi calibrado empiricamente para igualar à leitura do BME280 do projeto irmão (Raspberry Pi4), 
 * que mostra ~1022 hPa. Corresponde a uma altitude barométrica efetiva de ~914 m, não à cota geográfica oficial. 
 * Trata-se de uma decisão de consistência entre os dois projetos, não de exatidão absoluta.
 */

#define FATOR_CORRECAO_QNH                         1.116f

//______________________________________________________________________________________________________________________


/*
 * Variáveis Globais ---------------------------------------------------------------------------------------------------
 */

// Flag de execução: controla o loop principal (false quando 'Ctrl+C' for recebido)
static volatile bool s_terminar_execucao = false;

//______________________________________________________________________________________________________________________



/*
 ********************************************** Funções Privadas *******************************************************
 */



//**********************************************************************************************************************
//* FUNCTION: static void tratador_sigint(int sinal)
//*
//* PARAMETERS:
//*   sinal - Número do sinal recebido (não utilizado)
//*
//* DESCRIPTION: Handler de sinal para SIGINT. Apenas sinaliza a interrupção alterando a flag 's_terminar_execucao' para
//*              true, sem executar ações complexas (seguro para ser chamado em contexto de sinal).
//*
//* RETURNS: void
//*
//**********************************************************************************************************************
static void tratador_sigint(int sinal) {

    // Sinaliza que a execução deve parar
    s_terminar_execucao = true;

} // fim static void tratador_sigint(int sinal)



//______________________________________________________________________________________________________________________



/*
 * ------------------------------------------------ Rotina Principal ----------------------------------------------------
 */



//**********************************************************************************************************************
//* FUNCTION: int main(int argc, FAR char *argv[])
//*
//* PARAMETERS:
//*   argc - Número de argumentos (1 ou 2)
//*   argv - Vetor de argumentos; argv[1] opcional = intervalo entre publicações, em segundos
//*
//* DESCRIPTION:
//*
//*   Assina o tópico uORB 'sensor_baro', inicializa a conexão MQTT com a plataforma WEGnology e entra em um loop
//*   principal. A cada iteração:
//*
//*     - Aguarda evento do sensor com 'poll' (timeout de 1 s);
//*     - Copia a amostra mais recente com 'orb_copy' e a mantém em memória;
//*     - Processa tráfego MQTT e reconecta se necessário;
//*     - Publica um JSON com a última amostra caso o intervalo configurado tenha decorrido.
//*
//*   Ao receber Ctrl+C, sai do loop, encerra a sessão MQTT, fecha o LED e desfaz a assinatura uORB.
//*
//* RETURNS: OK (0) em caso de sucesso; ERROR (-1) em caso de falha fatal.
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
    int fd_uorb;

    // Variável para armazenar o retorno das chamadas
    int ret;

    // Intervalo entre publicações, em segundos (padrão ou argumento)
    int intervalo_s = INTERVALO_PUBLICACAO_S;

    // Marca temporal da última publicação
    time_t ultima_publicacao_s;

    // Marca temporal da iteração atual
    time_t agora_s;

    // Flag que indica se há alguma amostra válida desde a última publicação
    bool tem_amostra = false;

    // Buffer para o payload JSON
    char json[TAMANHO_BUFFER_JSON];

    // Comprimento do JSON gerado
    int comprimento_json;

    // Valor de pressão em centésimos de hPa
    int pressao_centesimos;

    // Valor de temperatura em centésimos de grau Celsius
    int temperatura_centesimos;

    // Valor de pressão corrigida para o nível do mar (QNH)
    float pressao_qnh;

    /*
    * Trata argumentos de linha de comando:
    */

    // Verifica se foi informado o intervalo como primeiro argumento
    if (argc > 1) {

        // Converte o argumento para inteiro (segundos)
        intervalo_s = atoi(argv[1]);

        // Garante que o intervalo não seja inferior ao mínimo aceito
        if (intervalo_s < INTERVALO_MINIMO_PUBLICACAO_S) {

            // Aplica o mínimo
            intervalo_s = INTERVALO_MINIMO_PUBLICACAO_S;

        } // fim if

    } // fim if

    // Registra o intervalo efetivo
    printf("[bmp280_mqtt]: intervalo de publicação = %d s\n", intervalo_s);


    /*
    * Configura o tratamento de Ctrl+C:
    */

    // Instala o handler para SIGINT (Ctrl+C)
    signal(SIGINT,                                        // Sinal a ser tratado
           tratador_sigint);                               // Função handler a ser registrada

    /*
    * API uORB Classe Subscribe:
    */

    // Obtém a metadata do tópico uORB do sensor barômetro
    meta = ORB_ID(sensor_baro);

    // Assina o tópico uORB, permitindo múltiplos assinantes
    fd_uorb = orb_subscribe_multi(meta, 0);

    // Verifica se a assinatura falhou
    if (fd_uorb < 0) {

        // Exibe mensagem de erro
        printf("[bmp280_mqtt]: Falha ao assinar tópico uORB: %d\n", fd_uorb);

        // Retorna erro
        return ERROR;

    } // fim if


    /*
    * Prepara a estrutura de 'poll' para o descritor do tópico:
    */

    // Atribui o descritor da assinatura uORB à estrutura de 'poll'
    fds.fd = fd_uorb;

    // Define o evento de interesse como leitura disponível
    fds.events = POLLIN;


    /*
    * Inicializa a conexão MQTT com a plataforma WEGnology:
    */

    // Aguarda IP, abre LED, abre socket, inicializa e conecta o cliente MQTT
    if (mqtt_wnology_iniciar(&s_terminar_execucao) != OK) {

        // Exibe mensagem de erro
        printf("[bmp280_mqtt]: Falha ao iniciar a conexão MQTT\n");

        // Cancela a assinatura do tópico uORB
        orb_unsubscribe(fd_uorb);

        // Retorna erro
        return ERROR;

    } // fim if


    // Marca o instante da "última publicação" como agora, para atrasar a primeira publicação
    ultima_publicacao_s = time(NULL);

    /*
    * Loop principal:
    */

    // Loop principal de leitura, controlado pela flag 's_terminar_execucao'
    while (!s_terminar_execucao) {

        /*
        * Aguarda evento no tópico uORB:
        */

        // Aguarda evento no tópico com timeout de 1 segundo
        ret = poll(&fds,                                              // Estrutura de descritores
                   1,                                                // Número de descritores
                   BMP280_SENSOR_TIMEOUT_MS);                     // Timeout em milissegundos

        // Verifica se 'poll' falhou com erro diferente de interrupção
        if (ret < 0 && errno != EINTR) {

            // Exibe mensagem de erro
            printf("[bmp280_mqtt]: Falha no poll\n");

            // Sai do loop
            break;

        } // fim if

        /*
        * Copia a amostra mais recente do sensor, se houver:
        */

        // Verifica se o descritor está pronto para leitura
        if (ret > 0 && (fds.revents & POLLIN)) {

            // Copia os dados do evento uORB para a estrutura local
            ret = orb_copy(meta,                                         // Metadata do tópico
                           fd_uorb,                                  // Descritor da assinatura
                           &dados);                              // Buffer de destino

            // Verifica se a cópia foi bem-sucedida
            if (ret != OK) {

                // Exibe mensagem de erro
                printf("[bmp280_mqtt]: Falha ao copiar dados do sensor: %d\n", ret);

                // Sai do loop
                break;

            } // fim if

            // Marca que existe amostra válida em memória
            tem_amostra = true;

        } // fim if

        /*
        * Processa tráfego MQTT e reconecta se necessário:
        */

        // Processa tráfego MQTT de entrada e saída (e limpa a fila de mensagens concluídas)
        mqtt_wnology_sincronizar();

        // Verifica se o cliente MQTT está em estado de erro
        if (!mqtt_wnology_ok()) {

            // Exibe mensagem de erro
            printf("[bmp280_mqtt]: Erro MQTT: %s\n", mqtt_wnology_erro_str());

            // Tenta reconectar ao broker
            if (mqtt_wnology_reconectar(&s_terminar_execucao) != OK) {

                // Se não conseguiu reconectar, reinicia o loop (tenta de novo)
                continue;

            } // fim if

        } // fim if

        /*
        * Verifica se é hora de publicar:
        */

        // Obtém o instante atual
        agora_s = time(NULL);

        // Verifica se o intervalo desde a última publicação já decorreu
        if ((agora_s - ultima_publicacao_s) >= intervalo_s) {

            // Verifica se há amostra válida para publicar
            if (tem_amostra) {

                /*
                 * Aplica a correção QNH (relativa ao nível do mar)
                 * Nota didática: o driver uORB publica pressão absoluta (leitura bruta do BMP280). Para obter a
                 * pressão relativa ao nível do mar, multiplicamos por 'FATOR_CORRECAO_QNH'. Ver NOTA CONCEITUAL
                 * no cabeçalho do arquivo para a origem e o significado desse fator.
                 */

                // 
                pressao_qnh = dados.pressure * FATOR_CORRECAO_QNH;

                // Converte a pressão corrigida para centésimos de hPa
                pressao_centesimos = (int)(pressao_qnh * 100.0f + 0.5f);

                // Converte a temperatura para centésimos de grau Celsius
                temperatura_centesimos = (int)(dados.temperature * 100.0f + 0.5f);

                /* 
                 * Formata o payload JSON no formato esperado pelo tópico state da WEGnology.
                 * Nota didática: o 'abs' no resto da divisão é necessário porque, em C, o sinal do resto
                 * segue o sinal do dividendo ('-1 % 100' resulta -1, não 99). Se a temperatura fosse negativa,
                 * por exemplo -40,25 °C, 'temperatura_centesimos' seria -4025, e '-4025 / 100 = -40' com
                 *  '-4025 % 100 = -25'. Sem o 'abs', o JSON sairia '-40.-25' em vez de '-40.25'.
                 */

                 //
                comprimento_json = snprintf(json,                                  // Buffer de destino
                                            sizeof(json),                         // Tamanho do buffer
                                            "{\"data\":{\"pressao\":%d.%02d,"
                                            "\"temperatura\":%d.%02d}}",
                                            pressao_centesimos / 100,                   // Parte inteira da pressão
                                            abs(pressao_centesimos % 100),           // Centésimos da pressão
                                            temperatura_centesimos / 100,              // Parte inteira da temperatura
                                            abs(temperatura_centesimos % 100));      // Centésimos da temperatura

                // Publica o payload no tópico state da plataforma WEGnology
                if (mqtt_wnology_publicar(json, comprimento_json) == MQTT_OK) {

                    // Acende o LED externo por 100 ms para indicar publicação bem-sucedida
                    mqtt_flash_led();

                    // Exibe mensagem informativa
                    printf("[bmp280_mqtt]: Publicado: %s\n", json);

                } // fim if

                else {

                    // Exibe mensagem de erro
                    printf("[bmp280_mqtt]: Falha na publicação: %s\n", mqtt_wnology_erro_str());

                } // fim else

            } // fim if externo

            else {

                // Exibe aviso: não há amostra válida desde a última publicação
                printf("[bmp280_mqtt]: Sem nova amostra do sensor; publicação pulada\n");

            } // fim else

            // Atualiza o instante da última tentativa de publicação
            ultima_publicacao_s = agora_s;

            // Consome a flag de amostra; nova amostra será marcada no próximo 'orb_copy'
            tem_amostra = false;

        } // fim if

    } // fim loop while


    /*
    * Trata fechamentos:
    */

    // Encerra a sessão MQTT, fecha o socket e fecha o LED
    mqtt_wnology_finalizar();

    // Cancela a assinatura do tópico uORB
    orb_unsubscribe(fd_uorb);

    // Retorna sucesso
    return OK;

} // fim função int main(int argc, FAR char *argv[])


//----------------------------------------------------------------------------------------------------------------------
// Fim do arquivo "bmp280_mqtt_main.c"
//----------------------------------------------------------------------------------------------------------------------
