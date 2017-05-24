/**
*********************************************************************************************************
*               Copyright(c) 2015, Realtek Semiconductor Corporation. All rights reserved.
**********************************************************************************************************
* @file		main.c
* @brief	      This file provides demo code of LCD display.
* @details
* @author	elliot chen
* @date 		2016-02-24
* @version	v1.0
*********************************************************************************************************
*/
/*Defines--------------------------------------------------------------------*/
#define EN_DBG                  1
#define SPI0_SCK				P4_0		 
#define SPI0_MOSI				P4_1
#define SPI0_MISO				P4_2		 
#define SPI0_CS				    P4_3

/* Includes ------------------------------------------------------------------*/
#include "rtl876x_rcc.h"
#include "rtl876x_gdma.h"
#include "rtl876x_spi.h"
#include "rtl876x_nvic.h"
#include "rtl876x_pinmux.h"
#include "trace.h"
#include "lcd_application.h"

void RCC_Configuration(void)
{
	/* turn on GDMA and SPI clock */
    RCC_PeriphClockCmd(APBPeriph_GDMA, APBPeriph_GDMA_CLOCK, ENABLE);
    RCC_PeriphClockCmd(APBPeriph_SPI0, APBPeriph_SPI0_CLOCK, ENABLE);
}

void SPI_PINMUXConfiguration(void)
{
    Pinmux_Config(SPI0_SCK, SPI0_CLK_Master);
	Pinmux_Config(SPI0_MOSI, SPI0_MO_Master);
	Pinmux_Config(SPI0_MISO, SPI0_MI_Master);
	Pinmux_Config(SPI0_CS, SPI0_SS_N_0_Master);
}

void SPI_PADConfiguration(void)
{
    Pad_Config(SPI0_SCK, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_ENABLE, PAD_OUT_HIGH);
	Pad_Config(SPI0_MOSI, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_ENABLE, PAD_OUT_HIGH);
	Pad_Config(SPI0_MISO, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_ENABLE, PAD_OUT_HIGH);
	Pad_Config(SPI0_CS, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_ENABLE, PAD_OUT_HIGH);
}

void SPI_InitConfiguration(void)
{
	SPI_InitTypeDef  SPI_InitStructure;
	
    SPI_StructInit(&SPI_InitStructure);   
    SPI_InitStructure.SPI_Direction   = SPI_Direction_FullDuplex;
    SPI_InitStructure.SPI_Mode        = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize    = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL        = SPI_CPOL_High;
    SPI_InitStructure.SPI_CPHA        = SPI_CPHA_2Edge;
    SPI_InitStructure.SPI_ClockSpeed  = 8000000;
    SPI_InitStructure.SPI_FrameFormat = SPI_Frame_Motorola;
	SPI_InitStructure.SPI_NDF 		  = 0;
	SPI_Init(SPI0, &SPI_InitStructure); 
	
 	SPI_Cmd(SPI0, ENABLE);
}


/*-----------------------------GD25Q40 driver -------------------------------*/
void GD25Q32_ReadFlashID(void)
{
	uint8_t len = 0;
	uint8_t idx = 0;
	uint8_t sendBuf[4] = {0x9f, 0, 0, 0};
	uint8_t recvBuf[4];
	
	SPI_SendBuffer(SPI0, sendBuf, 4);
	
	while(SPI_GetFlagState(SPI0, SPI_FLAG_BUSY) == SET);
	len = SPI_GetRxFIFOLen(SPI0);
	while(len--)
	{
		recvBuf[idx++] = SPI_ReceiveData(SPI0);
	}
	
#if EN_DBG
		//DBG_BUFFER(MODULE_APP,LEVEL_INFO,"GD25Q32 ID = %d,%d,%d",3 ,recvBuf[1], recvBuf[2], recvBuf[3]);
		DBG_DIRECT("GD25Q32 ID = 0x%x,0x%x,0x%x!", recvBuf[1], recvBuf[2], recvBuf[3]);
#endif
}

void GD25Q32_WriteEnable(void)
{
	uint8_t len = 0;
	SPI_SendData(SPI0,0x06);
	while(SPI_GetFlagState(SPI0, SPI_FLAG_BUSY) == SET);

	len = SPI_GetRxFIFOLen(SPI0);
	while(len--)
	{
		SPI_ReceiveData(SPI0);
	}
}

void GD25Q32_WriteStatusRegister(uint16_t data)
{
	uint8_t len = 0;
	uint8_t sendBuf[3] = {0x01, 0, 0};
	
	/* Enable write */
	GD25Q32_WriteEnable();
	
	sendBuf[1] = data & 0xff;
	sendBuf[2] = (data>>8) & 0xff;
	/* Write status register */
	SPI_SendBuffer(SPI0, sendBuf, 3);
	
	while(SPI_GetFlagState(SPI0, SPI_FLAG_BUSY) == SET);
	len = SPI_GetRxFIFOLen(SPI0);
	while(len--)
	{
		SPI_ReceiveData(SPI0);
	}
}

void GD25Q32_WriteData(uint8_t *psendBuf, uint16_t len)
{
	uint16_t recvlen = 0;
	uint8_t recdata[2];
	uint8_t senBuf[2] = {0x05, 0};
	uint8_t idx = 0;
	
	/* Enable write */
	GD25Q32_WriteEnable();

	/* Write data */
	SPI_SendBuffer(SPI0, psendBuf, len);
	
	while(SPI_GetFlagState(SPI0, SPI_FLAG_BUSY) == SET);
	recvlen = SPI_GetRxFIFOLen(SPI0);
	while(recvlen--)
	{
		SPI_ReceiveData(SPI0);
	}

	/* Read flash status register */
	do
	{
		GD25Q32_WriteEnable();

		SPI_SendBuffer(SPI0, senBuf, 2);
		while(SPI_GetFlagState(SPI0, SPI_FLAG_BUSY) == SET);

		recvlen = SPI_GetRxFIFOLen(SPI0);
		DBG_DIRECT("Recvlen = 0x%x!", recvlen);

		for(idx=0; idx<recvlen; idx++)
		{
			recdata[idx] = SPI_ReceiveData(SPI0);
			DBG_DIRECT("GD25Q32 status = 0x%x", recdata[idx]);
		}
#if 0
		while(recvlen--)
		{
			recdata[idx++] = SPI_ReceiveData(SPI0);
		}
		DBG_DIRECT("GD25Q32 status = 0x%x, 0x%x!", recdata[0], recdata[1]);
#endif

	}while(recdata[1] & 0x01);

	
}

void GD25Q32_Init(void)
{
	GD25Q32_ReadFlashID();
    GD25Q32_ReadFlashID();	
	GD25Q32_WriteStatusRegister(0x02);
}

void GD25Q32_TestCode(void)
{
	uint8_t senBuf[256] = {0x02, 0x5f, 0x00, 0x00};
	uint16_t idx = 0;

	for(idx=4; idx<256; idx++)
	{
		senBuf[idx] = idx - 3;
	}
	
	GD25Q32_Init();
   
	//GD25Q32_WriteData(senBuf, 256);
}


/*----------------------------------test GDMA SPI TX-----------------------------------*/
void GDMA_TXConfiguration(void)    
{
    SPI_InitTypeDef  SPI_InitStructure;
	GDMA_InitTypeDef GDMA_InitStruct;
	uint8_t GDMA_TestBuffer[256] = {0x02, 0x5f, 0x00, 0x00};
    uint16_t idx = 0;
    for (idx=4; idx<255; idx++)
    {
        GDMA_TestBuffer[idx] = idx - 3; 
    }
	
	SPI_Cmd(SPI0, DISABLE);
    SPI_DeInit(SPI0);
	RCC_PeriphClockCmd(APBPeriph_SPI0, APBPeriph_SPI0_CLOCK, ENABLE);
	/*-----------------------SPI init----------------------------*/
    SPI_StructInit(&SPI_InitStructure);   
    SPI_InitStructure.SPI_Direction   = SPI_Direction_FullDuplex;
    SPI_InitStructure.SPI_Mode        = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize    = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL        = SPI_CPOL_High;
    SPI_InitStructure.SPI_CPHA        = SPI_CPHA_2Edge;
    SPI_InitStructure.SPI_ClockSpeed  = 8000000;
    SPI_InitStructure.SPI_FrameFormat = SPI_Frame_Motorola;    

	/*-----------------------GDMA init--------------------------*/
    GDMA_StructInit(&GDMA_InitStruct);
        
	GDMA_InitStruct.GDMA_ChannelNum      = 0;
  	GDMA_InitStruct.GDMA_DIR 			 = GDMA_DIR_MemoryToPeripheral;
  	GDMA_InitStruct.GDMA_BufferSize 	 = 100;// total transfer data size: 100*GDMA_SourceDataSize
  	GDMA_InitStruct.GDMA_SourceInc       = DMA_SourceInc_Inc;
	GDMA_InitStruct.GDMA_DestinationInc  = DMA_DestinationInc_Fix;
    GDMA_InitStruct.GDMA_SourceDataSize  = GDMA_DataSize_Byte;
	GDMA_InitStruct.GDMA_DestinationDataSize = GDMA_DataSize_Byte;
	GDMA_InitStruct.GDMA_SourceMsize	  = GDMA_Msize_8;
	GDMA_InitStruct.GDMA_DestinationMsize = GDMA_Msize_8;   
	GDMA_InitStruct.GDMA_SourceAddr 	 = (uint32_t)GDMA_TestBuffer;
	GDMA_InitStruct.GDMA_DestinationAddr = (uint32_t)SPI0->DR;
	GDMA_InitStruct.GDMA_TransferType    = GDMA_TransferType_SPI0_TX;
	GDMA_InitStruct.GDMA_ChannelPriority = 6;//channel prority between 0 to 6
    
    GDMA_Init(GDMA_Channel0, &GDMA_InitStruct);
    SPI_Init(SPI0, &SPI_InitStructure);   
 	SPI_Cmd(SPI0, ENABLE);
    
    /*-------------------GDMA IRQ configuration-------------------*/
    NVIC_InitTypeDef NVIC_InitStruct;
	NVIC_InitStruct.NVIC_IRQChannel = GDMA0_CHANNEL0_IRQ;
	NVIC_InitStruct.NVIC_IRQChannelPriority = 1;
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStruct);
    GDMA_INTConfig(0, GDMA_INT_Transfer,ENABLE);
    
    /*send data if enable*/
    GDMA_Cmd(0, ENABLE);
}

int main(void)
{
#if 1
    LCD_Demo();
#else
    RCC_Configuration();
    SPI_PINMUXConfiguration();
    SPI_PADConfiguration();
	SPI_InitConfiguration();
	GD25Q32_Init();
    GDMA_TXConfiguration();
#endif
    while(1);
}

