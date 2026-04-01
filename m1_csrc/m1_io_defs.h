/* See COPYING.txt for license details. */

/*
*
*  m1_io_defs.h
*
*  M1 GPIO definitions
*
* M1 Project
*
*/

#ifndef M1_IO_DEFS_H_
#define M1_IO_DEFS_H_

#include "stm32h5xx_hal.h"

#define PE2_Pin 							GPIO_PIN_2
#define PE2_GPIO_Port 						GPIOE
#define FG_INT_Pin 							GPIO_PIN_3
#define FG_INT_GPIO_Port					GPIOE
#define PE4_Pin								GPIO_PIN_4
#define PE4_GPIO_Port						GPIOE
#define PE5_Pin								GPIO_PIN_5
#define PE5_GPIO_Port						GPIOE
#define PE6_Pin								GPIO_PIN_6
#define PE6_GPIO_Port						GPIOE
#define BUTTON_BACK_Pin 					GPIO_PIN_13
#define BUTTON_BACK_GPIO_Port				GPIOC
#define NFC_CS_Pin 							GPIO_PIN_0
#define NFC_CS_GPIO_Port 					GPIOC
#define ESP32_DATAREADY_Pin 				GPIO_PIN_1
#define ESP32_DATAREADY_GPIO_Port 			GPIOC
#define PC2_Pin 							GPIO_PIN_2
#define PC2_GPIO_Port 						GPIOC
#define PC3_Pin 							GPIO_PIN_3
#define PC3_GPIO_Port 						GPIOC
#define ESP32_RX_Pin 						GPIO_PIN_0
#define ESP32_RX_GPIO_Port 					GPIOA
#define ESP32_TX_Pin 						GPIO_PIN_1
#define ESP32_TX_GPIO_Port 					GPIOA
#define RFID_PULL_Pin 						GPIO_PIN_2
#define RFID_PULL_GPIO_Port 				GPIOA
#define RFID_RF_IN_Pin 						GPIO_PIN_3
#define RFID_RF_IN_GPIO_Port 				GPIOA
#define Display_RST_Pin 					GPIO_PIN_4
#define Display_RST_GPIO_Port 				GPIOA
#define Display_DI_Pin 						GPIO_PIN_6
#define Display_DI_GPIO_Port 				GPIOA
#define IR_RX_Pin 							GPIO_PIN_4
#define IR_RX_GPIO_Port 					GPIOC
#define IR_DRV_Pin 							GPIO_PIN_5
#define IR_DRV_GPIO_Port 					GPIOC
#define RFID_OUT_Pin 						GPIO_PIN_0
#define RFID_OUT_GPIO_Port 					GPIOB
#define EN_EXT_3V3_Pin 						GPIO_PIN_1
#define EN_EXT_3V3_GPIO_Port 				GPIOB
#define ESP32_SPI3_MOSI_Pin 				GPIO_PIN_2
#define ESP32_SPI3_MOSI_GPIO_Port 			GPIOB
#define SI4463_RFSW1_Pin 					GPIO_PIN_7
#define SI4463_RFSW1_GPIO_Port 				GPIOE
#define SI4463_GPIO1_Pin 					GPIO_PIN_8
#define SI4463_GPIO1_GPIO_Port 				GPIOE
#define SI4463_GPIO0_Pin 					GPIO_PIN_9
#define SI4463_GPIO0_GPIO_Port 				GPIOE
#define BUTTON_OK_Pin 						GPIO_PIN_10
#define BUTTON_OK_GPIO_Port 				GPIOE
#define BUTTON_UP_Pin 						GPIO_PIN_11
#define BUTTON_UP_GPIO_Port 				GPIOE
#define BUTTON_LEFT_Pin 					GPIO_PIN_12
#define BUTTON_LEFT_GPIO_Port 				GPIOE
#define BUTTON_RIGHT_Pin 					GPIO_PIN_13
#define BUTTON_RIGHT_GPIO_Port 				GPIOE
#define BUTTON_DOWN_Pin 					GPIO_PIN_14
#define BUTTON_DOWN_GPIO_Port 				GPIOE
#define SI4463_RFSW2_Pin 					GPIO_PIN_15
#define SI4463_RFSW2_GPIO_Port 				GPIOE
#define ESP32_SPI3_NSS_Pin 					GPIO_PIN_10
#define ESP32_SPI3_NSS_GPIO_Port 			GPIOB
#define SI4463_nINT_Pin 					GPIO_PIN_12
#define SI4463_nINT_GPIO_Port 				GPIOB
#define SPI_R_SCK_Pin 						GPIO_PIN_13
#define SPI_R_SCK_GPIO_Port 				GPIOB
#define SPI_R_MISO_Pin 						GPIO_PIN_14
#define SPI_R_MISO_GPIO_Port 				GPIOB
#define SPI_R_MOSI_Pin 						GPIO_PIN_15
#define SPI_R_MOSI_GPIO_Port 				GPIOB
#define SI4463_SW_V2_Pin 					GPIO_PIN_8
#define SI4463_SW_V2_GPIO_Port 				GPIOD
#define SI4463_SW_V1_Pin 					GPIO_PIN_9
#define SI4463_SW_V1_GPIO_Port 				GPIOD
#define SI4463_CS_Pin 						GPIO_PIN_10
#define SI4463_CS_GPIO_Port 				GPIOD
#define SI4463_ENA_Pin 						GPIO_PIN_11
#define SI4463_ENA_GPIO_Port 				GPIOD
#define PD12_Pin 							GPIO_PIN_12
#define PD12_GPIO_Port 						GPIOD
#define PD13_Pin 							GPIO_PIN_13
#define PD13_GPIO_Port 						GPIOD
#define EN_EXT_5V_Pin 						GPIO_PIN_14
#define EN_EXT_5V_GPIO_Port 				GPIOD
#define SD_DETECT_Pin 						GPIO_PIN_15
#define SD_DETECT_GPIO_Port 				GPIOD
#define USBC_INT_Pin 						GPIO_PIN_6
#define USBC_INT_GPIO_Port 					GPIOC
#define SPK_CTRL_Pin 						GPIO_PIN_7
#define SPK_CTRL_GPIO_Port 					GPIOC
#define SDIO1_D0_Pin 						GPIO_PIN_8
#define SDIO1_D0_GPIO_Port 					GPIOC
#define SDIO1_D1_Pin 						GPIO_PIN_9
#define SDIO1_D1_GPIO_Port 					GPIOC
#define ESP32_EN_Pin 						GPIO_PIN_8
#define ESP32_EN_GPIO_Port 					GPIOA
#define UART_1_TX_Pin 						GPIO_PIN_9
#define UART_1_TX_GPIO_Port 				GPIOA
#define UART_1_RX_Pin 						GPIO_PIN_10
#define UART_1_RX_GPIO_Port 				GPIOA
#define USB_DN_Pin 							GPIO_PIN_11
#define USB_DN_GPIO_Port 					GPIOA
#define USB_DP_Pin 							GPIO_PIN_12
#define USB_DP_GPIO_Port 					GPIOA
#define SWDIO_Pin 							GPIO_PIN_13
#define SWDIO_GPIO_Port 					GPIOA
#define SWCLK_Pin 							GPIO_PIN_14
#define SWCLK_GPIO_Port 					GPIOA
#define RF_CARRIER_Pin 						GPIO_PIN_15
#define RF_CARRIER_GPIO_Port 				GPIOA
#define SDIO1_D2_Pin 						GPIO_PIN_10
#define SDIO1_D2_GPIO_Port 					GPIOC
#define SDIO1_D3_Pin 						GPIO_PIN_11
#define SDIO1_D3_GPIO_Port 					GPIOC
#define SDIO1_CK_Pin 						GPIO_PIN_12
#define SDIO1_CK_GPIO_Port 					GPIOC
#define PD0_Pin 							GPIO_PIN_0
#define PD0_GPIO_Port 						GPIOD
#define PD1_Pin 							GPIO_PIN_1
#define PD1_GPIO_Port 						GPIOD
/* FDCAN1 aliases for J7 (X10) CAN bus header */
#define FDCAN1_RX_Pin						PD0_Pin
#define FDCAN1_RX_GPIO_Port					PD0_GPIO_Port
#define FDCAN1_TX_Pin						PD1_Pin
#define FDCAN1_TX_GPIO_Port					PD1_GPIO_Port
#define SDIO1_CMD_Pin 						GPIO_PIN_2
#define SDIO1_CMD_GPIO_Port 				GPIOD
#define PD3_Pin 							GPIO_PIN_3
#define PD3_GPIO_Port 						GPIOD
#define SI4463_GPIO3_Pin 					GPIO_PIN_4
#define SI4463_GPIO3_GPIO_Port 				GPIOD
#define SI4463_GPIO2_Pin 					GPIO_PIN_5
#define SI4463_GPIO2_GPIO_Port 				GPIOD
#define BAT_OTG_Pin 						GPIO_PIN_6
#define BAT_OTG_GPIO_Port 					GPIOD
#define ESP32_HANDSHAKE_Pin 				GPIO_PIN_7
#define ESP32_HANDSHAKE_GPIO_Port 			GPIOD
#define ESP32_SPI3_SCK_Pin 					GPIO_PIN_3
#define ESP32_SPI3_SCK_GPIO_Port 			GPIOB
#define ESP32_SPI3_MISO_Pin 				GPIO_PIN_4
#define ESP32_SPI3_MISO_GPIO_Port 			GPIOB
#define Display_CS_Pin 						GPIO_PIN_5
#define Display_CS_GPIO_Port 				GPIOB
#define I2C_SCL_Pin 						GPIO_PIN_6
#define I2C_SCL_GPIO_Port 					GPIOB
#define I2C_SDA_Pin 						GPIO_PIN_7
#define I2C_SDA_GPIO_Port 					GPIOB
#define I2C_INT_Pin 						GPIO_PIN_8
#define I2C_INT_GPIO_Port 					GPIOB
#define EN_EXT2_5V_Pin 						GPIO_PIN_9
#define EN_EXT2_5V_GPIO_Port 				GPIOB
#define NFC_IRQ_Pin 						GPIO_PIN_0
#define NFC_IRQ_GPIO_Port 					GPIOE

#define GPIO_DATA_READY_Pin			ESP32_DATAREADY_Pin
#define GPIO_DATA_READY_GPIO_Port	PC1_GPIO_Port

#endif /* M1_IO_DEFS_H_ */
