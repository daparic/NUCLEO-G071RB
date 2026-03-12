/**
 ******************************************************************************
 * @file    retarget.c
 * @brief   Redirect printf (newlib stdout/stderr) to USART2, and provide
 *          the minimal syscall stubs required by newlib-nano.
 *
 * newlib calls _write() for every fwrite/printf output.  By implementing it
 * here we route all stdout and stderr traffic to the ST-LINK Virtual COM Port
 * (USART2 TX = PA2) which appears as /dev/ttyACM0 on the host.
 ******************************************************************************
 */

#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include "main.h"

/* Provided by the linker script: marks the first free byte after .bss */
extern uint8_t _end;

/* ── stdout/stderr → USART2 ─────────────────────────────────────────────── */

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

/* ── Minimal stubs — not used by this application ───────────────────────── */

int _read(int file, char *data, int len)
{
    (void)file; (void)data; (void)len;
    errno = EBADF;
    return -1;
}

int _close(int file)
{
    (void)file;
    errno = EBADF;
    return -1;
}

int _lseek(int file, int offset, int whence)
{
    (void)file; (void)offset; (void)whence;
    errno = EBADF;
    return -1;
}

int _fstat(int file, struct stat *st)
{
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

/* ── Heap: grow upward from end of .bss ─────────────────────────────────── */

void *_sbrk(ptrdiff_t incr)
{
    static uint8_t *heap_end = NULL;
    uint8_t *prev;

    if (heap_end == NULL)
    {
        heap_end = &_end;
    }
    prev = heap_end;
    heap_end += incr;
    return (void *)prev;
}
