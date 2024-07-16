#include <stdint.h>
#include "usart.h"

#ifndef INC_ETX_OTA_UPDATE_H_
#define INC_ETX_OTA_UPDATE_H_

#define ETX_APP_FLASH_ADDR 0x08040000   //Application's Flash Address

#define ETX_OTA_CHUNK_MAX_SIZE ( 1024)  //Maximum chunk Size
#define FLASH_USER_START_ADDR  ((uint32_t)0x08040000) // Start of user Flash area
#define FLASH_USER_END_ADDR ( (uint32_t)0x0807F000 + 0x00001000U - 1)   // End of user Flash area



typedef enum
{
	FIRST_HALF,
	SECOND_HALF
}te_received_dma_half;


typedef enum
{
  ETX_OTA_EX_OK       = 0,
  ETX_OTA_EX_ERR      = 1,
}ETX_OTA_EX_;


typedef enum
{
  ETX_OTA_STATE_IDLE    ,
  ETX_OTA_STATE_START   ,
  ETX_OTA_STATE_FLASHING ,
  ETX_OTA_STATE_RECEIVED_CHUNK ,
  ETX_OTA_STATE_AWAITING_CHUNK,
  ETX_OTA_STATE_END     ,
}ETX_OTA_STATE_;



ETX_OTA_EX_ etx_ota_download_and_flash( void );
_Bool is_flash_empty(uint32_t start_addr,uint32_t end_addr);
#endif
