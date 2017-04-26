/* MiCO Team
 * Copyright (c) 2017 MXCHIP Information Tech. Co.,Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


/* Includes ------------------------------------------------------------------*/
#include "platform_logging.h"
#include "platform_peripheral.h"
#include "platform_mcu_peripheral.h"
#include "mico_board.h"
#include "mico_board_conf.h"

#include "device.h"
#include "pinmap.h"
#include "PeripheralPins.h"
#include "PeripheralPins_Extra.h"

#ifdef USE_QUAD_SPI_FLASH
#include "spi_flash.h"
#include "spi_flash_internal.h"

/* Private constants --------------------------------------------------------*/
/* End of the Flash address */
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

#define sFLASH_SPI_PAGESIZE       0x100

__IO uint8_t *qspi_addr = (__IO uint8_t *)(0x90000000);

QSPI_HandleTypeDef QSPIHandle;

#ifdef USE_QUAD_SPI_DMA
DMA_InitTypeDef DMA_InitStructure;
#endif

/* Private function prototypes -----------------------------------------------*/
static void QSPI_AutoPollingMemReady(QSPI_HandleTypeDef *hqspi);
static void QSPI_WriteEnable(QSPI_HandleTypeDef *hqspi);
static void QSPI_QEEnable(QSPI_HandleTypeDef *hqspi);

static int qsflash_sector_erase ( unsigned long device_address );

static int qsflash_read_ID( sflash_handle_t* const handle, void* const data_addr );
static int qsflash_write_page( const sflash_handle_t* const handle, unsigned long device_address,
                               const void* const data_addr, unsigned int size );

OSStatus qsflash_erase( uint32_t StartAddress, uint32_t EndAddress )
{
    OSStatus err = kNoErr;
    uint32_t StartSector, EndSector, i = 0;

    /* Get the sector where start the user flash area */
    StartSector = StartAddress >> 12;
    EndSector = EndAddress >> 12;

    for ( i = StartSector; i <= EndSector; i += 1 ) {
        require_action( qsflash_sector_erase(i<<12) == kNoErr, exit, err = kWriteErr );
    }

exit:
    return err;
}

static uint32_t _get_size( uint32_t flash_size )
{
    uint32_t count = 0;
    while ( 1 )
    {
        if ( flash_size >>= 1 )
            count++;
        else
            break;
    }
    return count - 1;
}

/**
  * @brief Init qspi flash
  * @param None
  * @retval result
  */
int qsflash_init( /*@out@*/const platform_qspi_t* qspi, const platform_flash_t* flash, /*@out@*/
                  sflash_handle_t* const handle, sflash_write_allowed_t write_allowed_in )
{
    int status;
    device_id_t tmp_device_id;

    /* Peripheral Clock Enable -------------------------------------------------*/
    __HAL_RCC_QSPI_CLK_ENABLE();

    __HAL_RCC_QSPI_FORCE_RESET();
    __HAL_RCC_QSPI_RELEASE_RESET();

    /* Enable DMA1/2 AHB1 clock */
#ifdef USE_QUAD_SPI_DMA
    if( peripheral->dma.controller == DMA1 ) __HAL_RCC_DMA1_CLK_ENABLE();
    else __HAL_RCC_DMA2_CLK_ENABLE();
#endif

    /* QSPI GPIO Configuration -------------------------------------------------*/
    pinmap_pinout( qspi->pin_clock->mbed_pin,   PinMap_QSPI_CLK );
    pinmap_pinout( qspi->pin_cs->mbed_pin,      PinMap_QSPI_CS );
    pinmap_pinout( qspi->pin_d0->mbed_pin,      PinMap_QSPI_D0 );
    pinmap_pinout( qspi->pin_d1->mbed_pin,      PinMap_QSPI_D1 );
    pinmap_pinout( qspi->pin_d2->mbed_pin,      PinMap_QSPI_D2 );
    pinmap_pinout( qspi->pin_d3->mbed_pin,      PinMap_QSPI_D3 );

    /* QSPI CS pin set high */
     //platform_gpio_output_high( qspi->pin_cs );

    /* Initialize QuadSPI ------------------------------------------------------*/
    QSPIHandle.Instance = QUADSPI;
    HAL_QSPI_DeInit(&QSPIHandle);

    QSPIHandle.Init.ClockPrescaler     = 0x01; /* 90 MHZ */
    QSPIHandle.Init.FifoThreshold      = 4;   //?
    QSPIHandle.Init.SampleShifting     = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
    QSPIHandle.Init.FlashSize          = _get_size( flash->flash_length );
    QSPIHandle.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_2_CYCLE;
    QSPIHandle.Init.ClockMode          = QSPI_CLOCK_MODE_0;

    QSPIHandle.Init.FlashID            = qspi->FlashID;
    QSPIHandle.Init.DualFlash          = QSPI_DUALFLASH_DISABLE;

    if (HAL_QSPI_Init(&QSPIHandle) != HAL_OK)
    {
      return -1;
    }

#ifdef USE_QUAD_SPI_DMA
    /* Initialize DMA ----------------------------------------------------------*/
    DMA_StructInit( &DMA_InitStructure );
    DMA_DeInit( peripheral->dma.stream );

    /*DMA configuration*/
    DMA_InitStructure.DMA_BufferSize = 0;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(peripheral->port->DR);
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Channel = peripheral->dma.channel;
    DMA_InitStructure.DMA_Memory0BaseAddr = 0;
    DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_Init( peripheral->dma.stream, &DMA_InitStructure );
#endif

    QSPI_QEEnable( &QSPIHandle );

    /*Delay wait to QE Mode*/
    handle->write_allowed = write_allowed_in;
    /* Read Flash ID */
    status = qsflash_read_ID( handle, &tmp_device_id );

    if ( status != 0 )
    {
        return status;
    }

    handle->device_id = (((uint32_t) tmp_device_id.id[0]) << 16) +
                        (((uint32_t) tmp_device_id.id[1]) << 8) +
                        (((uint32_t) tmp_device_id.id[2]) << 0);

    return 0;
}

/**
  * @brief  read block of data to the FLASH.
  *         
  * @param  data_addr: pointer to the buffer  containing the data to be written
  *         to the FLASH.
  * @param  device_address: FLASH's internal address to write to.
  * @param  size: number of bytes to write to the FLASH.
  * @retval None
  */
int qsflash_read( const sflash_handle_t* const handle, unsigned long device_address, void* const data_addr,
                  unsigned int size )
{
    uint8_t* pAddr;
    QSPI_CommandTypeDef sCommand;
    QSPI_MemoryMappedTypeDef sMemMappedCfg;

    /* Reading Sequence ----------------------------------------------------*/
    sCommand.InstructionMode    = QSPI_INSTRUCTION_1_LINE;
    sCommand.AddressSize        = QSPI_ADDRESS_24_BITS;
    sCommand.AlternateByteMode  = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DdrMode            = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle   = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode           = QSPI_SIOO_INST_EVERY_CMD;

    sCommand.Instruction        = SFLASH_QUAD_READ;
    sCommand.AddressMode        = QSPI_ADDRESS_4_LINES;
    sCommand.DataMode           = QSPI_DATA_4_LINES;
    sCommand.DummyCycles        = DUMMY_CLOCK_CYCLES_READ_QUAD;

    sMemMappedCfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;

    if ( HAL_QSPI_MemoryMapped( &QSPIHandle, &sCommand, &sMemMappedCfg ) != HAL_OK )
    {
        return -1;
    }

    pAddr = (uint8_t*) qspi_addr + device_address;
    memcpy( data_addr, (void *) (pAddr), size );

    HAL_QSPI_Abort(&QSPIHandle);

    return 0;
}


/**
  * @brief  Writes block of data to the FLASH. In this function, the number of
  *         WRITE cycles are reduced, using Page WRITE sequence.
  * @param  data_addr: pointer to the buffer  containing the data to be written
  *         to the FLASH.
  * @param  device_address: FLASH's internal address to write to.
  * @param  size: number of bytes to write to the FLASH.
  * @retval None
  */
int qsflash_write( const sflash_handle_t* const handle, unsigned long device_address, const void* const data_addr, unsigned int size )
{
  int status = 0;
  uint8_t NumOfPage = 0, NumOfSingle = 0, Addr = 0, count = 0, temp = 0;
  unsigned char* data_addr_ptr = (unsigned char*) data_addr;

  Addr = device_address % sFLASH_SPI_PAGESIZE;
  count = sFLASH_SPI_PAGESIZE - Addr;
  NumOfPage =  size / sFLASH_SPI_PAGESIZE;
  NumOfSingle = size % sFLASH_SPI_PAGESIZE;

  if (Addr == 0) /*!< WriteAddr is sFLASH_PAGESIZE aligned  */
  {
    if (NumOfPage == 0) /*!< NumByteToWrite < sFLASH_PAGESIZE */
    {
      status = qsflash_write_page( handle, device_address, data_addr_ptr, size );
      //sFLASH_WritePage(pBuffer, WriteAddr, NumByteToWrite);
    }
    else /*!< NumByteToWrite > sFLASH_PAGESIZE */
    {
      while (NumOfPage--)
      {
        status = qsflash_write_page( handle, device_address, data_addr_ptr, sFLASH_SPI_PAGESIZE );
        //sFLASH_WritePage(pBuffer, WriteAddr, sFLASH_SPI_PAGESIZE);
        device_address +=  sFLASH_SPI_PAGESIZE;
        data_addr_ptr += sFLASH_SPI_PAGESIZE;
      }
      if( NumOfSingle != 0 )
      {
        status = qsflash_write_page( handle, device_address, data_addr_ptr, NumOfSingle);
      }
    }
  }
  else /*!< WriteAddr is not sFLASH_PAGESIZE aligned  */
  {
    if (NumOfPage == 0) /*!< NumByteToWrite < sFLASH_PAGESIZE */
    {
      if (NumOfSingle > count) /*!< (NumByteToWrite + WriteAddr) > sFLASH_PAGESIZE */
      {
        temp = NumOfSingle - count;

        status = qsflash_write_page( handle, device_address, data_addr_ptr, count );
        //sFLASH_WritePage(pBuffer, WriteAddr, count);
        device_address +=  count;
        data_addr_ptr += count;

        status = qsflash_write_page( handle, device_address, data_addr_ptr, temp);
      }
      else
      {
        status = qsflash_write_page(handle, device_address, data_addr_ptr, size);
      }
    }
    else /*!< NumByteToWrite > sFLASH_PAGESIZE */
    {
      size -= count;
      NumOfPage =  size / sFLASH_SPI_PAGESIZE;
      NumOfSingle = size % sFLASH_SPI_PAGESIZE;

      status = qsflash_write_page( handle, device_address, data_addr_ptr, count );
      //sFLASH_WritePage(pBuffer, WriteAddr, count);
      device_address +=  count;
      data_addr_ptr += count;

      while (NumOfPage--)
      {
        status = qsflash_write_page( handle, device_address, data_addr_ptr, sFLASH_SPI_PAGESIZE );
        //sFLASH_WritePage(pBuffer, WriteAddr, sFLASH_SPI_PAGESIZE);
        device_address +=  sFLASH_SPI_PAGESIZE;
        data_addr_ptr += sFLASH_SPI_PAGESIZE;
      }

      if (NumOfSingle != 0)
      {
        status = qsflash_write_page( handle, device_address, data_addr_ptr, NumOfSingle );
        //sFLASH_WritePage(pBuffer, WriteAddr, NumOfSingle);
      }
    }
  }
  return status;
}

/**
  * @brief  Writes block of data to the FLASH. In this function, the number of
  *         WRITE cycles are reduced, using Page WRITE sequence.
  * @param  data_addr: pointer to the buffer  containing the data to be written
  *         to the FLASH.
  * @param  device_address: FLASH's internal address to write to.
  * @param  size: number of bytes to write to the FLASH.
  * @retval None
  */
int qsflash_write_page( const sflash_handle_t* const handle, unsigned long device_address, const void* const data_addr,
                        unsigned int size )
{
    QSPI_CommandTypeDef sCommand;

    if ( handle->write_allowed != SFLASH_WRITE_ALLOWED ) return -1;

    /* Enable write operations ---------------------------------------------*/
    QSPI_WriteEnable( &QSPIHandle );

    /* Writing Sequence ----------------------------------------------------*/
    sCommand.InstructionMode    = QSPI_INSTRUCTION_1_LINE;
    sCommand.AddressSize        = QSPI_ADDRESS_24_BITS;
    sCommand.AlternateByteMode  = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DdrMode            = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle   = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode           = QSPI_SIOO_INST_EVERY_CMD;
    sCommand.Address            = device_address;

    sCommand.Instruction = SFLASH_QUAD_WRITE;
    sCommand.AddressMode = QSPI_ADDRESS_4_LINES;
    sCommand.DataMode = QSPI_DATA_4_LINES;
    sCommand.NbData = size;
    sCommand.DummyCycles = 0;

    if ( HAL_QSPI_Command( &QSPIHandle, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE ) != HAL_OK )
    {
        return -1;
    }

    if ( HAL_QSPI_Transmit( &QSPIHandle, (uint8_t *)data_addr, HAL_QPSI_TIMEOUT_DEFAULT_VALUE ) != HAL_OK )
    {
        return -1;
    }

    /* Configure automatic polling mode to wait for end of program ---------*/
    QSPI_AutoPollingMemReady( &QSPIHandle );

    return 0;
}

/**
  * @brief Erase the selected sector through device_address.
  * @param None
  * @retval None
  */
int qsflash_sector_erase ( unsigned long device_address )
{
    QSPI_CommandTypeDef sCommand;

  /* Enable write operations ---------------------------------------------*/
  QSPI_WriteEnable( &QSPIHandle );

  /* Erasing Sequence ----------------------------------------------------*/
  sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  sCommand.AddressSize       = QSPI_ADDRESS_24_BITS;
  sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
  sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

  sCommand.Instruction       = SFLASH_SECTOR_ERASE;
  sCommand.AddressMode       = QSPI_ADDRESS_1_LINE;
  sCommand.Address           = device_address;
  sCommand.DataMode          = QSPI_DATA_NONE;
  sCommand.DummyCycles       = 0;

  if (HAL_QSPI_Command(&QSPIHandle, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return -1;
  }

  /* Configure automatic polling mode to wait for end of program ---------*/
  QSPI_AutoPollingMemReady( &QSPIHandle );

  return 0;
}

/**
  * @brief Read qspi flash ID
  * @param None
  * @retval result
  */
int qsflash_read_ID( sflash_handle_t* const handle, void* const data_addr )
{
    QSPI_CommandTypeDef sCommand;

    /* Read Volatile Configuration register --------------------------- */
    sCommand.InstructionMode    = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction        = SFLASH_READ_JEDEC_ID;
    sCommand.AddressMode        = QSPI_ADDRESS_NONE;
    sCommand.AlternateByteMode  = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode           = QSPI_DATA_1_LINE;
    sCommand.DummyCycles        = DUMMY_CLOCK_CYCLES_READ;
    sCommand.DdrMode            = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle   = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode           = QSPI_SIOO_INST_ONLY_FIRST_CMD;
    sCommand.NbData             = 3;

    if (HAL_QSPI_Command(&QSPIHandle, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return -1;
    }

    if (HAL_QSPI_Receive(&QSPIHandle, data_addr, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return -1;
    }

    return 0;

}

/**
  * @brief  This function read the SR of the memory and wait the EOP.
  * @param  hqspi: QSPI handle
  * @retval None
  */
static void QSPI_AutoPollingMemReady( QSPI_HandleTypeDef *hqspi )
{
  QSPI_CommandTypeDef     sCommand;
  QSPI_AutoPollingTypeDef sConfig;

  /* Configure automatic polling mode to wait for memory ready ------ */
  sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
  sCommand.Instruction       = SFLASH_READ_STATUS_REGISTER;
  sCommand.AddressMode       = QSPI_ADDRESS_NONE;
  sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  sCommand.DataMode          = QSPI_DATA_1_LINE;
  sCommand.DummyCycles       = 0;
  sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
  sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
  sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

  sConfig.Match           = 0x00;
  sConfig.Mask            = 0x01;
  sConfig.MatchMode       = QSPI_MATCH_MODE_AND;
  sConfig.StatusBytesSize = 1;
  sConfig.Interval        = 0x10;
  sConfig.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;

  if (HAL_QSPI_AutoPolling(&QSPIHandle, &sCommand, &sConfig, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return;
  }
}

/**
  * @brief  This function send a Write Enable and wait it is effective.
  * @param  None
  * @retval None
  */
static void QSPI_WriteEnable( QSPI_HandleTypeDef *hqspi )
{
    QSPI_CommandTypeDef     sCommand;
    QSPI_AutoPollingTypeDef sConfig;

    /* Enable write operations ------------------------------------------ */
    sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction       = SFLASH_WRITE_ENABLE;   //<<<<<<<<<<<<<
    sCommand.AddressMode       = QSPI_ADDRESS_NONE;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode          = QSPI_DATA_NONE;
    sCommand.DummyCycles       = 0;
    sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

    if (HAL_QSPI_Command(&QSPIHandle, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
      return;
    }

    /* Configure automatic polling mode to wait for write enabling ---- */
    sConfig.Match = 0x02;
    sConfig.Mask = 0x02;
    sConfig.MatchMode = QSPI_MATCH_MODE_AND;
    sConfig.StatusBytesSize = 1;
    sConfig.Interval = 0x10;
    sConfig.AutomaticStop = QSPI_AUTOMATIC_STOP_ENABLE;

    sCommand.Instruction = SFLASH_READ_STATUS_REGISTER;
    sCommand.DataMode = QSPI_DATA_1_LINE;

    if ( HAL_QSPI_AutoPolling( &QSPIHandle, &sCommand, &sConfig, HAL_QPSI_TIMEOUT_DEFAULT_VALUE ) != HAL_OK )
    {
        return;
    }
}

/**
 * @brief  This function send a Quad Enable and wait it is effective.
 * @param  None
 * @retval None
 */
static void QSPI_QEEnable( QSPI_HandleTypeDef *hqspi )
{
    uint8_t reg;

    QSPI_CommandTypeDef     sCommand;
    QSPI_AutoPollingTypeDef sConfig;

    /* Read Volatile Configuration register --------------------------- */
    sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    sCommand.Instruction       = SFLASH_READ_STATUS_REGISTER;  //<<<<<<<
    sCommand.AddressMode       = QSPI_ADDRESS_NONE;
    sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode          = QSPI_DATA_1_LINE;
    sCommand.DummyCycles       = 0;
    sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
    sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    sCommand.NbData            = 1;


    if (HAL_QSPI_Command(&QSPIHandle, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return;
    }

    if (HAL_QSPI_Receive(&QSPIHandle, &reg, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return;
    }

    /* Enable write operations ---------------------------------------- */
    QSPI_WriteEnable(&QSPIHandle);

    /* Write Status register (with QE status) -- */
    sCommand.Instruction = SFLASH_WRITE_STATUS_REGISTER;  //<<<<<<<<
    MODIFY_REG( reg, 0x40, (0x1 << POSITION_VAL(0x40)) );

    if ( HAL_QSPI_Command( &QSPIHandle, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE ) != HAL_OK )
    {
        return;
    }

    if ( HAL_QSPI_Transmit( &QSPIHandle, &reg, HAL_QPSI_TIMEOUT_DEFAULT_VALUE ) != HAL_OK )
    {
        return;
    }

    /* Configure automatic polling mode to wait for QE enabling ---- */
    sConfig.Match = 0x40;
    sConfig.Mask = 0x40;
    sConfig.MatchMode = QSPI_MATCH_MODE_AND;
    sConfig.StatusBytesSize = 1;
    sConfig.Interval = 0x10;
    sConfig.AutomaticStop = QSPI_AUTOMATIC_STOP_ENABLE;

    sCommand.Instruction = SFLASH_READ_STATUS_REGISTER;  //<<<<<<<<

    if ( HAL_QSPI_AutoPolling( &QSPIHandle, &sCommand, &sConfig, HAL_QPSI_TIMEOUT_DEFAULT_VALUE ) != HAL_OK )
    {
        return;
    }

}

#endif
