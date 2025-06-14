################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
Y:/VM/stm-workspace/fall-guard-pe/STM32Cube_FW_N6/Middlewares/ST/netxduo/common/drivers/ethernet/rtl8211/nx_stm32_phy_driver.c 

OBJS += \
./STM32Cube_FW_N6/Middlewares/NetXDuo/Network/Ethernet\ Phy\ Interface/RTL8211_Phy_Interface/nx_stm32_phy_driver.o 

C_DEPS += \
./STM32Cube_FW_N6/Middlewares/NetXDuo/Network/Ethernet\ Phy\ Interface/RTL8211_Phy_Interface/nx_stm32_phy_driver.d 


# Each subdirectory must supply rules for building sources it contributes
STM32Cube_FW_N6/Middlewares/NetXDuo/Network/Ethernet\ Phy\ Interface/RTL8211_Phy_Interface/nx_stm32_phy_driver.o: Y:/VM/stm-workspace/fall-guard-pe/STM32Cube_FW_N6/Middlewares/ST/netxduo/common/drivers/ethernet/rtl8211/nx_stm32_phy_driver.c STM32Cube_FW_N6/Middlewares/NetXDuo/Network/Ethernet\ Phy\ Interface/RTL8211_Phy_Interface/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m55 -std=gnu11 -g3 -DSTM32N657xx -DRTL8211_INIT_TO=10000 -DNX_INCLUDE_USER_DEFINE_FILE -DETH_PHY_1000MBITS_SUPPORTED -DNX_SECURE_INCLUDE_USER_DEFINE_FILE -DUSE_FULL_LL_DRIVER -DVECT_TAB_SRAM -DUSE_IMX335_SENSOR -DUSE_VD66GY_SENSOR -DUSE_VD55G1_SENSOR -DTX_MAX_PARALLEL_NETWORKS=1 -DTX_SINGLE_MODE_SECURE=1 -DLL_ATON_PLATFORM=LL_ATON_PLAT_STM32N6 -DLL_ATON_OSAL=LL_ATON_OSAL_THREADX -DLL_ATON_RT_MODE=LL_ATON_RT_ASYNC -DLL_ATON_SW_FALLBACK -DLL_ATON_DBG_BUFFER_INFO_EXCLUDED=1 -DTX_HAS_PARALLEL_NETWORKS=0 -DTX_SINGLE_MODE_SECURE -DFEAT_THREADX -DTX_INCLUDE_USER_DEFINE_FILE -DTX_EXECUTION_PROFILE_ENABLE -DUSE_HAL_RTC_REGISTER_CALLBACKS=0 -c -I../../Inc -I../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Inc -I../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Inc/Legacy -I../../STM32Cube_FW_N6/Drivers/CMSIS/Device/ST/STM32N6xx/Include -I../../STM32Cube_FW_N6/Drivers/CMSIS/Include -I../../STM32Cube_FW_N6/Drivers/CMSIS/DSP/Include -I../../STM32Cube_FW_N6/Drivers/BSP/Components/Common -I../../STM32Cube_FW_N6/Drivers/BSP/STM32N6570-DK -I../../STM32Cube_FW_N6/Utilities/lcd -I../../STM32Cube_FW_N6/Drivers/BSP/Components/aps256xx -I../../Lib/AI_Runtime/Inc -I../../Lib/AI_Runtime/Npu/ll_aton -I../../Lib/AI_Runtime/Npu/Devices/STM32N6XX -I../../Lib/lib_vision_models_pp/lib_vision_models_pp/Inc -I../../Lib/Camera_Middleware -I../../Lib/Camera_Middleware/sensors -I../../Lib/Camera_Middleware/sensors/imx335 -I../../Lib/Camera_Middleware/sensors/vd55g1 -I../../Lib/Camera_Middleware/sensors/vd6g -I../../Lib/Camera_Middleware/ISP_Library/isp/Inc -I../../Lib/Camera_Middleware/ISP_Library/evision/Inc -I../../STM32Cube_FW_N6/Middlewares/ST/threadx/common/inc -I../../STM32Cube_FW_N6/Middlewares/ST/threadx/ports/cortex_m55/gnu/inc -I../../STM32Cube_FW_N6/Middlewares/ST/threadx/utility/execution_profile_kit -I../../STM32Cube_FW_N6/Middlewares/ST/netxduo/common/drivers/ethernet -I../../STM32Cube_FW_N6/Middlewares/ST/netxduo/nx_secure/inc -I../../STM32Cube_FW_N6/Middlewares/ST/netxduo/nx_secure/ports -I../../STM32Cube_FW_N6/Middlewares/ST/netxduo/crypto_libraries/inc -I../../STM32Cube_FW_N6/Middlewares/ST/netxduo/tsn/inc -I../../STM32Cube_FW_N6/Middlewares/ST/netxduo/addons/mqtt -I../../STM32Cube_FW_N6/Middlewares/ST/netxduo/addons/dhcp -I../../STM32Cube_FW_N6/Middlewares/ST/netxduo/addons/sntp -I../../STM32Cube_FW_N6/Middlewares/ST/netxduo/addons/dns -I../../STM32Cube_FW_N6/Middlewares/ST/netxduo/common/inc -I../../STM32Cube_FW_N6/Middlewares/ST/netxduo/ports/cortex_m55/gnu/inc -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -mcmse -MMD -MP -MF"STM32Cube_FW_N6/Middlewares/NetXDuo/Network/Ethernet Phy Interface/RTL8211_Phy_Interface/nx_stm32_phy_driver.d" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-STM32Cube_FW_N6-2f-Middlewares-2f-NetXDuo-2f-Network-2f-Ethernet-20-Phy-20-Interface-2f-RTL8211_Phy_Interface

clean-STM32Cube_FW_N6-2f-Middlewares-2f-NetXDuo-2f-Network-2f-Ethernet-20-Phy-20-Interface-2f-RTL8211_Phy_Interface:
	-$(RM) ./STM32Cube_FW_N6/Middlewares/NetXDuo/Network/Ethernet\ Phy\ Interface/RTL8211_Phy_Interface/nx_stm32_phy_driver.cyclo ./STM32Cube_FW_N6/Middlewares/NetXDuo/Network/Ethernet\ Phy\ Interface/RTL8211_Phy_Interface/nx_stm32_phy_driver.d ./STM32Cube_FW_N6/Middlewares/NetXDuo/Network/Ethernet\ Phy\ Interface/RTL8211_Phy_Interface/nx_stm32_phy_driver.o ./STM32Cube_FW_N6/Middlewares/NetXDuo/Network/Ethernet\ Phy\ Interface/RTL8211_Phy_Interface/nx_stm32_phy_driver.su

.PHONY: clean-STM32Cube_FW_N6-2f-Middlewares-2f-NetXDuo-2f-Network-2f-Ethernet-20-Phy-20-Interface-2f-RTL8211_Phy_Interface

