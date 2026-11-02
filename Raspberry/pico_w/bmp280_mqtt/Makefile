########################################################################################################################
#
# Arquivo: /nuttxspace_rp2040/apps/external/Raspberry/pico_w/bmp280_mqtt/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
# FILOSOFIA E PAPEL ESTRUTURAL NA ARQUITETURA DE APLICATIVOS EXTERNOS:
#
# Este Makefile compila o aplicativo 'bmp280_mqtt' no modelo GNU Make 'In-Tree'.
#
# O codigo-fonte do aplicativo reside em subpastas de 'src/' e utiliza a API uORB do NuttX para assinar o topico
# 'sensor_baro', alem da API MQTT-C para publicar os dados na plataforma WEGnology.
#
# FUNCIONAMENTO DO MECANISMO DE BUILD:
#
# - A variavel PREFIX define o diretorio de saida dos objetos.
# - O diretorio 'built/' e criado automaticamente antes da compilacao.
# - A limpeza remove o diretorio 'built/' e seus artefatos.
# - As variaveis PROGNAME, PRIORITY, STACKSIZE e MODULE sao alimentadas pelas opcoes do menuconfig atraves do
#   Kconfig.
# - O arquivo Application.mk, incluido ao final, utiliza essas variaveis para registrar o aplicativo no NSH e
#   gerar o binario final.
#
# NOTA DIDATICA — SOBRE 'MAINSRC' E 'CSRCS':
#
#   O sistema de build dos aplicativos do NuttX distingue duas categorias de fontes:
#
#     - 'MAINSRC' : fonte que contem a funcao 'main()'. Cada aplicativo tem exatamente um. O build trata
#                   este arquivo de forma especial: sua funcao 'main' e encapsulada para ser chamada quando
#                   o usuario digita o nome do programa (PROGNAME) no NSH.
#
#     - 'CSRCS'   : demais fontes C do aplicativo. Sao compilados normalmente e ligados ao binario junto com
#                   o 'MAINSRC'.
#
#   Neste aplicativo, ha quatro fontes:
#
#     - 'bmp280_mqtt_main.c' : 'main()' — assina uORB, gerencia o loop principal, formata JSON, publica.
#     - 'mqtt_wnology.c'     : gerenciador do ciclo de vida MQTT (conectar, reconectar, finalizar).
#     - 'mqtt_utils.c'       : utilitarios MQTT-C (init, publish, sync) e controle do LED externo.
#     - 'network_utils.c'    : utilitarios de rede (aguardar IP, abrir socket com DNS).
#
#   A separacao entre 'MAINSRC' e 'CSRCS' e o que permite que o mesmo conjunto de modulos de apoio seja
#   reutilizado por outros aplicativos no futuro, trocando apenas o 'MAINSRC'.
#
# NOTA DIDATICA — SOBRE 'VPATH' E A DIVISAO EM SUBPASTAS:
#
#   Os fontes estao organizados em subpastas logicas dentro de 'src/':
#
#     - 'src/main/'    : ponto de entrada do aplicativo.
#     - 'src/mqtt/'    : gerenciador e utilitarios MQTT.
#     - 'src/network/' : utilitarios de rede (IP e socket).
#
#   A variavel 'VPATH' instrui o GNU Make a procurar cada arquivo listado em 'MAINSRC' e 'CSRCS' nos
#   diretorios informados, na ordem da esquerda para a direita. O separador de diretorios no 'VPATH' e o
#   caractere ':' (dois-pontos) — e nao espaco ou virgula. Os objetos resultantes da compilacao continuam
#   caindo em 'built/' com o nome-base de cada fonte (por exemplo, 'mqtt_utils.o'), sem replicar a
#   estrutura de subpastas. Isso significa que dois arquivos com o mesmo nome-base em pastas diferentes
#   colidiriam — por isso, nesta aplicacao, todos os basenames sao unicos.
#
#   A variavel 'CFLAGS' com os tres '-I' garante que os '#include "..."' continuem funcionando sem alteracao
#   nos fontes. Sem esses '-I', o compilador buscaria 'mqtt_utils.h' apenas ao lado do arquivo que o inclui,
#   e nao nos diretorios vizinhos.
#
########################################################################################################################

# Inclui as definicoes globais do sistema de build de aplicacoes
include $(APPDIR)/Make.defs

# Inclui o caminho para os cabecalhos MQTT da biblioteca
CFLAGS += -I$(APPDIR)/system/mqtt

# Inclui os caminhos dos cabecalhos internos do proprio aplicativo (para os '#include "..."')
CFLAGS += -Isrc/main
CFLAGS += -Isrc/mqtt
CFLAGS += -Isrc/network

# Define os diretorios onde os fontes do aplicativo estao localizados (separador ':')
VPATH = src/main:src/mqtt:src/network

# Fonte principal do aplicativo
MAINSRC = bmp280_mqtt_main.c

# Fontes auxiliares do aplicativo (gerenciador MQTT, utilitarios MQTT e utilitarios de rede)
CSRCS = mqtt_wnology.c mqtt_utils.c network_utils.c

# Define o diretorio de saida dos objetos. O sufixo '/' e obrigatorio.
PREFIX = built/

# Cria o diretorio 'built' caso ainda nao exista
$(shell mkdir -p built)

# Nome do programa registrado no NSH
PROGNAME = $(CONFIG_PROPRIETARY_BMP280_MQTT_PROGNAME)

# Prioridade da tarefa no escalonador do NuttX
PRIORITY = $(CONFIG_PROPRIETARY_BMP280_MQTT_PRIORITY)

# Tamanho da pilha alocada para a tarefa do aplicativo
STACKSIZE = $(CONFIG_PROPRIETARY_BMP280_MQTT_STACKSIZE)

# Flag que controla se o aplicativo sera compilado
MODULE = $(CONFIG_PROPRIETARY_BMP280_MQTT)

# Inclui as regras comuns para compilacao e registro de aplicativos NuttX
include $(APPDIR)/Application.mk

# Regra de limpeza adicional: remove o diretorio 'built' e seus artefatos
clean::
	$(call DELDIR, built)

########################################################################################################################
# Fim do arquivo 'Makefile'
########################################################################################################################
