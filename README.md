# NUCLEO-G071RB — Blink + Button Demo

A bare-metal C project for the STM32G071RB on the NUCLEO-G071RB development board.
**LED4 blinks continuously. Each press of the USER button increases the blink rate,
cycling through five speeds before wrapping back to the slowest.**

---

## Table of Contents

1. [Hardware](#hardware)
2. [Project Structure](#project-structure)
3. [Prerequisites](#prerequisites)
4. [How to Build](#how-to-build)
5. [How to Flash](#how-to-flash)
6. [Application Behaviour](#application-behaviour)
7. [Memory Usage Explained](#memory-usage-explained)

---

## Hardware

| Signal      | MCU pin | NUCLEO label |
|-------------|---------|--------------|
| LED4 (green)| PA5     | LD4          |
| USER button | PC13    | B1           |

The USER button is active-low: the pin is pulled to GND when pressed.
The firmware configures it as a **falling-edge EXTI interrupt** on `EXTI4_15_IRQn`.

---

## Project Structure

```
NUCLEO-G071RB/
├── CMakeLists.txt              # Top-level CMake build
├── STM32G071RBTX_FLASH.ld      # Linker script (128 KB FLASH / 36 KB RAM)
├── cmake/
│   └── arm-none-eabi.cmake     # Cross-compilation toolchain file
├── Inc/
│   ├── main.h                  # LED & button pin definitions
│   ├── stm32g0xx_hal_conf.h    # HAL module selection
│   └── stm32g0xx_it.h          # ISR prototypes
└── Src/
    ├── main.c                  # Application logic
    └── stm32g0xx_it.c          # Exception & peripheral ISRs
```

HAL and CMSIS sources are **not copied into the project**; they are referenced
directly from the STM32Cube G0 firmware pack (see [Prerequisites](#prerequisites)).

---

## Prerequisites

### Toolchain — STM32CubeCLT 1.16.0

Installed at `/opt/st/stm32cubeclt_1.16.0/`. The following tools are used:

| Tool | Path |
|------|------|
| ARM GCC 12.3.1 | `GNU-tools-for-STM32/bin/arm-none-eabi-gcc` |
| CMake 3.x | `CMake/bin/cmake` |
| Ninja | `Ninja/bin/ninja` |
| STM32_Programmer_CLI | `STM32CubeProgrammer/bin/STM32_Programmer_CLI` |

### STM32Cube G0 Firmware Pack v1.6.2

Located at:

```
/home/dx/STM32Cube/Repository/STM32Cube_FW_G0_V1.6.2/
```

The build references the following subdirectories from this pack:

| Contents | Path (relative to pack root) |
|----------|------------------------------|
| HAL driver sources | `Drivers/STM32G0xx_HAL_Driver/Src/` |
| HAL driver headers | `Drivers/STM32G0xx_HAL_Driver/Inc/` |
| CMSIS device headers | `Drivers/CMSIS/Device/ST/STM32G0xx/Include/` |
| CMSIS core headers | `Drivers/CMSIS/Include/` |
| Startup file | `Drivers/CMSIS/Device/ST/STM32G0xx/Source/Templates/gcc/startup_stm32g071xx.s` |
| System init | `Drivers/CMSIS/Device/ST/STM32G0xx/Source/Templates/system_stm32g0xx.c` |

---

## How to Build

All commands are run from the project root (`NUCLEO-G071RB/`).

### 1. Configure

```bash
/opt/st/stm32cubeclt_1.16.0/CMake/bin/cmake \
    -B build \
    -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
    -DCMAKE_BUILD_TYPE=Debug \
    -G Ninja \
    -DCMAKE_MAKE_PROGRAM=/opt/st/stm32cubeclt_1.16.0/Ninja/bin/ninja
```

`-DCMAKE_TOOLCHAIN_FILE` points CMake at the cross-compilation settings so it
uses `arm-none-eabi-gcc` instead of the host compiler.

### 2. Compile & link

```bash
/opt/st/stm32cubeclt_1.16.0/Ninja/bin/ninja -C build
```

A successful build produces three artefacts inside `build/`:

| File | Description |
|------|-------------|
| `blink_button.elf` | ELF with debug info — used for flashing and debugging |
| `blink_button.hex` | Intel HEX — alternative flash format |
| `blink_button.bin` | Raw binary — alternative flash format |
| `blink_button.map` | Linker map — symbol and section placement details |

### Compiler & linker flags

| Flag | Purpose |
|------|---------|
| `-mcpu=cortex-m0plus -mthumb` | Target the Cortex-M0+ core in Thumb mode |
| `-ffunction-sections -fdata-sections` | Place each function/variable in its own section so the linker can discard unused ones |
| `-Wl,--gc-sections` | Garbage-collect unreferenced sections (dead-code elimination) |
| `-specs=nano.specs` | Link against newlib-nano, a size-optimised C library for embedded targets |
| `-lnosys` | Provide stub syscall implementations (no OS, no semihosting) |
| `-T STM32G071RBTX_FLASH.ld` | Use the project linker script |

---

## How to Flash

The NUCLEO board exposes an on-board ST-LINK debugger. Flashing is done over
the **SWD** (Serial Wire Debug) interface — no separate programmer is needed.

```bash
/opt/st/STM32CubeProgrammer-2.17.0/bin/STM32_Programmer_CLI \
    --connect port=SWD freq=4000 reset=HWrst \
    --download build/blink_button.elf \
    --verify \
    --go
```

| Option | Meaning |
|--------|---------|
| `port=SWD` | Connect via the ST-LINK SWD interface |
| `freq=4000` | SWD clock frequency in kHz (4 MHz) |
| `reset=HWrst` | Apply a hardware reset to the MCU before connecting |
| `--download` | Program the specified ELF into FLASH |
| `--verify` | Read back FLASH after programming and compare against the ELF |
| `--go` | Release reset and start the application immediately after flashing |

### Re-flashing after a rebuild

Just run the `--download` command again; there is no need to re-run CMake or
Ninja unless source files change.

---

## Application Behaviour

The main loop reads the current half-period from a table, drives the LED high
for that duration, then low for the same duration:

```c
while (1) {
    uint32_t half = blink_periods[blink_index];
    HAL_GPIO_WritePin(LED4_GPIO_PORT, LED4_PIN, GPIO_PIN_SET);
    HAL_Delay(half);   // LED on
    HAL_GPIO_WritePin(LED4_GPIO_PORT, LED4_PIN, GPIO_PIN_RESET);
    HAL_Delay(half);   // LED off
}
```

`HAL_Delay()` counts milliseconds using the SysTick timer (1 ms tick, configured
by `HAL_Init()`).

Each press of the USER button fires the `EXTI4_15_IRQHandler` → `HAL_GPIO_EXTI_IRQHandler`
→ `HAL_GPIO_EXTI_Falling_Callback` chain, which advances the index:

```c
blink_index = (blink_index + 1) % BLINK_STEPS;
```

The speed table cycles as follows:

| Button presses | Half-period | Full blink cycle |
|---------------|-------------|-----------------|
| 0 (power-on)  | 1000 ms     | 2.0 s           |
| 1             | 500 ms      | 1.0 s           |
| 2             | 250 ms      | 0.5 s           |
| 3             | 125 ms      | 0.25 s          |
| 4             | 62 ms       | ~125 ms         |
| 5             | wraps → 1000 ms | 2.0 s       |

---

## Memory Usage Explained

After a successful build, the linker prints:

```
Memory region    Used Size   Region Size   %age Used
           RAM:      1584 B       36 KB       4.30%
         FLASH:      5672 B      128 KB       4.33%
```

These figures come from the `MEMORY` block in the linker script, which defines
the two physical memory regions of the STM32G071RB:

```ld
MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 36K
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 128K
}
```

### How each number is derived

Running `arm-none-eabi-size --format=sysv blink_button.elf` shows every
ELF section with its size and destination address.
Note: the `addr` column is always printed in **decimal** by this tool:

```
section               size        addr
.isr_vector            188   134217728   ← FLASH (0x08000000)
.text                 5380   134217916   ← FLASH (0x080000bc)
.rodata                 84   134223296   ← FLASH (0x080015c0)
.init_array              4   134223372   ← FLASH (0x0800160c)
.fini_array              4   134223376   ← FLASH (0x08001610)
.data                   12   536870912   ← RAM   (0x20000000, loaded from FLASH at 0x0800161c)
.bss                    36   536870924   ← RAM   (0x2000000c)
._user_heap_stack     1536   536870960   ← RAM   (0x20000030)
```

To see addresses in hex directly, use `arm-none-eabi-objdump -h` instead:

```bash
arm-none-eabi-objdump -h blink_button.elf
```

#### FLASH used = 5672 B

All sections that are stored in FLASH — including the LMA (Load Memory Address)
copy of `.data` that the startup code copies into RAM at boot — are summed:

```
.isr_vector   188
.text        5380
.rodata        84
.init_array     4
.fini_array     4
.data          12   ← initialisation image lives in FLASH
               ──
             5672 B  →  5672 / (128 × 1024) = 4.33 %
```

#### RAM used = 1584 B

All sections mapped into the RAM region are summed:

```
.data           12   ← initialised globals/statics (copied from FLASH at boot)
.bss            36   ← zero-initialised globals/statics
._user_heap_stack 1536   ← heap (0x200 = 512 B) + stack (0x400 = 1024 B)
                ──
              1584 B  →  1584 / (36 × 1024) = 4.30 %
```

#### The `._user_heap_stack` section

This section is not real data; it is a **reservation** defined in the linker
script to guarantee that enough RAM remains free for the heap and stack at
link time:

```ld
._user_heap_stack :
{
  . = ALIGN(8);
  PROVIDE(end = .);
  . = . + _Min_Heap_Size;    /* 0x200 = 512 B */
  . = . + _Min_Stack_Size;   /* 0x400 = 1024 B */
  . = ALIGN(8);
} >RAM
```

If the total RAM consumption of `.data` + `.bss` + heap + stack would exceed
36 KB, the linker raises a region overflow error at build time — a useful
static check that prevents stack/heap collisions from being discovered only at
runtime.

#### Cross-check with `arm-none-eabi-size` Berkeley format

The standard (Berkeley) output groups sections differently:

```
text    data    bss     dec     hex
5652      20   1572    7244    1c4c
```

| Column | Sections included | Total |
|--------|------------------|-------|
| `text` | `.isr_vector` + `.text` + `.rodata` | 188+5380+84 = **5652** |
| `data` | `.init_array` + `.fini_array` + `.data` | 4+4+12 = **20** |
| `bss`  | `.bss` + `._user_heap_stack` | 36+1536 = **1572** |

- **FLASH** = `text + data` = 5652 + 20 = **5672 B** ✓
- **RAM** = `data + bss` = 20 + 1572 = 1592 B — this overcounts by 8 B
  because the Berkeley format incorrectly places `.init_array` and
  `.fini_array` (which reside in FLASH) into the `data` column.
  The linker's own `--print-memory-usage` report (1584 B) is authoritative.
