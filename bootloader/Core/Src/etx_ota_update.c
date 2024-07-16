#include <stdio.h>
#include "etx_ota_update.h"
#include "main.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>


uint8_t Rx_Buffer[ETX_OTA_CHUNK_MAX_SIZE] = {0};
uint8_t temp_Buffer[2*ETX_OTA_CHUNK_MAX_SIZE] = {0};
ETX_OTA_STATE_ ota_state = ETX_OTA_STATE_IDLE;
extern te_received_dma_half dma_rec_half;
static uint32_t ota_fw_received_size = 0;
static uint32_t ota_fw_total_size  = 0;
bool is_the_first_chunk = true;
bool is_the_last_chunk = false;
uint32_t max_num_of_chunks = 0;

volatile uint16_t uart1_buf_current_size = 0;

static ETX_OTA_EX_ etx_process_data( uint8_t *buf, uint16_t len );
static HAL_StatusTypeDef write_data_to_flash_app( uint8_t *data,
                                              uint16_t data_len,
										   bool is_full_image );
static uint32_t GetPage(uint32_t Addr);

/**
  * @brief Download the application from UART and flash it.
  * @param None
  * @retval ETX_OTA_EX_OK if no error, otherwise ETX_OTA_EX_ERR
  */
ETX_OTA_EX_ etx_ota_download_and_flash( void )
{
	ETX_OTA_EX_ ret = ETX_OTA_EX_OK;
	uint16_t len = 0;
	char* ota_response[2] = {"ACK0","NACK"} ;

	printf("Reading the file size...\r\n");

	HAL_UART_Receive(&huart1, Rx_Buffer, 10, HAL_MAX_DELAY );

	if((ota_fw_total_size = (uint32_t)atoi((char*)Rx_Buffer)) > 0)
	{
	  max_num_of_chunks =  (uint32_t)floor((double)((float)ota_fw_total_size/ETX_OTA_CHUNK_MAX_SIZE));
	  printf("Total file size %ld + 46 Bytes\r\n",ota_fw_total_size-46); //46 bytes of file end boundary web mark
	  printf("Sending ACK...\r\n");
	  HAL_UART_Transmit(&huart1, (uint8_t *)ota_response[0], sizeof(ota_response[0]), 1000 );//send the ACK
	  printf("Waiting for the chunks...\r\n");

	  HAL_UART_Receive_DMA(&huart1, temp_Buffer, sizeof(temp_Buffer));

	  ota_state = ETX_OTA_STATE_AWAITING_CHUNK;

	}
	else
	{
	  printf("Zero size file received. Sending NACK...\r\n");
	  HAL_UART_Transmit(&huart1, (uint8_t *)ota_response[1], sizeof(ota_response[1]), 100 );
	  return ETX_OTA_EX_ERR;
	}

	do
	{
		if(!is_the_last_chunk)
		{
			while(ota_state != ETX_OTA_STATE_RECEIVED_CHUNK)
			{}
			len = (uint16_t)sizeof(Rx_Buffer);
		}
		else
		{
			HAL_Delay(2000); //wait for the last chunk to be available on dma buffer
			ota_state = ETX_OTA_STATE_RECEIVED_CHUNK;
			len = ota_fw_total_size - ota_fw_received_size;
			printf("Received chunk of length %d\r\n",len);

			if(dma_rec_half == FIRST_HALF)
				ret = etx_process_data( temp_Buffer+ETX_OTA_CHUNK_MAX_SIZE, len );
			else
				ret = etx_process_data( temp_Buffer, len );

			break;
		}
		printf("Received chunk of length %d\r\n",len);
		ret = etx_process_data( Rx_Buffer, len );
	}while( ota_state != ETX_OTA_STATE_END );

	return ret;
}

/**
  * @brief Process the received data from UART1.
  * @param buf buffer to store the received data
  * @param len maximum length to receive
  * @retval ETX_OTA_EX_OK if no error, otherwise ETX_OTA_EX_ERR
  */
static ETX_OTA_EX_ etx_process_data(uint8_t *buf, uint16_t len)
{
	  ETX_OTA_EX_ ret = ETX_OTA_EX_ERR;


	  if( write_data_to_flash_app( buf, len, ( ota_fw_received_size == 0) ) == HAL_OK )
	  {

			if( ota_fw_received_size >= ota_fw_total_size )
			{
			  printf("[%ld/%ld]\r\n", ota_fw_received_size/ETX_OTA_CHUNK_MAX_SIZE,max_num_of_chunks);
			  ota_state = ETX_OTA_STATE_END;
			  return ret = ETX_OTA_EX_OK;
			}



			if(ota_fw_received_size/ETX_OTA_CHUNK_MAX_SIZE < ota_fw_total_size/ETX_OTA_CHUNK_MAX_SIZE)
			{
				printf("[%ld/%ld]\r\n", ota_fw_received_size/ETX_OTA_CHUNK_MAX_SIZE, max_num_of_chunks);
				ota_state = ETX_OTA_STATE_AWAITING_CHUNK;
			}
			else if(ota_fw_received_size/ETX_OTA_CHUNK_MAX_SIZE == ota_fw_total_size/ETX_OTA_CHUNK_MAX_SIZE )
			{
				printf("[%ld/%ld]\r\n", ota_fw_received_size/ETX_OTA_CHUNK_MAX_SIZE, max_num_of_chunks);
				ota_state = ETX_OTA_STATE_AWAITING_CHUNK;
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

/**
  * @brief Write data chunk to the flash memory
  * @param data pointer to the chunk to be flashed
  * @param data_len chunk length to be flashed
  * @param is_first_block check variable to clear the memory only once
  * @retval  HAL_OK, HAL_ERROR, HAL_BUSY,HAL_TIMEOUT
  */
static HAL_StatusTypeDef write_data_to_flash_app( uint8_t *data,uint16_t data_len,bool is_first_block)
{
  HAL_StatusTypeDef ret;
  uint64_t data_temp = 0;
  uint32_t FirstPage = 0, NbOfPages = 0;
  int i;

  do
  {
    ret = HAL_FLASH_Unlock();
    if( ret != HAL_OK )
    {
    	printf("Flash unlock Error\r\n");
    	break;
    }

    if(is_first_block) //No need to erase every time.
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

    for( i = 0; i <= data_len-8; i += 8 )
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
        ota_fw_received_size += 8; //we are writting double words
      }
      else
      {
        printf("Flash Write Error\r\n");
        break;
      }
    }

    if(data_len%8 != 0 && is_the_last_chunk == true) // flash the tail of the code
    {
    	uint8_t cnt1 = 0;
    	for(int cnt = i,cnt1 = 0; cnt<data_len;cnt++,cnt1++)
    	{
    		data_temp = data_temp | ((uint64_t)data[cnt]<<cnt1*8);
    	}

        ret = HAL_FLASH_Program( FLASH_TYPEPROGRAM_DOUBLEWORD,
                                 (ETX_APP_FLASH_ADDR + ota_fw_received_size),
    							   data_temp
                               );

        if( ret == HAL_OK )
        {
        ota_fw_received_size += (data_len-i);
        }
        else
        {
          printf("Flash Write Error\r\n");
          break;
        }
    }

    ret = HAL_FLASH_Lock();

    if( ret != HAL_OK )
    {
    	printf("Flash lock Error\r\n");
    	break;
    }

  }while(false);

  return ret;
}

/**
 * @brief Get the order number given a page address
 * @param Addr memory address for the page
 * @retval page order uint32_t number */
static uint32_t GetPage(uint32_t Addr)
{
  return (Addr - FLASH_BASE) / FLASH_PAGE_SIZE;;
}
