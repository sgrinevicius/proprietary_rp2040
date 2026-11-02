########################################################################################################################
#
# Arquivo: /nuttxspace_rp2040/apps/external/Raspberry/pico_w/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
# FILOSOFIA E PAPEL ESTRUTURAL NA ARQUITETURA DE APLICATIVOS EXTERNOS:
#
# Este Makefile atua como o orquestrador de compilacao dos aplicativos do
# Raspberry Pi Pico W. Ele processa os subdiretorios de aplicativos.
#
# FUNCIONAMENTO DO MECANISMO DE BUILD:
#
# - O include de $(APPDIR)/Directory.mk processa os subdiretorios listados.
# - Os aplicativos sao adicionados à CONFIGURED_APPS pelos seus proprios
#   Make.defs.
#
########################################################################################################################

# Descricao do menu para o sistema de build
MENUDESC = "PICO-W"

# Inclui as regras para processamento recursivo de subdiretorios
include $(APPDIR)/Directory.mk

########################################################################################################################
# Fim do arquivo 'Makefile'
########################################################################################################################
