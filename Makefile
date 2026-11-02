########################################################################################################################
#
# Arquivo: /nuttxspace_rp2040/apps/external/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
# FILOSOFIA E PAPEL ESTRUTURAL NA ARQUITETURA DE APLICATIVOS EXTERNOS:
#
# Este Makefile atua como o orquestrador de compilacao dos aplicativos
# localizados em 'apps/external/'. Ele define os subdiretorios que devem ser
# varridos pelo sistema de build GNU Make do NuttX.
#
# FUNCIONAMENTO DO MECANISMO DE BUILD:
#
# - A variavel SUBDIRS lista os diretorios que contem outros Makefiles de
#   aplicativos ou intermediarios.
# - O include de $(APPDIR)/Directory.mk processa recursivamente cada
#   subdiretorio listado.
#
# ESTRATEGIA DE RESOLUCAO DE CAMINHOS:
#
# - O caminho para Directory.mk e resolvido pela variavel APPDIR, definida
#   pelo sistema de build do NuttX.
#
# ESTRUTURA DE ORGANIZACAO:
#
# - O subdiretorio 'Raspberry' agrupa os aplicativos da plataforma RP2040.
# - A adicao de novas plataformas requer a inclusao de novos subdiretorios.
#
########################################################################################################################

# Descricao do menu para o sistema de build
MENUDESC = "Proprietary Applications"

# Inclui as regras para processamento recursivo de subdiretorios
include $(APPDIR)/Directory.mk

# Lista de subdiretorios que contem aplicativos
SUBDIRS = Raspberry

########################################################################################################################
# Fim do arquivo 'Makefile'
########################################################################################################################
