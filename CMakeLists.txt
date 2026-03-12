cmake_minimum_required(VERSION 3.22)

project(blink_button C ASM)

# ── Paths to the STM32Cube G0 firmware pack ──────────────────────────────────
set(CUBE_FW  /home/dx/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.2)
set(HAL_SRC  ${CUBE_FW}/Drivers/STM32G0xx_HAL_Driver/Src)
set(HAL_INC  ${CUBE_FW}/Drivers/STM32G0xx_HAL_Driver/Inc)
set(CMSIS_DEVICE_INC ${CUBE_FW}/Drivers/CMSIS/Device/ST/STM32G0xx/Include)
set(CMSIS_CORE_INC   ${CUBE_FW}/Drivers/CMSIS/Include)
set(STARTUP  ${CUBE_FW}/Drivers/CMSIS/Device/ST/STM32G0xx/Source/Templates/gcc/startup_stm32g071xx.s)
set(SYSTEM_C ${CUBE_FW}/Drivers/CMSIS/Device/ST/STM32G0xx/Source/Templates/system_stm32g0xx.c)

# ── MCU flags ────────────────────────────────────────────────────────────────
# Separate list so each token is passed as a distinct compiler/linker argument.
set(CPU_FLAGS -mcpu=cortex-m0plus -mthumb)

set(CMAKE_C_FLAGS   "-mcpu=cortex-m0plus -mthumb -Wall -fdata-sections -ffunction-sections" CACHE STRING "" FORCE)
set(CMAKE_ASM_FLAGS "-mcpu=cortex-m0plus -mthumb -x assembler-with-cpp"                     CACHE STRING "" FORCE)

# ── Sources ──────────────────────────────────────────────────────────────────
set(APP_SOURCES
    Src/main.c
    Src/stm32g0xx_it.c
    Src/retarget.c
    ${SYSTEM_C}
    ${STARTUP}

    # HAL modules used by this project
    ${HAL_SRC}/stm32g0xx_hal.c
    ${HAL_SRC}/stm32g0xx_hal_rcc.c
    ${HAL_SRC}/stm32g0xx_hal_rcc_ex.c
    ${HAL_SRC}/stm32g0xx_hal_gpio.c
    ${HAL_SRC}/stm32g0xx_hal_exti.c
    ${HAL_SRC}/stm32g0xx_hal_flash.c
    ${HAL_SRC}/stm32g0xx_hal_flash_ex.c
    ${HAL_SRC}/stm32g0xx_hal_pwr.c
    ${HAL_SRC}/stm32g0xx_hal_pwr_ex.c
    ${HAL_SRC}/stm32g0xx_hal_cortex.c
    ${HAL_SRC}/stm32g0xx_hal_dma.c
    ${HAL_SRC}/stm32g0xx_hal_uart.c
)

# ── Executable ───────────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME}.elf ${APP_SOURCES})

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    Inc
    ${HAL_INC}
    ${HAL_INC}/Legacy
    ${CMSIS_DEVICE_INC}
    ${CMSIS_CORE_INC}
)

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    STM32G071xx
    USE_HAL_DRIVER
)

# ── Linker ───────────────────────────────────────────────────────────────────
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/STM32G071RBTX_FLASH.ld)

target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${CPU_FLAGS}
    -T${LINKER_SCRIPT}
    -specs=nano.specs
    -lc -lm
    -Wl,-Map=${PROJECT_NAME}.map,--cref
    -Wl,--gc-sections
    -Wl,--print-memory-usage
)

set_target_properties(${PROJECT_NAME}.elf PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})

# ── Post-build: generate .bin and .hex ───────────────────────────────────────
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND /opt/st/stm32cubeclt_1.16.0/GNU-tools-for-STM32/bin/arm-none-eabi-objcopy
            -O ihex   $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND /opt/st/stm32cubeclt_1.16.0/GNU-tools-for-STM32/bin/arm-none-eabi-objcopy
            -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMAND /opt/st/stm32cubeclt_1.16.0/GNU-tools-for-STM32/bin/arm-none-eabi-size
            $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating HEX, BIN, and size report"
)
