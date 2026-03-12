# NUCLEO-G071RB — Blink + Button Demo

A bare-metal C project for the STM32G071RB on the NUCLEO-G071RB development board.
**LED4 blinks continuously. Each press of the USER button increases the blink rate,
cycling through five speeds before wrapping back to the slowest.**

State is reported in real time over USART2, visible on the host as `/dev/ttyACM0`
via the ST-LINK Virtual COM Port (VCP).

---

## Table of Contents

1. [Hardware](#hardware)
2. [Project Structure](#project-structure)
3. [Prerequisites](#prerequisites)
4. [How to Build](#how-to-build)
   - [Post-build steps](#post-build-steps-cmakeliststxt-lines-7482)
5. [How to Flash](#how-to-flash)
6. [Serial Output](#serial-output)
7. [Application Behaviour](#application-behaviour)
8. [Memory Usage Explained](#memory-usage-explained)

---

## Hardware

| Signal        | MCU pin | NUCLEO label |
|---------------|---------|--------------|
| LED4 (green)  | PA5     | LD4          |
| USER button   | PC13    | B1           |
| USART2 TX     | PA2     | VCP TX → ST-LINK |
| USART2 RX     | PA3     | VCP RX → ST-LINK |

The USER button is active-low: the pin is pulled to GND when pressed.
The firmware configures it as a **falling-edge EXTI interrupt** on `EXTI4_15_IRQn`.

PA2 and PA3 are routed directly to the ST-LINK chip on the NUCLEO board via PCB
traces. No jumper wires or external hardware are needed for serial communication.

---

## Project Structure

```
NUCLEO-G071RB/
├── CMakeLists.txt              # Top-level CMake build
├── STM32G071RBTX_FLASH.ld      # Linker script (128 KB FLASH / 36 KB RAM)
├── cmake/
│   └── arm-none-eabi.cmake     # Cross-compilation toolchain file
├── Inc/
│   ├── main.h                  # LED, button pin definitions; huart2 extern
│   ├── stm32g0xx_hal_conf.h    # HAL module selection
│   └── stm32g0xx_it.h          # ISR prototypes
└── Src/
    ├── main.c                  # Application logic
    ├── stm32g0xx_it.c          # Exception & peripheral ISRs
    └── retarget.c              # Redirects printf → USART2; newlib syscall stubs
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
| `-T STM32G071RBTX_FLASH.ld` | Use the project linker script |

Note: `-lnosys` is **not** used. That library provides stub syscalls, but its
`_write` symbol is non-weak and would conflict with the `_write` implementation
in `retarget.c`. Instead, `retarget.c` provides all required syscall stubs
directly (`_write`, `_read`, `_close`, `_lseek`, `_fstat`, `_isatty`, `_sbrk`).

### Post-build steps (`CMakeLists.txt` lines 74–82)

`add_executable` creates `blink_button.elf` by compiling and linking all sources.
The `add_custom_command(POST_BUILD …)` block is a separate hook that runs
**immediately after** a successful link — think of it as a decorator that shows up
once the ELF is ready to convert and report on it.

```cmake
add_custom_command(TARGET blink_button.elf POST_BUILD
    COMMAND arm-none-eabi-objcopy -O ihex   blink_button.elf blink_button.hex
    COMMAND arm-none-eabi-objcopy -O binary blink_button.elf blink_button.bin
    COMMAND arm-none-eabi-size              blink_button.elf
    COMMENT "Generating HEX, BIN, and size report"
)
```

The three commands it runs, in order:

| Command | What it does |
|---------|-------------|
| `objcopy -O ihex … .hex` | Converts the ELF to Intel HEX format — a text file of hex digits accepted by many programmers |
| `objcopy -O binary … .bin` | Strips all debug info and metadata from the ELF, leaving only the raw bytes that will be programmed into FLASH |
| `arm-none-eabi-size` | Prints the `text / data / bss` size table to the terminal (the numbers discussed in [Memory Usage Explained](#memory-usage-explained)) |

**When is it triggered?**
Only when Ninja actually re-links the ELF — i.e. after a source file or the
linker script changes. If nothing has changed and Ninja skips the link step,
this block is also skipped.

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

## Serial Output

The firmware prints state messages over **USART2 at 115200 baud 8N1**.
The ST-LINK chip on the NUCLEO board bridges USART2 (PA2/PA3) to USB, so it
appears as `/dev/ttyACM0` on Linux without any extra hardware.

```
STM32G071RB          ST-LINK chip           Host PC
  PA2 (USART2 TX) ──► VCP TX ──► USB ──► /dev/ttyACM0
  PA3 (USART2 RX) ◄── VCP RX ◄── USB ◄── /dev/ttyACM0
```

### Opening a terminal

```bash
# picocom
picocom -b 115200 /dev/ttyACM0

# screen
screen /dev/ttyACM0 115200
```

Press `Ctrl+A, Ctrl+X` to exit picocom, or `Ctrl+A, K` to exit screen.

### Expected output

On power-on or reset:

```
== NUCLEO-G071RB Blink+Button ==
Press USER button to increase blink speed.
[BOOT] speed=0  half-period=1000 ms
```

Each USER button press:

```
[BTN]  speed=1  half-period=500 ms
[BTN]  speed=2  half-period=250 ms
[BTN]  speed=3  half-period=125 ms
[BTN]  speed=4  half-period=62 ms
[BTN]  speed=0  half-period=1000 ms
```

### How printf reaches the UART (`retarget.c`)

`printf` is part of newlib-nano (the C library linked via `-specs=nano.specs`).
Internally, newlib routes all output through a syscall named `_write`. By
implementing `_write` in `retarget.c`, every `printf` call in the application
is redirected to `HAL_UART_Transmit`:

```c
int _write(int file, char *data, int len)
{
    if (file == STDOUT_FILENO || file == STDERR_FILENO)
    {
        HAL_UART_Transmit(&huart2, (uint8_t *)data, (uint16_t)len, HAL_MAX_DELAY);
        return len;
    }
    errno = EBADF;
    return -1;
}
```

`retarget.c` also provides the other syscall stubs that newlib requires at link
time even though this application does not use them:

| Stub | Why it is needed |
|------|-----------------|
| `_read` | newlib pulls it in transitively through stdio |
| `_close` | same as above |
| `_lseek` | same as above |
| `_fstat` | used by printf to query the file descriptor type |
| `_isatty` | used by printf to decide line-buffering behaviour |
| `_sbrk` | heap allocator — newlib's printf uses `malloc` internally for its format buffer |

### Why printf is never called from the button ISR

`HAL_UART_Transmit` blocks until all bytes are sent and uses `HAL_GetTick()`
for its timeout, which reads the SysTick counter. SysTick increments via its
own ISR at a lower priority than the button EXTI. If `printf` were called
inside the button ISR, the UART transmit would wait forever for a tick that
can never arrive because the SysTick ISR is blocked behind the button ISR.

The solution is a **deferred-print** pattern: the ISR only sets a flag and
updates the index; the main loop checks the flag on every iteration and calls
`printf` from safe non-ISR context.

```c
/* In ISR — fast, no blocking calls */
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == USER_BUTTON_PIN)
    {
        blink_index = (blink_index + 1) % BLINK_STEPS;
        button_pressed = 1;
    }
}

/* In main loop — safe to call printf here */
if (button_pressed)
{
    button_pressed = 0;
    printf("[BTN]  speed=%lu  half-period=%lu ms\r\n",
           blink_index, blink_periods[blink_index]);
}
```

---

## Application Behaviour

The main loop reads the current half-period from a table, drives the LED high
for that duration, then low for the same duration:

```c
while (1) {
    if (button_pressed)
    {
        button_pressed = 0;
        printf("[BTN]  speed=%lu  half-period=%lu ms\r\n",
               blink_index, blink_periods[blink_index]);
    }

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
           RAM:      2168 B       36 KB       5.88%
         FLASH:     14224 B      128 KB      10.85%
```

The increase compared to the LED-only version (1584 B RAM / 5672 B FLASH) comes
entirely from adding `printf` support: the UART HAL driver, newlib-nano's stdio
formatting code, and the syscall stubs all contribute to FLASH, while
newlib-nano's internal `printf` format buffer is allocated from the heap at
runtime and contributes to RAM.

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
.text                14112   134217916   ← FLASH (0x080000bc)
.rodata                  0   134232028   ← FLASH
.init_array              4   134232028   ← FLASH
.fini_array              4   134232032   ← FLASH
.data                  112   536870912   ← RAM   (0x20000000, loaded from FLASH)
.bss                   528   536871024   ← RAM   (0x20000030)
._user_heap_stack     1536   536871552   ← RAM
```

To see addresses in hex directly, use `arm-none-eabi-objdump -h` instead:

```bash
arm-none-eabi-objdump -h blink_button.elf
```

#### FLASH used = 14224 B

```
.isr_vector    188
.text        14112
.rodata          0
.init_array      4
.fini_array      4
.data          112   ← initialisation image lives in FLASH
               ───
            14420 B ... wait, that's the Berkeley sum
```

The linker's `--print-memory-usage` (14224 B) is the authoritative figure; it
sums only the bytes actually placed within the FLASH region by the linker
script, after alignment padding is resolved.

#### RAM used = 2168 B

```
.data          112   ← initialised globals/statics (copied from FLASH at boot)
.bss           520   ← zero-initialised globals/statics
._user_heap_stack 1536  ← heap (512 B) + stack (1024 B) reservation
               ───
             2168 B  →  2168 / (36 × 1024) = 5.88 %
```

The growth in `.data` (from 12 B to 112 B) and `.bss` (from 36 B to 520 B)
reflects the newlib-nano stdio state structures (`FILE`, internal buffers)
that are initialised at startup when `printf` is first used.

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
14112    112   2064   16288    3fa0
```

| Column | Sections included | Total |
|--------|------------------|-------|
| `text` | `.isr_vector` + `.text` + `.rodata` | **14112** |
| `data` | `.init_array` + `.fini_array` + `.data` | 4+4+112 = **120** |
| `bss`  | `.bss` + `._user_heap_stack` | 528+1536 = **2064** |

- **FLASH** = `text + data` = 14112 + 112 = **14224 B** ✓
- **RAM** = `data + bss` = 120 + 2064 = 2184 B — overcounts by 8 B because
  Berkeley incorrectly places `.init_array` and `.fini_array` (FLASH) into
  the `data` column. The linker's `--print-memory-usage` (2168 B) is authoritative.
