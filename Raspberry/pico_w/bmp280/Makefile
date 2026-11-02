########################################################################################################################
#
# Arquivo: /nuttxspace_rp2040/apps/external/Raspberry/pico_w/bmp280/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
# FILOSOFIA E PAPEL ESTRUTURAL NA ARQUITETURA DE APLICATIVOS EXTERNOS:
#
# Este Makefile compila o aplicativo 'bmp280' no modelo GNU Make In-Tree.
# O aplicativo le o sensor BMP280 atraves do device /dev/uorb/sensor_baro0.
#
########################################################################################################################

# Inclui as definicoes globais do sistema de build de aplicacoes
include $(APPDIR)/Make.defs

# Define o diretorio onde os fontes do aplicativo estao localizados
VPATH = src

# Fonte principal do aplicativo
MAINSRC = bmp280_main.c

# Define o diretorio de saida dos objetos. O sufixo '/' e obrigatorio.
PREFIX = built/

# Cria o diretorio 'built' caso ainda nao exista
$(shell mkdir -p built)

# Nome do programa registrado no NSH
PROGNAME = $(CONFIG_PROPRIETARY_BMP280_PROGNAME)

# Prioridade da tarefa no escalonador do NuttX
PRIORITY = $(CONFIG_PROPRIETARY_BMP280_PRIORITY)

# Tamanho da pilha alocada para a tarefa do aplicativo
STACKSIZE = $(CONFIG_PROPRIETARY_BMP280_STACKSIZE)

# Flag que controla se o aplicativo sera compilado
MODULE = $(CONFIG_PROPRIETARY_BMP280)

# Inclui as regras comuns para compilacao e registro de aplicativos NuttX
include $(APPDIR)/Application.mk

# Regra de limpeza adicional: remove o diretorio 'built' e seus artefatos
clean::
	$(call DELDIR, built)

########################################################################################################################
# Fim do arquivo 'Makefile'
########################################################################################################################
