#include <efi.h>
#include <efilib.h>

#include "efi/timer.h"

#define CALIBRATION_US 50000
#define CALIBRATION_MS 50.0

/* Timer events count in 100 ns units. */
#define CALIBRATION_100NS (CALIBRATION_US * 10)

#define MIN_TICKS_PER_MS 100000.0
#define MAX_TICKS_PER_MS 10000000.0

static UINT64 start_ticks;
static double ticks_per_ms;

static inline UINT64 read_tsc(void)
{
    UINT32 low;
    UINT32 high;

    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((UINT64)high << 32) | low;
}

static inline void cpuid(UINT32 leaf, UINT32 *a, UINT32 *b, UINT32 *c, UINT32 *d)
{
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(leaf), "c"(0));
}

static int plausible(double value)
{
    return value >= MIN_TICKS_PER_MS && value <= MAX_TICKS_PER_MS;
}

/* Intel: the CPU reports its TSC frequency as crystal clock x ratio (leaf 0x15). */
static double ticks_from_cpuid(void)
{
    UINT32 a;
    UINT32 b;
    UINT32 c;
    UINT32 d;

    cpuid(0, &a, &b, &c, &d);
    if (a < 0x15) {
        return 0.0;
    }

    cpuid(0x15, &a, &b, &c, &d);
    if (a == 0 || b == 0 || c == 0) {
        return 0.0;
    }

    return (double)c * b / a / 1000.0;
}

/*
 * Count TSC ticks over one period of a periodic timer event. The first event
 * aligns the start to a timer tick, so the period is measured edge to edge.
 * Unlike Stall(), timer events run off the platform timer interrupt.
 */
static double ticks_from_timer_event(void)
{
    EFI_EVENT event;
    UINTN index;
    UINT64 before;
    UINT64 after;

    if (BS->CreateEvent(EVT_TIMER, 0, NULL, NULL, &event) != EFI_SUCCESS) {
        return 0.0;
    }

    if (BS->SetTimer(event, TimerPeriodic, CALIBRATION_100NS) != EFI_SUCCESS) {
        BS->CloseEvent(event);
        return 0.0;
    }

    BS->WaitForEvent(1, &event, &index);
    before = read_tsc();
    BS->WaitForEvent(1, &event, &index);
    after = read_tsc();

    BS->SetTimer(event, TimerCancel, 0);
    BS->CloseEvent(event);
    return (double)(after - before) / CALIBRATION_MS;
}

static double ticks_from_stall(void)
{
    UINT64 before = read_tsc();

    BS->Stall(CALIBRATION_US);
    return (double)(read_tsc() - before) / CALIBRATION_MS;
}

/*
 * Stall() is not trusted for calibration: on some firmware it waits many
 * times longer than asked, which slows every animation and the countdown.
 */
void timer_calibrate(void)
{
    ticks_per_ms = ticks_from_cpuid();

    if (!plausible(ticks_per_ms)) {
        ticks_per_ms = ticks_from_timer_event();
    }

    if (!plausible(ticks_per_ms)) {
        ticks_per_ms = ticks_from_stall();
    }

    start_ticks = read_tsc();
}

float timer_now_ms(void)
{
    return (float)((double)(read_tsc() - start_ticks) / ticks_per_ms);
}

void timer_sleep_ms(float milliseconds)
{
    UINT64 end;

    if (milliseconds <= 0.0f) {
        return;
    }

    end = read_tsc() + (UINT64)(milliseconds * ticks_per_ms);
    while (read_tsc() < end) {
        __asm__ volatile("pause");
    }
}
