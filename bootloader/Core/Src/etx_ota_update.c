#include <stdio.h>
#include "etx_ota_update.h"
#include "main.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

/* Buffer to hold the received data */
uint8_t Rx_Buffer[ETX_OTA_PACKET_MAX_SIZE] = {'\0'};
extern DMA_HandleTypeDef hdma_usart1_rx;
/* OTA State */
static ETX_OTA_STATE_ ota_state = ETX_OTA_STATE_IDLE;
/* Firmware Total Size that we are going to receive */
static uint32_t ota_fw_total_size;
/* Firmware Size that we have received */
static uint32_t ota_fw_received_size = 0;
bool is_the_first_chunk = true;
bool is_the_last_chunk = false;

volatile uint16_t uart1_buf_current_size = 0;

static ETX_OTA_EX_ etx_process_data( uint8_t *buf, uint16_t len );
static HAL_StatusTypeDef write_data_to_flash_app( uint8_t *data,
                                        uint16_t data_len, bool is_full_image );
static uint32_t GetPage(uint32_t Addr);

/**
  * @brief Download the application from UART and flash it.
  * @param None
  * @retval ETX_OTA_EX_
  */
ETX_OTA_EX_ etx_ota_download_and_flash( void )
{
	ETX_OTA_EX_ ret = ETX_OTA_EX_OK;
	uint16_t len = 0;
	char* ota_response[2] = {"ACK0","NACK"} ;

	printf("Reading the file size ...\r\n");

	HAL_UART_Receive(&huart1, Rx_Buffer, 10, HAL_MAX_DELAY );

	if((ota_fw_total_size = (uint32_t)atoi((char*)Rx_Buffer)) > 0)
	{
	  printf("Total file size %ld Bytes\r\n",ota_fw_total_size);
	  printf("Sending ACK...\r\n");
	  printf("Waiting for the chunks...\r\n");
	  HAL_UART_Transmit(&huart1, (uint8_t *)ota_response[0], sizeof(ota_response[0]), HAL_MAX_DELAY );//send the ACK
	}
	else
	{
	  printf("Sending NACK...\r\n");
	  HAL_UART_Transmit(&huart1, (uint8_t *)ota_response[1], sizeof(ota_response[1]), HAL_MAX_DELAY );
	  return ETX_OTA_EX_ERR;
	}

	do
	{
		if(!is_the_last_chunk)
		{
			HAL_UART_Receive(&huart1, Rx_Buffer, ETX_OTA_PACKET_MAX_SIZE, HAL_MAX_DELAY );
			len = (uint16_t)sizeof(Rx_Buffer);
		}
		else
		{
			HAL_UART_Receive(&huart1, Rx_Buffer, 642, HAL_MAX_DELAY );//todo: generalize this
			len = 642;
		}
		if( len <= ETX_OTA_PACKET_MAX_SIZE && len>0)
		{
		  printf("Received chunk of length %d\r\n",len);
		  ret = etx_process_data( Rx_Buffer, len );
		  memset(Rx_Buffer,'\0',ETX_OTA_PACKET_MAX_SIZE);
		  printf("Sending ACK...\r\n");
		  HAL_UART_Transmit(&huart1, (uint8_t *)ota_response[0], sizeof(ota_response[1]), HAL_MAX_DELAY );
		}
		else
		{
		  printf("No chunk received!\r\n");
		  return ETX_OTA_EX_ERR;
		}
	}while( ota_state != ETX_OTA_STATE_END );
  return ret;
}

/**
  * @brief Process the received data from UART1.
  * @param buf buffer to store the received data
  * @param max_len maximum length to receive
  * @retval ETX_OTA_EX_
  */
static ETX_OTA_EX_ etx_process_data( uint8_t *buf, uint16_t len )
{
	  ETX_OTA_EX_ ret = ETX_OTA_EX_ERR;

	  if( write_data_to_flash_app( buf, len, ( ota_fw_received_size == 0) ) == HAL_OK )
	  {

			if( ota_fw_received_size >= 19028 )//todo: generalize this

//			if( ota_fw_received_size >= ota_fw_total_size )
			{
			  ota_state = ETX_OTA_STATE_END;
			  return ret = ETX_OTA_EX_OK;
			}



			if(ota_fw_received_size/ETX_OTA_DATA_MAX_SIZE < ota_fw_total_size/ETX_OTA_DATA_MAX_SIZE)
			{
				printf("[%ld/%ld]\r\n", ota_fw_received_size/ETX_OTA_DATA_MAX_SIZE, ota_fw_total_size/ETX_OTA_DATA_MAX_SIZE);
			}
			else if(ota_fw_received_size/ETX_OTA_DATA_MAX_SIZE == ota_fw_total_size/ETX_OTA_DATA_MAX_SIZE )
			{
				printf("[%ld/%ld]\r\n", ota_fw_received_size/ETX_OTA_DATA_MAX_SIZE, ota_fw_total_size/ETX_OTA_DATA_MAX_SIZE);
				is_the_last_chunk = true;
			}


			ret = ETX_OTA_EX_OK;
	  }
	  else
	  {
			ret = ETX_OTA_EX_ERR;
	  }
	  return ret;
}


static HAL_StatusTypeDef write_data_to_flash_app( uint8_t *data,
                                        	  uint16_t data_len,
											 bool is_first_block)
{
  HAL_StatusTypeDef ret;
  uint64_t data_temp = 0;
  uint32_t FirstPage = 0, NbOfPages = 0;

  do
  {
    ret = HAL_FLASH_Unlock();
    if( ret != HAL_OK )
    {
      break;
    }

    //No need to erase every time. Erase only the first time.
    if( is_first_block )
    {
       ota_state = ETX_OTA_STATE_FLASHING;
       printf("Erasing the Flash memory...\r\n");

       __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);

       FirstPage = GetPage(FLASH_USER_START_ADDR);

       NbOfPages = GetPage(FLASH_USER_END_ADDR) - FirstPage + 1;


       FLASH_EraseInitTypeDef EraseInitStruct;
       uint32_t SectorError;

       EraseInitStruct.TypeErase       = FLASH_TYPEERASE_PAGES;
       EraseInitStruct.Page            = FirstPage;
       EraseInitStruct.NbPages         = NbOfPages;

      ret = HAL_FLASHEx_Erase( &EraseInitStruct, &SectorError );

      if( ret != HAL_OK )
      {
    	  printf("Flash Erase Error\r\n");
    	  break;
      }

      printf("Flash memory successfuly erased...\r\n");
    }

//    ota_fw_received_size += data_len;
    for(int i = 0; i <= data_len-8; i += 8 )
    {
      data_temp = (uint64_t)data[i+7]<<56 |
    		  	  (uint64_t)data[i+6]<<48 |
				  (uint64_t)data[i+5]<<40 |
				  (uint64_t)data[i+4]<<32 |
				  (uint64_t)data[i+3]<<24 |
				  (uint64_t)data[i+2]<<16 |
				  (uint64_t)data[i+1]<<8  |
				  (uint64_t)data[i]  <<0  ;

      ret = HAL_FLASH_Program( FLASH_TYPEPROGRAM_DOUBLEWORD,
                               (ETX_APP_FLASH_ADDR + ota_fw_received_size),
							   data_temp
                             );
      if( ret == HAL_OK )
      {
        //update the data count
        ota_fw_received_size += 8;
      }
      else
      {
        printf("Flash Write Error\r\n");
        break;
      }
    }

    if( ret != HAL_OK )
    {
      break;
    }

    ret = HAL_FLASH_Lock();
    if( ret != HAL_OK )
    {
      break;
    }

  }while( false );

  return ret;
}

static uint32_t GetPage(uint32_t Addr)
{
  return (Addr - FLASH_BASE) / FLASH_PAGE_SIZE;;
}
