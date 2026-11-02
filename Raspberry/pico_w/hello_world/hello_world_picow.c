//######################################################################################################################
// *
// *  Arquivo: "hello_world_picow_main.c"
// *
// *  Título: Teste de Sanidade do Sistema
// *
// *  Objetivo: Validar a integridade do ambiente de desenvolvimento CMake Out-of-Tree, imprimindo uma mensagem de 
// *            confirmação no console NSH.
// *
// *  Design Pattern: Demonstração de Conceito - nenhum modelo adotado
// *
// *  Nome no NSH: 'meu_app'
// *
// *  Autor: DeepSeek
// *  Hardware: ESP32-S3 
// *  RTOS: Apache NuttX 13.0.0
// *
// *--------------------------------------------------------------------------------------------------------------------
// *  Ver  | dd mmm aaaa | Who  | Descrição
// * ======|=============|======|=======================================================================================
// *  1.0.0| 29 Jul 2026 | S.G. | Versão inicial
// *####################################################################################################################
// ********************************************************************************************************************/

/*
 * Inclusão de Diretivas de Compilação - Início ------------------------------------------------------------------------
 */


/*
 * Inclusão de Bibliotecas em Arquivos de Cabeçalho --------------------------------------------------------------------
 */

// Inclui as macros globais de configuração do Kernel NuttX
#include <nuttx/config.h>

// Inclui a biblioteca padrão de entrada e saída em C (printf, etc.)
#include <stdio.h>


//______________________________________________________________________________________________________________________



/*
 ********************************************** Rotina Principal *******************************************************
 */
 


//**********************************************************************************************************************
//*  FUNCTION: int hello_world_picow_main(int argc, char *argv[])
//*
//*  PARAMETERS: int argc = número de argumentos, char *argv[] = vetor de argumentos
//*
//*  DESCRIPTION: Ponto de entrada padrão reescrito pelo CMake do NuttX como nome único.
//*
//*  RETURNS: 0 em caso de sucesso.
//*
//*********************************************************************************************************************/
int hello_world_picow_main(int argc, char *argv[]) {

  /*
  * Mensagem:
  */
  
  // Imprime no Terminal Minicon (NSH) a linha superior do cabeçalho
  printf("\n=======================================================================\n");
  printf("  Teste de Sanidade Raspberry Pi Pico W: Ambiente GNUMake 'In-Tree' OK!  \n");
  printf("========================================================================\n\n");

  // Retorna sucesso para o Kernel
  return 0;

  
} // fim main


//______________________________________________________________________________________________________________________



//--------------------------------------------------------------------------------------------------------------------*/
// Fim do arquivo "hello_world_picow_main.c"
//--------------------------------------------------------------------------------------------------------------------*/
