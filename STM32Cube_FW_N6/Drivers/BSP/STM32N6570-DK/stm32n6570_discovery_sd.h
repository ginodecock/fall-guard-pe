/**
  ******************************************************************************
  * @file    stm32n6570_discovery_sd.h
  * @author  MCD Application Team
  * @brief   This file contains the common defines and functions prototypes for
  *          the stm32n6570_discovery_sd.c driver.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef STM32N6570_DK_SD_H
#define STM32N6570_DK_SD_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32n6570_discovery_conf.h"
#include "stm32n6570_discovery_errno.h"

/** @addtogroup BSP
  * @{
  */

/** @addtogroup STM32N6570_DK
  * @{
  */

/** @defgroup STM32N6570_DK_SD SD
  * @{
  */

/** @defgroup STM32N6570_DK_SD_Exported_Types SD Exported Types
  * @{
  */

/**
  * @brief SD Card information structure
  */
#define BSP_SD_CardInfo HAL_SD_CardInfoTypeDef

#if (USE_HAL_SD_REGISTER_CALLBACKS > 0)
typedef struct
{
  void (*pMspInitCb)(SD_HandleTypeDef *);
  void (*pMspDeInitCb)(SD_HandleTypeDef *);
} BSP_SD_Cb_t;
#endif /* (USE_HAL_SD_REGISTER_CALLBACKS > 0) */
/**
  * @}
  */

/** @defgroup STM32N6570_DK_SD_Exported_Constants SD Exported Constants
  * @{
  */
#define SD_INSTANCES_NBR         1UL

#ifndef SD_WRITE_TIMEOUT
#define SD_WRITE_TIMEOUT         100U
#endif /* SD_WRITE_TIMEOUT */

#ifndef SD_READ_TIMEOUT
#define SD_READ_TIMEOUT          100U
#endif /* SD_READ_TIMEOUT */

/**
  * @brief  SD transfer state definition
  */
#define   SD_TRANSFER_OK         0U
#define   SD_TRANSFER_BUSY       1U

/**
  * @brief SD-detect signal
  */
#define SD_PRESENT               1U
#define SD_NOT_PRESENT           0U

#define SD_DETECT_PIN                        GPIO_PIN_12
#define SD_DETECT_GPIO_PORT                  GPION
#define SD_DETECT_GPIO_CLK_ENABLE()          __HAL_RCC_GPION_CLK_ENABLE()
#define SD_DETECT_GPIO_CLK_DISABLE()         __HAL_RCC_GPION_CLK_DISABLE()
#define SD_DETECT_EXTI_IRQn                  EXTI12_IRQn
#define SD_DETECT_EXTI_LINE                  EXTI_LINE_12
/**
  * @}
  */

/** @addtogroup STM32N6570_DK_SD_Exported_Variables
  * @{
  */
extern SD_HandleTypeDef   hsd_sdmmc[];
extern EXTI_HandleTypeDef hsd_exti[];

/**
  * @}
  */

/** @addtogroup STM32N6570_DK_SD_Exported_Functions
  * @{
  */
int32_t BSP_SD_Init(uint32_t Instance);
int32_t BSP_SD_DeInit(uint32_t Instance);
#if (USE_HAL_SD_REGISTER_CALLBACKS > 0)
int32_t BSP_SD_RegisterMspCallbacks(uint32_t Instance, BSP_SD_Cb_t *CallBacks);
int32_t BSP_SD_RegisterDefaultMspCallbacks(uint32_t Instance);
#endif /* (USE_HAL_SD_REGISTER_CALLBACKS > 0) */
int32_t BSP_SD_DetectITConfig(uint32_t Instance);
int32_t BSP_SD_ReadBlocks(uint32_t Instance, uint32_t *pData, uint32_t BlockIdx, uint32_t NbrOfBlocks);
int32_t BSP_SD_WriteBlocks(uint32_t Instance, uint32_t *pData, uint32_t BlockIdx, uint32_t NbrOfBlocks);
int32_t BSP_SD_ReadBlocks_DMA(uint32_t Instance, uint32_t *pData, uint32_t BlockIdx, uint32_t NbrOfBlocks);
int32_t BSP_SD_WriteBlocks_DMA(uint32_t Instance, uint32_t *pData, uint32_t BlockIdx, uint32_t NbrOfBlocks);
int32_t BSP_SD_ReadBlocks_IT(uint32_t Instance, uint32_t *pData, uint32_t BlockIdx, uint32_t NbrOfBlocks);
int32_t BSP_SD_WriteBlocks_IT(uint32_t Instance, uint32_t *pData, uint32_t BlockIdx, uint32_t NbrOfBlocks);
int32_t BSP_SD_Erase(uint32_t Instance, uint32_t BlockIdx, uint32_t NbrOfBlocks);
int32_t BSP_SD_GetCardState(uint32_t Instance);
int32_t BSP_SD_GetCardInfo(uint32_t Instance, BSP_SD_CardInfo *CardInfo);
int32_t BSP_SD_IsDetected(uint32_t Instance);

void    BSP_SD_DETECT_IRQHandler(uint32_t Instance);
void    BSP_SD_IRQHandler(uint32_t Instance);

/* These functions can be modified in case the current settings (e.g. DMA stream or IT)
   need to be changed for specific application needs */
void BSP_SD_AbortCallback(uint32_t Instance);
void BSP_SD_WriteCpltCallback(uint32_t Instance);
void BSP_SD_ReadCpltCallback(uint32_t Instance);
void BSP_SD_DetectCallback(uint32_t Instance, uint32_t Status);

HAL_StatusTypeDef MX_SDMMC1_SD_Init(SD_HandleTypeDef *hsd);

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* STM32N6570_DK_SD_H */
