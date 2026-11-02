########################################################################################################################
#
# Arquivo: /nuttxspace_rp2040/apps/external/Raspberry/pico_w/i2c_scanner/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
# FILOSOFIA E PAPEL ESTRUTURAL NA ARQUITETURA DE APLICATIVOS EXTERNOS:
#
# Este Makefile compila o aplicativo 'i2c_scanner' no modelo GNU Make In-Tree.
# O código-fonte reside em 'src/' e os headers locais (se houver) em 'include/'.
#
########################################################################################################################

# Inclui as definicoes globais do sistema de build de aplicacoes
include $(APPDIR)/Make.defs

# Define o diretorio onde os fontes do aplicativo estao localizados
VPATH = src

# Fonte principal do aplicativo
MAINSRC = i2c_scanner_main.c

# Define o diretorio de saida dos objetos
PREFIX = built/

# Cria o diretorio 'built' caso ainda nao exista
$(shell mkdir -p built)

# Nome do programa registrado no NSH
PROGNAME = $(CONFIG_PROPRIETARY_I2C_SCANNER_PROGNAME)

# Prioridade da tarefa no escalonador
PRIORITY = $(CONFIG_PROPRIETARY_I2C_SCANNER_PRIORITY)

# Tamanho da pilha alocada para a tarefa
STACKSIZE = $(CONFIG_PROPRIETARY_I2C_SCANNER_STACKSIZE)

# Flag que controla se o aplicativo sera compilado
MODULE = $(CONFIG_PROPRIETARY_I2C_SCANNER)

# Inclui as regras comuns para compilacao e registro de aplicativos NuttX
include $(APPDIR)/Application.mk

# Regra de limpeza adicional: remove o diretorio 'built' e seus artefatos
clean::
	$(call DELDIR, built)

########################################################################################################################
# Fim do arquivo 'Makefile'
########################################################################################################################
