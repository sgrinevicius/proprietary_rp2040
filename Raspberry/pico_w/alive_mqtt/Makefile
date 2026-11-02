########################################################################################################################
#
# Arquivo: /nuttxspace_rp2040/apps/external/Raspberry/pico_w/alive_mqtt/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
# FILOSOFIA E PAPEL ESTRUTURAL NA ARQUITETURA DE APLICATIVOS EXTERNOS:
#
# Este Makefile compila o aplicativo 'alive_mqtt' no fluxo GNU Make In-Tree, redirecionando todos os artefatos de
# compilacao para o diretorio 'built/', mantendo a pasta de codigo-fonte organizada.
#
# FUNCIONAMENTO DO MECANISMO DE BUILD:
#
# - A variavel PREFIX define o diretorio de saida dos objetos.
# - O diretorio 'built/' e criado automaticamente antes da compilacao.
# - A limpeza remove o diretorio 'built/' e todos os seus artefatos.
# - As variaveis PROGNAME, PRIORITY, STACKSIZE e MODULE sao definidas conforme a configuracao do aplicativo.
# - O arquivo Application.mk, incluido ao final, utiliza essas variaveis para registrar o aplicativo no NSH e gerar o
#   binario final.
#
# ESTRATEGIA DE RESOLUCAO DE CAMINHOS:
#
# - O caminho para Application.mk e resolvido pela variavel APPDIR, definida pelo sistema de build do NuttX.
# - O diretorio 'built/' e relativo ao diretorio do aplicativo.
#
########################################################################################################################

# Inclui as definicoes globais do sistema de build de aplicacoes
include $(APPDIR)/Make.defs

# Define o diretorio de saida dos objetos. O sufixo '/' e obrigatorio.
PREFIX = built/

# Cria o diretorio 'built' caso ainda nao exista
$(shell mkdir -p built)

# Nome do programa registrado no NSH
PROGNAME = alive_mqtt

# Prioridade da tarefa
PRIORITY = SCHED_PRIORITY_DEFAULT

# Tamanho da pilha
STACKSIZE = 8192

# Flag que controla a compilacao como modulo
MODULE = $(CONFIG_PROPRIETARY_ALIVE_MQTT)

# Arquivo fonte principal
MAINSRC = alive_mqtt_main.c

# Demais arquivos-fonte do aplicativo
CSRCS = network_utils.c mqtt_utils.c

# Inclui as regras comuns para compilacao e registro de aplicativos NuttX
include $(APPDIR)/Application.mk

# Regra de limpeza adicional: remove o diretorio 'built' e seus artefatos
clean::
	$(call DELDIR, built)

########################################################################################################################
# Fim do arquivo 'Makefile'
########################################################################################################################
