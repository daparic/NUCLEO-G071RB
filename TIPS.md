# Tips & Internals

## How `HAL_GPIO_EXTI_Falling_Callback` in `main.c` becomes operational

At first glance it looks like the function is never called — there is no explicit
call to it anywhere in the application code. It becomes operational through a
chain of five mechanisms that connect a physical button press all the way to
user code. Each step is described below.

---

### The full call chain

```
USER button pressed (PC13 falls low)
  │
  ▼
[1] EXTI hardware detects falling edge → asserts EXTI4_15_IRQn to the NVIC
  │
  ▼
[2] Cortex-M0+ reads vector table → jumps to EXTI4_15_IRQHandler
  │                                  (startup_stm32g0b1xx.s, line 155)
  ▼
[3] EXTI4_15_IRQHandler()            (stm32g0xx_it.c)
      calls HAL_GPIO_EXTI_IRQHandler(USER_BUTTON_PIN)
  │
  ▼
[4] HAL_GPIO_EXTI_IRQHandler()       (stm32g0xx_hal_gpio.c)
      clears the pending EXTI flag
      calls HAL_GPIO_EXTI_Falling_Callback(GPIO_Pin)
  │
  ▼
[5] HAL_GPIO_EXTI_Falling_Callback() (main.c)  ← our code runs here
      blink_index = (blink_index + 1) % BLINK_STEPS;
      button_pressed = 1;
```

---

### Step 1 — GPIO and EXTI configuration

Before any interrupt can fire, `Button_EXTI_Init()` in `main.c` must wire
PC13 to the EXTI subsystem:

```c
GPIO_InitStruct.Pin  = USER_BUTTON_PIN;   // GPIO_PIN_13
GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
GPIO_InitStruct.Pull = GPIO_NOPULL;
HAL_GPIO_Init(USER_BUTTON_PORT, &GPIO_InitStruct);

HAL_NVIC_SetPriority(EXTI4_15_IRQn, 2, 0);
HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
```

`GPIO_MODE_IT_FALLING` tells the HAL to configure both the GPIO alternate
function and the EXTI line so that a falling edge on PC13 triggers an interrupt
request. `HAL_NVIC_EnableIRQ` then unmasks that request in the Cortex-M0+
Nested Vectored Interrupt Controller (NVIC), allowing it to reach the CPU.

---

### Step 2 — The vector table (startup file)

When the EXTI4_15 interrupt fires, the Cortex-M0+ hardware automatically
suspends the current instruction stream and looks up the handler address in the
**vector table** — an array of function pointers placed at the very start of
FLASH (address `0x08000000`).

The startup file `startup_stm32g0b1xx.s` defines this table. The relevant
entry is:

```asm
.word EXTI4_15_IRQHandler    /* EXTI Line 4 to 15 — vector table position */
```

The same startup file also provides a default fallback:

```asm
.weak      EXTI4_15_IRQHandler
.thumb_set EXTI4_15_IRQHandler,Default_Handler
```

The `.weak` directive means: *use this symbol only if nobody else defines it*.
`Default_Handler` is an infinite loop — a safe catch-all for unhandled
interrupts. Because `stm32g0xx_it.c` defines `EXTI4_15_IRQHandler` without
`__weak`, the linker discards the startup file's weak fallback and uses our
definition instead.

> **Q: Are lines 226–227 defining a weak implementation of
> `EXTI4_15_IRQHandler` inside `Default_Handler` assembly?**
>
> No — these two lines create a **weak alias**, which is a different mechanism
> from embedding a weak implementation inside `Default_Handler`.
>
> ```asm
> .weak      EXTI4_15_IRQHandler
> .thumb_set EXTI4_15_IRQHandler,Default_Handler
> ```
>
> - **`.weak`** marks `EXTI4_15_IRQHandler` as a weak symbol — the linker
>   discards it if a strong definition of the same name exists anywhere else.
> - **`.thumb_set`** makes `EXTI4_15_IRQHandler` point at the **same address**
>   as `Default_Handler`. It does not copy or embed the `Default_Handler` code;
>   it just says *"these two names resolve to the same address"*.
>
> `Default_Handler` itself is defined once, separately, earlier in the startup
> file:
>
> ```asm
> Default_Handler:
>   b  Default_Handler      @ infinite loop
> ```
>
> So if nothing defines a strong `EXTI4_15_IRQHandler`, the vector table slot
> for EXTI4_15 points to that one shared infinite loop — used by every
> unhandled interrupt. There is only **one copy** of the loop in the binary
> regardless of how many unhandled interrupts there are.
>
> | What the code does | What it does NOT do |
> |--------------------|---------------------|
> | Creates a weak alias `EXTI4_15_IRQHandler` → `Default_Handler` | Does not create a separate weak copy of `Default_Handler` for EXTI4_15 |
> | Is overridden when a strong `EXTI4_15_IRQHandler` exists (ours in `stm32g0xx_it.c`) | Does not get merged or mixed with our strong definition |

---

### Step 3 — `EXTI4_15_IRQHandler` (`stm32g0xx_it.c`)

This is the actual ISR registered in the vector table. Its only job is to
forward the call into the HAL:

```c
void EXTI4_15_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(USER_BUTTON_PIN);
}
```

It passes `USER_BUTTON_PIN` so the HAL knows which specific pin within the
EXTI4–15 group triggered the interrupt (pins 4 through 15 all share one IRQ
line on the STM32G0).

> **Q: Does "pins 4 through 15 share one IRQ line" refer to the 64 physical
> pins of the chip package?**
>
> No — it refers to **EXTI lines**, not package pins. The STM32G0 has 16 EXTI
> lines (EXTI0–EXTI15), one per GPIO pin *number* regardless of port. They are
> grouped into three IRQ entries in the NVIC:
>
> | IRQ | EXTI lines | GPIO pins that map to it |
> |-----|-----------|--------------------------|
> | `EXTI0_1_IRQn`  | EXTI0, EXTI1          | any port, pin number 0 or 1  |
> | `EXTI2_3_IRQn`  | EXTI2, EXTI3          | any port, pin number 2 or 3  |
> | `EXTI4_15_IRQn` | EXTI4 through EXTI15  | any port, pin number 4 to 15 |
>
> PC13 has pin number **13**, so it maps to EXTI13, which falls in the EXTI4–15
> group → `EXTI4_15_IRQn`. This same IRQ would fire for PA4, PB7, PC9, or any
> other GPIO pin whose number is between 4 and 15, regardless of which port
> (A, B, C…) it belongs to. That is why `HAL_GPIO_EXTI_IRQHandler` receives
> `USER_BUTTON_PIN` as an argument — so it can inspect the EXTI pending register
> to determine *which* of the 12 possible lines actually fired before invoking
> the callback.

> **Q: What would happen if a different parameter (not `USER_BUTTON_PIN`) were
> passed to `HAL_GPIO_EXTI_IRQHandler()` on line 44?**
>
> Three things go wrong in sequence.
>
> The `GPIO_Pin` parameter is used as a **bitmask** directly against the EXTI
> hardware pending registers:
>
> ```c
> // stm32g0xx_hal_gpio.h
> #define __HAL_GPIO_EXTI_GET_FALLING_IT(__EXTI_LINE__)   (EXTI->FPR1 & (__EXTI_LINE__))
> #define __HAL_GPIO_EXTI_CLEAR_FALLING_IT(__EXTI_LINE__) (EXTI->FPR1 = (__EXTI_LINE__))
> ```
>
> `GPIO_PIN_13 = (1 << 13) = 0x2000`. If a wrong pin such as `GPIO_PIN_4
> = 0x0010` is passed instead:
>
> **1. The pending flag is never cleared.**
> `EXTI->FPR1 & 0x0010` evaluates to 0 because pin 4 never fired. The `if`
> block is never entered, so `EXTI->FPR1` bit 13 remains set.
>
> **2. The callback is never called.**
> Because the wrong pending bit was checked, `HAL_GPIO_EXTI_Falling_Callback`
> is never invoked. `blink_index` never advances.
>
> **3. The system locks up in an interrupt storm.**
> The NVIC keeps the interrupt asserted for as long as the EXTI pending flag
> is set. The instant `EXTI4_15_IRQHandler` returns, the CPU sees the flag
> still set and immediately re-enters the ISR. The main loop never gets CPU
> time again — the board appears completely frozen.
>
> ```
> button pressed
>   → EXTI4_15_IRQHandler entered
>     → HAL_GPIO_EXTI_IRQHandler(WRONG_PIN)
>       → checks wrong bit → 0 → no clear, no callback
>   → ISR returns
>   → NVIC: flag still set → re-enter ISR immediately
>   → ISR returns
>   → re-enter ISR ...  ← stuck here forever
> ```
>
> The parameter is not just an identifier forwarded to the callback — it is the
> bitmask used to both **check** and **clear** the hardware pending flag. It
> must match the pin that actually triggered the interrupt.

---

### Step 4 — `HAL_GPIO_EXTI_IRQHandler` (`stm32g0xx_hal_gpio.c`)

This HAL function does two things: housekeeping and delegation.

```c
void HAL_GPIO_EXTI_IRQHandler(uint16_t GPIO_Pin)
{
    if (__HAL_GPIO_EXTI_GET_RISING_IT(GPIO_Pin) != 0x00u)
    {
        __HAL_GPIO_EXTI_CLEAR_RISING_IT(GPIO_Pin);
        HAL_GPIO_EXTI_Rising_Callback(GPIO_Pin);
    }

    if (__HAL_GPIO_EXTI_GET_FALLING_IT(GPIO_Pin) != 0x00u)
    {
        __HAL_GPIO_EXTI_CLEAR_FALLING_IT(GPIO_Pin);
        HAL_GPIO_EXTI_Falling_Callback(GPIO_Pin);   // ← calls our function
    }
}
```

- **Housekeeping**: it reads the EXTI pending register to determine the edge
  direction and clears the flag. If the flag were not cleared, the interrupt
  would immediately re-fire the moment the ISR returned.
- **Delegation**: it calls the appropriate edge callback — in our case
  `HAL_GPIO_EXTI_Falling_Callback` because the button pull-down produces a
  falling edge.

---

### Step 5 — The weak symbol override (`main.c`)

The HAL defines both callbacks with the `__weak` attribute:

```c
// stm32g0xx_hal_gpio.c — HAL default (does nothing)
__weak void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
    UNUSED(GPIO_Pin);
}
```

`__weak` is a GCC linker directive meaning: *this definition can be replaced
by any other translation unit that provides a strong (non-weak) definition of
the same symbol*.

Our `main.c` provides exactly that:

```c
// main.c — strong definition, replaces the weak one
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == USER_BUTTON_PIN)
    {
        blink_index = (blink_index + 1) % BLINK_STEPS;
        button_pressed = 1;
    }
}
```

At link time, the linker sees two definitions of `HAL_GPIO_EXTI_Falling_Callback`:
one weak (from the HAL object file) and one strong (from `main.c`). It always
prefers the strong definition and silently discards the weak one. The HAL's
empty stub effectively becomes a no-op placeholder that exists solely to
prevent a link error in projects that never press a button.

---

### Summary

| Mechanism | File | Role |
|-----------|------|------|
| GPIO + EXTI + NVIC init | `main.c` — `Button_EXTI_Init()` | Connects PC13 falling edge to `EXTI4_15_IRQn` and unmasks it in the NVIC |
| Vector table entry | `startup_stm32g0b1xx.s` | Maps `EXTI4_15_IRQn` → `EXTI4_15_IRQHandler` function pointer at boot |
| Weak default handler | `startup_stm32g0b1xx.s` | Safe infinite-loop fallback, overridden by our strong definition |
| Peripheral ISR | `stm32g0xx_it.c` | Strong `EXTI4_15_IRQHandler` — overrides startup weak default; forwards to HAL |
| HAL dispatcher | `stm32g0xx_hal_gpio.c` | Clears pending flag, detects edge direction, calls appropriate weak callback |
| Weak callback override | `main.c` | Strong `HAL_GPIO_EXTI_Falling_Callback` — overrides HAL weak stub; runs user logic |

The pattern of **weak stub in library + strong override in user code** is used
throughout the STM32 HAL. It allows every peripheral callback to have a safe
do-nothing default while letting the application selectively override only the
ones it cares about, without requiring any registration or function-pointer
tables.

---

## Why `USART2` in `main.c` is required for `/dev/ttyACM0`

> **Q: Must `huart2.Instance` always be `USART2` on a NUCLEO-G071RB?
> Will `USART1` also work?**

At the **firmware level**, `USART1` is a valid peripheral — changing
`huart2.Instance = USART1` compiles and the peripheral functions correctly.
The HAL is instance-agnostic; it operates on whichever register block you point
it at.

At the **board/hardware level**, `USART2` is the only instance whose TX/RX pins
are physically connected to the ST-LINK chip. The NUCLEO-G071RB PCB has fixed
copper traces:

```
MCU                    ST-LINK chip           Host
PA2 (USART2_TX) ────── STLK_RX  ──────────── /dev/ttyACM0
PA3 (USART2_RX) ────── STLK_TX
```

These traces are soldered — they cannot be rerouted in firmware. If you point
the firmware at `USART1` and route its output to `PA9` (USART1_TX), the signal
appears on header pin **CN10** instead. Nothing shows on `/dev/ttyACM0`. An
external USB-to-UART adapter connected to `PA9` would be required to see it.

| Layer | Constraint | Reason |
|-------|-----------|--------|
| Firmware | None — any USART instance compiles | HAL is peripheral-agnostic |
| PCB / hardware | Must be `USART2` with PA2/PA3 | ST-LINK VCP traces are hardwired to those pins |

Note that the variable name `huart2` is pure convention (Hungarian notation from
STM32CubeIDE). The functionally critical line is `huart2.Instance = USART2` —
that selects the actual peripheral register block. If you switched to `USART1`,
the GPIO alternate function assignments would also need updating to match
USART1's AF mapping (`PA9`/`PA10` with `GPIO_AF1_USART1`, or `PB6`/`PB7`).

---

## How many USART peripherals does the STM32G071RB have?

> **Q: How many USARTs does the NUCLEO-G071RB have? Is it 3 (USART1, USART2,
> USART3) or more?**

The STM32G071RB has **4 full USARTs and 1 Low-Power UART**, confirmed directly
from the CMSIS device header
`Drivers/CMSIS/Device/ST/STM32G0xx/Include/stm32g071xx.h` in the STM32Cube FW
pack:

| Peripheral | Register base | IRQ |
|-----------|--------------|-----|
| USART1 | APB2 + 0x13800 | `USART1_IRQn` (dedicated) |
| USART2 | APB1 + 0x04400 | `USART2_IRQn` (dedicated) |
| USART3 | APB1 + 0x04800 | `USART3_4_LPUART1_IRQn` (shared) |
| USART4 | APB1 + 0x04C00 | `USART3_4_LPUART1_IRQn` (shared) |
| LPUART1 | APB1 + 0x08000 | `USART3_4_LPUART1_IRQn` (shared) |

Three independent pieces of evidence confirm USART4 exists:

1. **IRQ table** — the interrupt vector is explicitly named
   `USART3_4_LPUART1_IRQn` and its comment reads
   *"USART3, USART4 and LPUART1 global Interrupts"*.

2. **RCC reset register** — a dedicated reset bit `USART4RST` exists in
   `RCC_APBRSTR1` alongside `USART2RST`, `USART3RST`, and `LPUART1RST`.

3. **RCC clock-enable register** — `USART4EN` has its own enable bit in
   `RCC_APBENR1`.

The common mistake of counting only 3 USARTs likely comes from the fact that
USART3, USART4, and LPUART1 **share a single IRQ line** — they are easy to
overlook when only reading the NVIC interrupt table at a glance.
