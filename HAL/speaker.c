#include "speaker.h"
#include "dac.h"
#include <rtthread.h>
#include <rthw.h>
#include "ch32v30x.h"
#include "ch32v30x_it.h"

#define HALF 64U
#define EVENTS (DMA_IT_HT | DMA_IT_TC | DMA_IT_TE)
static uint16_t buffer[2 * HALF];
static const int16_t *next_sample;
static size_t remaining;
static uint8_t last[2];
static struct rt_semaphore done;
static volatile int result;
static struct rt_mutex mutex;
static int speaker_ipc_init(void) { return rt_mutex_init(&mutex, "speaker", RT_IPC_FLAG_PRIO); }
INIT_COMPONENT_EXPORT(speaker_ipc_init);

/* DMA 刚读完的半区才可以重填。末尾补 DAC 中点，使播放自然停止。 */
static void fill(unsigned half)
{
    size_t n = remaining < HALF ? remaining : HALF;
    uint16_t *dst = buffer + half * HALF;
    for (size_t i = 0; i < n; ++i) dst[i] = ((int32_t)*next_sample++ + 32768) >> 4;
    for (size_t i = n; i < HALF; ++i) dst[i] = 2048;
    remaining -= n;
    last[half] = !remaining;
    __sync_synchronize();
}

static void pcm_interrupt(void)
{
    uint32_t flags = DMA2->INTFR & (DMA2_FLAG_HT3 | DMA2_FLAG_TC3 | DMA2_FLAG_TE3);
    /* 标准库常量的高位用于选择 DMA2，不属于硬件状态位。 */
    flags &= 0xEU << 8;
    if (!flags) return;
    DMA2->INTFCR = flags;
    unsigned half = (flags & DMA2_FLAG_HT3) ? 0U : 1U;
    if ((flags & DMA2_FLAG_TE3) || ((flags & DMA2_FLAG_HT3) && (flags & DMA2_FLAG_TC3))) {
        result = -RT_EIO;  /* 半区未及时重填或 DMA 总线错误。 */
    } else if (!last[half]) {
        fill(half);
        return;
    }
    TIM_Cmd(TIM6, DISABLE);
    DMA_ITConfig(DMA2_Channel3, EVENTS, DISABLE);
    rt_sem_release(&done);
}

void DMA2_Channel3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void DMA2_Channel3_IRQHandler(void)
{
    GET_INT_SP();
    rt_interrupt_enter();
    pcm_interrupt();
    rt_interrupt_leave();
    FREE_INT_SP();
}

static int tone(uint32_t hz, uint8_t volume)
{
    static const int16_t sine[16] = {
        0, 783, 1447, 1891, 2047, 1891, 1447, 783,
        0, -783, -1447, -1891, -2047, -1891, -1447, -783
    };
    static uint16_t wave[16];
    int rc = dac1_write(2048);
    if (rc != RT_EOK) return rc;
    for (unsigned i = 0; i < 16; ++i) wave[i] = 2048 + sine[i] * volume / 100;
    return dac1_play(wave, 16, hz * 16);
}

int speaker_stop(void)
{
    int rc = rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    if (rc < 0) return rc;
    rc = dac1_write(2048);
    rt_mutex_release(&mutex);
    return rc;
}

int speaker_tone(uint32_t hz, uint8_t volume_percent, uint32_t duration_ms)
{
    if (hz < 100 || hz > 6000 || volume_percent > 100 || duration_ms > INT32_MAX) return -RT_EINVAL;
    int rc = rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    if (rc != RT_EOK) return rc;
    rc = tone(hz, volume_percent);
    if (rc == RT_EOK && duration_ms) {
        rt_thread_mdelay(duration_ms);
        rc = dac1_write(2048);
    }
    rt_mutex_release(&mutex);
    return rc;
}

int speaker_play(const int16_t *samples, size_t count, uint32_t rate)
{
    if (!samples || ((uintptr_t)samples & 1U) || !count || rate < 1000 || rate > 48000)
        return -RT_EINVAL;
    int rc = rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    if (rc < 0) return rc;
    rc = dac1_write(2048);
    if (rc == RT_EOK) rc = rt_sem_init(&done, "pcm", 0, RT_IPC_FLAG_FIFO);
    if (rc == RT_EOK) {
        next_sample = samples;
        remaining = count;
        fill(0);
        fill(1);
        result = RT_EOK;
        rt_base_t irq = rt_hw_interrupt_disable();
        rc = dac1_play(buffer, 2 * HALF, rate);
        if (rc != RT_EOK) {
            rt_hw_interrupt_enable(irq);
            rt_sem_detach(&done);
            rt_mutex_release(&mutex);
            return rc;
        }
        NVIC_ClearPendingIRQ(DMA2_Channel3_IRQn);
        DMA_ITConfig(DMA2_Channel3, EVENTS, ENABLE);
        NVIC_EnableIRQ(DMA2_Channel3_IRQn);
        rt_hw_interrupt_enable(irq);

        uint64_t ticks = ((uint64_t)count + HALF) * RT_TICK_PER_SECOND / rate;
        ticks += ticks / 50 + RT_TICK_PER_SECOND;
        rc = rt_sem_take(&done, ticks > INT32_MAX ? INT32_MAX : (rt_int32_t)ticks);
        if (rc == RT_EOK) rc = result;
        irq = rt_hw_interrupt_disable();
        TIM_Cmd(TIM6, DISABLE);
        DMA_ITConfig(DMA2_Channel3, EVENTS, DISABLE);
        NVIC_DisableIRQ(DMA2_Channel3_IRQn);
        NVIC_ClearPendingIRQ(DMA2_Channel3_IRQn);
        rt_hw_interrupt_enable(irq);
        int idle_rc = dac1_write(2048);
        if (rc == RT_EOK) rc = idle_rc;
        rt_sem_detach(&done);
    }
    rt_mutex_release(&mutex);
    return rc;
}
