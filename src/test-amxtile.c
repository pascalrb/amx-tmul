//==============================================================
// Copyright © 2022 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#include <immintrin.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

#define MAX 1024
#define MAX_ROWS 16
#define MAX_COLS 64
#define STRIDE 64
#define ARCH_GET_XCOMP_PERM     0x1022
#define ARCH_REQ_XCOMP_PERM     0x1023
#define XFEATURE_XTILECFG       17
#define XFEATURE_XTILEDATA      18

//Define tile config data structure 
typedef struct __tile_config
{
  uint8_t palette_id;
  uint8_t start_row;
  uint8_t reserved_0[14];
  //uint16_t colsb[16]; 
  
  uint16_t colsb[8]; 
  uint8_t reserved_1[16]; 

  //uint8_t rows[16]; 
  uint8_t rows[8]; 
  uint8_t reserved_2[8]; 
} __tilecfg;

/* Initialize tile config */
static void init_tile_config (__tilecfg *tileinfo)
{
  int i;
  tileinfo->palette_id = 1;
  tileinfo->start_row = 0;

  //for (i = 0; i < 1; ++i)
  //{
  //  // 1 Tile - 16x16
  //  //tileinfo->colsb[i] = MAX_ROWS;
  //  //tileinfo->rows[i] =  MAX_ROWS;
  //}
  // Not used

  for (i = 1; i < 4; ++i)
  {
    // 3 Tiles - each 16x64
    tileinfo->colsb[i] = MAX_COLS;
    tileinfo->rows[i] =  MAX_ROWS;
  }

  _tile_loadconfig (tileinfo);
}

/* Initialize int8_t buffer */
static void init_buffer (int8_t *buf, int8_t value)
{
  int rows, colsb, i, j;
  rows  = MAX_ROWS;
  colsb = MAX_COLS;

  for (i = 0; i < rows; i++)
    for (j = 0; j < colsb; j++)
    {
        buf[i * colsb + j] = value;
    }
}

/* Initialize int32_t buffer */
static void init_buffer32 (int32_t *buf, int32_t value)
{
  int rows, colsb, i, j;
  rows  = MAX_ROWS;
  colsb = MAX_COLS;
  int colsb2=colsb/4;

  for (i = 0; i < rows; i++)
    for (j = 0; j < (colsb2); j++)
    {
        buf[i * colsb2 + j] = value;
    }
}

/* Set_tiledata_use() - Invoke syscall to set ARCH_SET_STATE_USE */
static bool set_tiledata_use()
{
   if (syscall(SYS_arch_prctl, ARCH_REQ_XCOMP_PERM, XFEATURE_XTILEDATA)) 
   {
      printf("\n Fail to do XFEATURE_XTILEDATA \n\n");
      return false;
   }
   else
   {
      printf("\n TILE DATA USE SET - OK \n\n");
      return true;
   }

   return true;
}

/* Print int8_t buffer */
static void print_buffer(int8_t* buf, int32_t rows, int32_t colsb) 
{
   for (int i = 0; i < rows; i++) {
     for (int j = 0; j < (colsb); j++)
     {
         printf("%d ", buf[i * colsb + j]);
     }
     printf("\n");
   }
   printf("\n");
}

/* Print int32_t buffer */
static void print_buffer32(int32_t* buf, int32_t rows, int32_t colsb)
{
   for (int i = 0; i < rows; i++) {
     for (int j = 0; j < (colsb); j++)
     {
         printf("%d ", buf[i * colsb + j]);
     }
     printf("\n");
   }
   printf("\n");
}

int main(){

  __tilecfg tile_data = {0};
  int8_t src1[MAX];
  int8_t src2[MAX];
  int32_t res[MAX/4];
  int rows  = MAX_ROWS;
  int colsb = MAX_COLS;

  clock_t start0 = clock();
  // Request permission to linux kernel to run AMX 
  if (!set_tiledata_use())
     exit(-1);

  // Load tile configuration 
  init_tile_config (&tile_data);

  // Init src matrix buffers with data
  init_buffer (src1, 2);
  //print_buffer(src1, rows, colsb);
 
  init_buffer (src2, 4);
  //print_buffer(src2, rows, colsb);

  // Init dst matrix buffers with data
  init_buffer32 (res, 0);

  // Load tile rows from memory into tile registers
  //_tile_loadd (2, src1, STRIDE);    // Load src1 into Tile2 reg, from *src1, STRIDE bytes at a time
  //_tile_loadd (3, src2, STRIDE);
  //_tile_loadd (1, res, STRIDE);
  printf("here0\n");
  _tile_loadd(1, src1, STRIDE);    // Load src1 into Tile2 reg, from *src1, STRIDE bytes at a time
  printf("here1\n");
  _tile_loadd(2, src2, STRIDE);
  printf("here2\n");
  _tile_loadd(3, res, STRIDE);
  printf("here3\n");

  //clock_t start0 = clock();
  // Compute dot-product of bytes in tiles 
  //_tile_dpbssd (1, 2, 3);
  _tile_dpbssd (3, 2, 1);
  printf("here4\n");
  //clock_t end0 = clock();

  // Store the tile data to memory
  //_tile_stored (1, res, STRIDE);
  _tile_stored (3, res, STRIDE);
  printf("here5\n\n");
  clock_t end0 = clock();

  double mat_mul_amx = (double)(end0-start0)/ CLOCKS_PER_SEC;

  printf("Result AMX:\n");
  print_buffer32(res, rows, colsb/4);

  //Conventional Mat Mul 
  int8_t A[rows][colsb];
  int8_t B[colsb][rows];
  int32_t C[rows][rows];
  clock_t start1 = clock();
  for(int i = 0; i < rows; i++){
   for(int j = 0; j < colsb; j++){
     A[i][j] = 2;
   }
  }
  for(int i = 0; i < colsb; i++){
   for(int j = 0; j < rows; j++){
     B[i][j] = 4;
   }
  }
  //clock_t start1 = clock();
  //Matmul
  for(int i = 0; i < rows; i++){
   for(int j = 0; j < rows; j++){
     C[i][j] = 0;
     for(int k = 0; k < colsb; k++){
       C[i][j] += A[i][k] * B[k][j];
     }
   }
  }
  clock_t end1 = clock();
  double mat_mul_conv = (double)(end1-start1)/ CLOCKS_PER_SEC;

  printf("Result Conventional:\n");
  //Print C
  for(int i = 0; i < rows; i++){
    for(int j = 0; j < rows; j++){
      printf("%d ", C[i][j]);
    }
    printf("\n");
  }
  printf("\n");
  
  printf("Time for AMX:          %lf\n", mat_mul_amx);
  printf("Time for conventional: %lf\n", mat_mul_conv);

  //TODO: 
  //1. understand tileconfig initalization
  //2. Figure out how we can use TILE regs on large containers
  //  (~1MB of data)


  // Release the tile configuration to return to the init state, 
  // which releases all storage it currently holds
  _tile_release ();
}
