########################################################################################################################
#
# Arquivo: /nuttxspace_rp2040/apps/external/Raspberry/pico_w/gpio_blink/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
# FILOSOFIA E PAPEL ESTRUTURAL NA ARQUITETURA DE APLICATIVOS EXTERNOS:
#
# Este Makefile compila o aplicativo 'gpio_blink' no fluxo GNU Make In-Tree, redirecionando todos os artefatos de 
# compilação para o diretório 'built/', mantendo a pasta de código-fonte organizada.
#
# FUNCIONAMENTO DO MECANISMO DE BUILD:
#
# - A variável PREFIX define o diretório de saída dos objetos.
#
# - O diretório 'built/' é criado automaticamente antes da compilação.
#
# - A limpeza remove o diretório 'built/' e todos os seus artefatos.
#
# - As variáveis PROGNAME, PRIORITY, STACKSIZE e MODULE são definidas conforme a configuração do aplicativo.
#
# - O arquivo Application.mk, incluído ao final, utiliza essas variáveis para registrar o aplicativo no NSH e gerar o 
#   binário final.
#
# ESTRATÉGIA DE RESOLUÇÃO DE CAMINHOS:
#
# - O caminho para Application.mk é resolvido pela variável APPDIR, definida pelo sistema de build do NuttX. 
#   Não há necessidade de caminhos absolutos.
#
# - O diretório 'built/' é relativo ao diretório do aplicativo.
#
########################################################################################################################

# Inclui as definições globais do sistema de build de aplicações
include $(APPDIR)/Make.defs

# Define o diretório de saída dos objetos. O sufixo '/' é obrigatório.
PREFIX = built/

# Cria o diretório 'built' caso ainda não exista
$(shell mkdir -p built)

# Nome do programa registrado no NSH
PROGNAME = gpio_blink

# Prioridade da tarefa
PRIORITY = SCHED_PRIORITY_DEFAULT

# Tamanho da pilha
STACKSIZE = 2048

# Flag que controla a compilação como módulo
MODULE = $(CONFIG_PROPRIETARY_GPIO_BLINK)

# Arquivo fonte principal
MAINSRC = gpio_blink_main.c

# Inclui as regras comuns para compilação e registro de aplicativos NuttX
include $(APPDIR)/Application.mk

# Regra de limpeza adicional: remove o diretório 'built' e seus artefatos
clean::
	$(call DELDIR, built)

########################################################################################################################
# Fim do arquivo 'Makefile'
########################################################################################################################
