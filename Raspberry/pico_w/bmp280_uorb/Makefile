########################################################################################################################
#
# Arquivo: /nuttxspace_esp32/apps/external/Raspberry/pico_w/bmp280_uorb/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
# FILOSOFIA E PAPEL ESTRUTURAL NA ARQUITETURA DE APLICATIVOS EXTERNOS:
#
# Este Makefile compila o aplicativo 'bmp280_uorb' no modelo GNU Make In-Tree.
# O codigo-fonte do aplicativo reside em 'src/' e utiliza a API uORB do
# NuttX para assinar o topico 'sensor_baro'.
#
# FUNCIONAMENTO DO MECANISMO DE BUILD:
#
# - A variavel PREFIX define o diretorio de saida dos objetos.
# - O diretorio 'built/' e criado automaticamente antes da compilacao.
# - A limpeza remove o diretorio 'built/' e seus artefatos.
# - As variaveis PROGNAME, PRIORITY, STACKSIZE e MODULE sao alimentadas
#   pelas opcoes do menuconfig atraves do Kconfig.
# - O arquivo Application.mk, incluido ao final, utiliza essas variaveis
#   para registrar o aplicativo no NSH e gerar o binario final.
#
########################################################################################################################

# Inclui as definicoes globais do sistema de build de aplicacoes
include $(APPDIR)/Make.defs

# Inclui o caminho para os cabecalhos uORB/sensor
CFLAGS += -I$(APPDIR)/system/uorb

# Define o diretorio onde os fontes do aplicativo estao localizados
VPATH = src

# Fonte principal do aplicativo
MAINSRC = bmp280_uorb_main.c

# Define o diretorio de saida dos objetos. O sufixo '/' e obrigatorio.
PREFIX = built/

# Cria o diretorio 'built' caso ainda nao exista
$(shell mkdir -p built)

# Nome do programa registrado no NSH
PROGNAME = $(CONFIG_PROPRIETARY_BMP280_UORB_PROGNAME)

# Prioridade da tarefa no escalonador do NuttX
PRIORITY = $(CONFIG_PROPRIETARY_BMP280_UORB_PRIORITY)

# Tamanho da pilha alocada para a tarefa do aplicativo
STACKSIZE = $(CONFIG_PROPRIETARY_BMP280_UORB_STACKSIZE)

# Flag que controla se o aplicativo sera compilado
MODULE = $(CONFIG_PROPRIETARY_BMP280_UORB)

# Inclui as regras comuns para compilacao e registro de aplicativos NuttX
include $(APPDIR)/Application.mk

# Regra de limpeza adicional: remove o diretorio 'built' e seus artefatos
clean::
	$(call DELDIR, built)  

########################################################################################################################
# Fim do arquivo 'Makefile'
########################################################################################################################
