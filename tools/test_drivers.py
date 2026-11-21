"""Exercise the board drivers against a simulated ADC, two DAC outputs and a CH422 bus."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
RT = """
#pragma once
#include <pthread.h>
#include <stdint.h>
#define RT_EOK 0
#define RT_EBUSY 7
#define RT_ETIMEOUT 2
#define RT_EINVAL 10
#define RT_EIO 8
typedef uint32_t rt_tick_t;
rt_tick_t rt_tick_get(void);
#define rt_tick_from_millisecond(ms) (ms)
void rt_thread_mdelay(int32_t ms);
#define rt_thread_delay(n) ((void)0)
#define RT_ERROR 1
#define RT_WAITING_FOREVER (-1)
#define RT_IPC_FLAG_PRIO 1
struct rt_mutex { pthread_mutex_t native; };
#define rt_thread_self() 1
#define rt_enter_critical() ((void)0)
#define rt_exit_critical() ((void)0)
#define rt_interrupt_get_nest() 0
#define rt_critical_level() 0
#define rt_mutex_init(m,n,f) pthread_mutex_init(&(m)->native, 0)
#define rt_mutex_take(m,t) pthread_mutex_lock(&(m)->native)
#define rt_mutex_release(m) pthread_mutex_unlock(&(m)->native)
#define rt_mutex_detach(m) pthread_mutex_destroy(&(m)->native)
#define INIT_COMPONENT_EXPORT(fn) static void __attribute__((constructor)) run_##fn(void){if(fn()) abort();}
#include <stdlib.h>
#define RT_IPC_CMD_RESET 0
#define RT_NULL NULL
struct rt_semaphore { int released; };
typedef int32_t rt_int32_t;
#define RT_IPC_FLAG_FIFO 0
#define RT_TICK_PER_SECOND 1000
int rt_sem_init(struct rt_semaphore *, const char *, int, int);
int rt_sem_take(struct rt_semaphore *, int);
void rt_sem_release(struct rt_semaphore *);
void rt_sem_detach(struct rt_semaphore *);
int rt_sem_control(struct rt_semaphore *, int, void *);
void rt_interrupt_enter(void);
void rt_interrupt_leave(void);
"""


# Use the vendor's register types and pin, flag and channel values.
CHIP = r"""
#pragma once
#include "ch32v30x_adc.h"
#include "ch32v30x_gpio.h"
#include "ch32v30x_dma.h"
#include "ch32v30x_tim.h"
#include "ch32v30x_rcc.h"
#include "ch32v30x_dac.h"
extern ADC_TypeDef adc_regs;
#undef ADC1
#define ADC1 (&adc_regs)
extern DMA_TypeDef dma2_regs;
#undef DMA2
#define DMA2 (&dma2_regs)
#define ADC_DeInit(a) ((void)0)
#define ADC_Cmd(a,x) ((void)0)
#define ADC_StructInit(a) ((void)0)
#define ADC_Init(a,x) ((void)0)
#define ADC_DMACmd(a,x) ((void)0)
#define ADC_ExternalTrigConvCmd(a,x) ((void)0)
#define ADC_BufferCmd(a,x) ((void)0)
#define ADC_ResetCalibration(a) ((void)0)
#define ADC_StartCalibration(a) ((a)->RDATAR=2048)
#define ADC_ClearFlag(a,f) ((void)0)
void mock_software_trigger(void);
#define ADC_SoftwareStartConvCmd(a,x) mock_software_trigger()
#define TIM_SelectOutputTrigger(t,s) ((void)0)
#define TIM_DMACmd(t,f,e) ((void)0)
#define DAC_StructInit(s) ((void)0)
#define DAC_Init(c,s) ((void)0)
#define DAC_Cmd(c,e) ((void)0)
void mock_adc_prescaler(uint32_t d);
#define RCC_ADCCLKConfig(d) mock_adc_prescaler(d)
#define RCC_AHBPeriphClockCmd(c,e) ((void)0)
#define DMA_ClearFlag(f) ((void)0)
extern TIM_TypeDef timer2, timer10;
#undef TIM10
#define TIM10 (&timer10)
#undef TIM2
#define TIM2 (&timer2)
typedef struct {
    uintptr_t DMA_MemoryBaseAddr, DMA_PeripheralBaseAddr;
    uint32_t DMA_BufferSize, DMA_DIR, DMA_MemoryInc, DMA_PeripheralDataSize, DMA_MemoryDataSize;
    uint32_t DMA_Mode, DMA_M2M, DMA_Priority;
} host_dma_config;
#define DMA_InitTypeDef host_dma_config
void mock_dma_init(DMA_Channel_TypeDef *,host_dma_config *);
#define DMA_Init mock_dma_init

#define RCC_APB1PeriphClockCmd(c,e) ((void)0)
#define RCC_APB2PeriphClockCmd(c,e) ((void)0)

#define GPIO_SetBits(p,b) ((void)0)
#define DMA_ITConfig(c,i,e) ((void)0)
#define NVIC_ClearPendingIRQ(n) ((void)0)
#define NVIC_DisableIRQ(n) ((void)0)
#define NVIC_EnableIRQ(n) ((void)0)
"""

HARNESS = r"""
#include <rtthread.h>
#include "ch32v30x.h"
#include "buzzer.h"
#include "led.h"
#include "adc.h"
#include "dac.h"
#include "dma.h"
#include "speaker.h"
#include "mic.h"
#include "ch422.h"
#include "veml7700.h"
#include <assert.h>
#include <string.h>
#include <sched.h>

TIM_TypeDef timer2, timer10;
static struct rt_semaphore *adc_sem;
ADC_TypeDef adc_regs;
DMA_TypeDef dma2_regs;
static uint8_t selected;
static unsigned sample_window;
static int adc_error, pcm_error, active[3], data_ready;
static uint16_t *frames[3], output[2];
static size_t lengths[3];
static const int16_t *pcm;
static size_t pcm_count;
static unsigned ticks;

rt_tick_t rt_tick_get(void) { return ticks++; }
void ADC_RegularChannelConfig(ADC_TypeDef *adc, uint8_t ch, uint8_t rank, uint8_t time) {
    assert(adc == ADC1 && (ch == 1 || ch == 9) && rank == 1);
    selected = ch; sample_window=time;
    sched_yield();
}
static unsigned route(DMA_Channel_TypeDef *ch) {
    if(ch==DMA1_Channel1) return 0;
    if(ch==DMA2_Channel3) return 1;
    assert(ch==DMA2_Channel4); return 2;
}
static host_dma_config transfers[3];
static int scalar_done[3], scalar_fault;
static unsigned prescaler, trigger_count, last_delay, buzzer_duty, buzzer_hz, buzzer_pin;
void rt_thread_mdelay(int32_t ms) {last_delay=ms;}
void mock_adc_prescaler(uint32_t d) {prescaler=d;}
void RCC_GetClocksFreq(RCC_ClocksTypeDef *clocks) {
    clocks->HCLK_Frequency=144000000; clocks->PCLK1_Frequency=36000000; clocks->PCLK2_Frequency=72000000;
}
void DMA_DeInit(DMA_Channel_TypeDef *ch) {
    unsigned i=route(ch);
    assert(!active[i]); scalar_done[i]=0;
}
void mock_dma_init(DMA_Channel_TypeDef *ch, host_dma_config *cfg) {
    unsigned i=route(ch);
    assert(cfg->DMA_MemoryDataSize==DMA_MemoryDataSize_HalfWord);
    assert(cfg->DMA_PeripheralDataSize==DMA_PeripheralDataSize_HalfWord);
    assert(cfg->DMA_DIR==(i?DMA_DIR_PeripheralDST:DMA_DIR_PeripheralSRC));
    uintptr_t reg = i==0 ? (uintptr_t)&ADC1->RDATAR : i==1 ?
                    (uintptr_t)&DAC->R12BDHR1 : (uintptr_t)&DAC->R12BDHR2;
    assert(cfg->DMA_PeripheralBaseAddr == reg);
    frames[i]=(uint16_t *)cfg->DMA_MemoryBaseAddr; lengths[i]=cfg->DMA_BufferSize;
    transfers[i]=*cfg;
}
void DMA_Cmd(DMA_Channel_TypeDef *ch, FunctionalState en) {
    unsigned i=route(ch);
    active[i]=(en==ENABLE);
    if(en && transfers[i].DMA_M2M==DMA_M2M_Enable) {
        assert(i && lengths[i]==1 && transfers[i].DMA_Mode==DMA_Mode_Normal);
        if (!scalar_fault) {output[i-1]=*frames[i]; scalar_done[i]=1;}
    }
}
FlagStatus DMA_GetFlagStatus(uint32_t flag) {
    if(flag==DMA2_FLAG_TC3 || flag==DMA2_FLAG_TC4) return scalar_done[flag==DMA2_FLAG_TC3?1:2]?SET:RESET;
    if(flag==DMA2_FLAG_TE3 || flag==DMA2_FLAG_TE4) return scalar_fault==1?SET:RESET;
    if(flag==DMA1_FLAG_TE1) return adc_error==1?SET:RESET;
    assert(flag==DMA1_FLAG_TC1); return data_ready && !adc_error ? SET : RESET;
}
static unsigned led_inits, led_mode, led_pins, preload, staged_writes, updates_blocked;
static uint16_t led_ccr[3], pending_ccr[3];
void TIM_DeInit(TIM_TypeDef *t) { if(t==TIM10) ++led_inits; }
void TIM_ClearFlag(TIM_TypeDef *t, uint16_t flags) { assert(flags==TIM_FLAG_Update); }
void TIM_TimeBaseInit(TIM_TypeDef *t,TIM_TimeBaseInitTypeDef *base) {
    unsigned period=(unsigned)base->TIM_Period+1, divider=(unsigned)base->TIM_Prescaler+1;
    if(t==TIM2) {t->ATRLR=period-1; buzzer_hz=72000000U/(period*divider);}
    if(t==TIM10) {t->ATRLR=period-1; assert(period*divider==144000 && period<=UINT16_MAX);}
}
void GPIO_PinRemapConfig(uint32_t remap,FunctionalState en) {
    assert(remap==GPIO_PartialRemap_TIM10 && en==ENABLE);
    led_mode=1;
}
void GPIO_Init(GPIO_TypeDef *port,GPIO_InitTypeDef *cfg) {
    if(port==GPIOB && cfg->GPIO_Mode==GPIO_Mode_AF_PP) {
        assert(led_mode && cfg->GPIO_Pin==(GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5));
        ++led_pins;
    }
}
static void led_oc(TIM_TypeDef *t,TIM_OCInitTypeDef *pwm) {
    assert(t==TIM10 && pwm->TIM_OCMode==TIM_OCMode_PWM1 && pwm->TIM_OCPolarity==TIM_OCPolarity_Low);
    assert(pwm->TIM_OutputState==TIM_OutputState_Enable && pwm->TIM_Pulse==0);
}
void TIM_OC1Init(TIM_TypeDef *t,TIM_OCInitTypeDef *pwm) { led_oc(t,pwm); }
void TIM_OC2Init(TIM_TypeDef *t,TIM_OCInitTypeDef *pwm) { led_oc(t,pwm); }
void TIM_OC3Init(TIM_TypeDef *t,TIM_OCInitTypeDef *pwm) {
    if(t==TIM2) buzzer_duty=pwm->TIM_Pulse;
    else led_oc(t,pwm);
}
void TIM_OC1PreloadConfig(TIM_TypeDef *t,uint16_t en) {assert(t==TIM10 && en==TIM_OCPreload_Enable); preload|=1;}
void TIM_OC2PreloadConfig(TIM_TypeDef *t,uint16_t en) {assert(t==TIM10 && en==TIM_OCPreload_Enable); preload|=2;}
void TIM_OC3PreloadConfig(TIM_TypeDef *t,uint16_t en) {assert(t==TIM10 && en==TIM_OCPreload_Enable); preload|=4;}
void TIM_CtrlPWMOutputs(TIM_TypeDef *t,FunctionalState en) {assert(t==TIM10 && en && preload==7);}
void TIM_UpdateDisableConfig(TIM_TypeDef *t,FunctionalState en) {
    assert(t==TIM10 && updates_blocked!=(unsigned)en);
    updates_blocked=en;
    if(en) staged_writes=0;
    else {assert(staged_writes==7); memcpy(led_ccr,pending_ccr,sizeof(led_ccr));}
}
static void stage_led(TIM_TypeDef *t,unsigned i,uint16_t value) {
    assert(t==TIM10 && updates_blocked); pending_ccr[i]=value; staged_writes|=1U<<i;
}
void TIM_SetCompare1(TIM_TypeDef *t,uint16_t c) {stage_led(t,0,c);}
void TIM_SetCompare2(TIM_TypeDef *t,uint16_t c) {stage_led(t,1,c);}
void TIM_SetCompare3(TIM_TypeDef *t,uint16_t c) {stage_led(t,2,c);}
static void test_led(void) {
    assert(led_set(0x1000000,100)==-RT_EINVAL && led_inits==0);
    assert(!led_set(LED_COLOR_WHITE,100));
    unsigned full=TIM10->ATRLR+1U;
    assert(led_ccr[0]==full && led_ccr[1]==full && led_ccr[2]==full);
    assert(!led_set(LED_COLOR_CYAN,25) && led_ccr[0]==0 && led_ccr[1]==full/4 && led_ccr[2]==full/4);
    assert(!led_set(0xff8000,50) && led_ccr[0]==full/2 && led_ccr[1]==(full*64U+127U)/255U && led_ccr[2]==0);
    assert(led_set(LED_COLOR_BLUE,101)==-RT_EINVAL && led_ccr[0]==full/2 && led_ccr[2]==0);
    assert(!led_set(LED_COLOR_RED,0) && !led_ccr[0] && !led_ccr[1] && !led_ccr[2]);
    assert(!led_set(LED_COLOR_BLACK,100) && !led_ccr[0] && !led_ccr[1] && !led_ccr[2]);
    assert(led_inits==1 && led_pins==1);
}
static void adc_trigger(void) {
    assert(active[0]);
    assert(transfers[0].DMA_Mode==DMA_Mode_Normal && transfers[0].DMA_MemoryInc==DMA_MemoryInc_Enable);
    sched_yield();
    for(size_t i=0;i<lengths[0];++i) frames[0][i]=(uint16_t)(selected+i);
    ++trigger_count; data_ready=1;
}
void mock_software_trigger(void) {
    assert((ADC1->CTLR2 & ADC_EXTSEL)==ADC_ExternalTrigConv_None);
    assert(!!(ADC1->CTLR2 & ADC_CONT)==(lengths[0]>1));
    adc_trigger();
}
void TIM_Cmd(TIM_TypeDef *timer, FunctionalState en) {
    if(timer==TIM3 && en) {
        assert((ADC1->CTLR2 & ADC_EXTSEL)==ADC_ExternalTrigConv_T3_TRGO);
        adc_trigger();
    }
    if(timer==TIM6 && en) assert(active[1] && transfers[1].DMA_Mode==DMA_Mode_Circular);
    if(timer==TIM7 && en) assert(active[2] && transfers[2].DMA_Mode==DMA_Mode_Circular);
}

void DMA2_Channel3_IRQHandler(void);
void rt_interrupt_enter(void) {}
void rt_interrupt_leave(void) {}
int rt_sem_init(struct rt_semaphore *s, const char *name, int initial, int flags) {
    s->released = 0;
    if (!strcmp(name,"adc_done")) adc_sem=s;
    return RT_EOK;
}
void rt_sem_release(struct rt_semaphore *s) { s->released = 1; }
void rt_sem_detach(struct rt_semaphore *s) {}
int rt_sem_control(struct rt_semaphore *s,int cmd,void *arg) {s->released=0; return 0;}
void DMA1_Channel1_IRQHandler(void);
int rt_sem_take(struct rt_semaphore *s, int timeout) {
    if (s==adc_sem) {
        assert(timeout>0 && data_ready);
        data_ready=0;
        if(adc_error==2) return -RT_ETIMEOUT;
        data_ready=1; DMA1_Channel1_IRQHandler(); data_ready=0;
        assert(s->released); s->released=0;
        return RT_EOK;
    }
    if (pcm_error == 2) return -RT_ETIMEOUT;
    for (size_t pos = 0; !s->released; pos += 64) {
        unsigned half = (pos/64)%2;
        assert(pos < pcm_count && active[1] && active[2]);
        for (unsigned i = 0; i < 64; ++i) {
            int expected = pos+i < pcm_count ? ((int32_t)pcm[pos+i]+32768)>>4 : 2048;
            assert(frames[1][half*64+i] == expected);
        }
        dma2_regs.INTFR = pcm_error ? 0x600U : half ? 0x200U : 0x400U;
        DMA2_Channel3_IRQHandler();
    }
    return RT_EOK;
}

static void *sample_worker(void *arg) {
    int mic = arg != NULL;
    for (unsigned i = 0; i < 100; ++i) {
        uint16_t frame[8] __attribute__((aligned(16)));
        assert((mic ? mic_read() : adc_read(ADC_PB1)) == (mic ? 1 : 9));
        assert((mic ? mic_capture(frame, 8, 8000) : adc_capture(ADC_PB1, frame, 8, 8000)) == 0);
        for (unsigned j = 0; j < 8; ++j) assert(frame[j] == (mic ? 1 : 9) + j);
    }
    return NULL;
}

static unsigned scl=1, sda=1, bit_count, byte, writing, num_bytes;
static uint8_t wire_bytes[32];
void GPIO_WriteBit(GPIO_TypeDef *port, uint16_t pin, BitAction value) {
    if (port==GPIOA) {assert(pin==GPIO_Pin_2); buzzer_pin=value; return;}
    assert(port == GPIOE);
    if (pin == GPIO_Pin_1) {
        if (scl && sda && !value) { writing=1; bit_count=0; byte=0; }
        if (scl && !sda && value) writing=0;
        sda=value;
    } else {
        assert(pin == GPIO_Pin_0);
        if (writing && !scl && value) {
            if (bit_count % 9 < 8) byte=(byte<<1)|sda;
            if (++bit_count % 9 == 0) { wire_bytes[num_bytes++]=byte; byte=0; }
        }
        scl=value;
    }
}
static int light_error, config_writes;
static uint16_t light_config; static int asleep;
int i2c_write_reg(uint8_t a,uint8_t r,const uint8_t *data,uint16_t n) {
    assert(a == 0x10 && (r == 0 || r == 3) && n == 2);
    if (r==0) {
        uint16_t word=data[0]|((uint16_t)data[1]<<8);
        if(word&1) asleep=1;
        else {assert(asleep); asleep=0; light_config=word;}
    }
    ++config_writes;
    return 0;
}
int i2c_read_reg(uint8_t a,uint8_t r,uint8_t *data,uint16_t n) {
    assert(a == 0x10 && r == 4 && n == 2);
    data[0]=100; data[1]=0; return light_error;
}

int main(void) {
    test_led();
    assert(!buzzer_beep(30,2400,120) && buzzer_hz==2400 && buzzer_duty==9000 && last_delay==120 && buzzer_pin==0);
    assert(!buzzer_beep(100,500,0) && buzzer_pin==1);
    assert(!buzzer_beep(0,0,0) && buzzer_pin==0);
    assert(buzzer_beep(101,500,0)==-RT_EINVAL);
    uint16_t wave[]={0,1024,2048,3072};
    assert(!dac_play(wave,4,4000));
    assert(!speaker_tone(440,30,0) && active[1] && active[2] && frames[2]==wave);
    static uint16_t level_frame[128] __attribute__((aligned(256)));
    pthread_t a,b;
    assert(!pthread_create(&a,NULL,sample_worker,NULL));
    assert(!pthread_create(&b,NULL,sample_worker,&b));
    pthread_join(a,NULL); pthread_join(b,NULL);
    assert(mic_level(level_frame,128,8000)==127 && !active[0] && active[1] && active[2]);
    uint16_t frame[4] __attribute__((aligned(8)));
    adc_error=1; assert(mic_capture(frame,4,8000)==-RT_EIO && !active[0]);
    adc_error=2; assert(adc_capture(ADC_PB1,frame,4,8000)==-RT_ETIMEOUT && !active[0]);
    assert(adc_read(ADC_PB1)==-RT_ETIMEOUT);
    adc_error=0; assert(adc_read(ADC_PB1)==9 && mic_read()==1);
    assert(adc_capture(ADC_PB1,(uint16_t *)(uintptr_t)0x2000fffe,2,8000)==-RT_EINVAL);
    assert(prescaler==RCC_PCLK2_Div6 && adc_max_rate_hz()==12000000U/14);
    assert(!adc_capture(ADC_PB1,frame,4,0) && sample_window==0 && frame[3]==12);
    assert(!adc_capture(ADC_PB1,frame,4,500000) && sample_window==1);
    assert(adc_capture(ADC_PB1,frame,4,900000)==-RT_EINVAL);
    assert(!dac_play(wave,4,1000000));
    int16_t samples[259];
    for(unsigned i=0;i<259;++i) samples[i]=(int16_t)(i*997);
    pcm=samples;
    size_t sizes[]={1,64,65,128,129,259};
    for(unsigned i=0;i<6;++i) {
        pcm_count=sizes[i];
        assert(!speaker_play(pcm,pcm_count,16000) && !active[1] && active[2]);
    }
    pcm_error=1; assert(speaker_play(pcm,pcm_count,16000)==-RT_EIO);
    pcm_error=2; assert(speaker_play(pcm,pcm_count,16000)==-RT_ETIMEOUT);
    assert(!speaker_tone(440,30,0));
    assert(!dac_write(1234) && output[1]==1234 && active[1] && !active[2]);
    assert(!speaker_stop() && output[0]==2048);
    assert(!CH422_Show("%.1f",12.3));
    uint8_t expected[]={0x70,0x14,0x72,0xed,0x74,0x5d,0x76,0,0x48,5};
    assert(num_bytes==sizeof(expected) && !memcmp(wire_bytes,expected,sizeof(expected)));
    num_bytes=0; assert(!CH422_Show("1234.5") && wire_bytes[7]==0x3e && num_bytes==8);
    scalar_fault=1; assert(dac_write(5)==-RT_EIO && !active[2]);
    scalar_fault=2; assert(dac_write(5)==-RT_ETIMEOUT && !active[2]);
    scalar_fault=0; assert(!dac_write(5) && output[1]==5);
    float lux=veml7700_lux(100);
    assert(lux>23.03f && lux<23.05f && config_writes==3 && light_config==0x1800);
    assert(veml7700_raw(100)==100 && config_writes==3);
    uint16_t ms[]={25,50,100,200,400,800}, bits[]={12,8,0,1,2,3};
    for(unsigned i=0;i<6;++i) {
        float x=veml7700_lux(ms[i]), expected=2304.0f/ms[i];
        assert(x>expected-0.01f && x<expected+0.01f);
        assert(light_config==(0x1800|(bits[i]<<6)) && last_delay>=ms[i]);
    }
    int before=config_writes;
    assert(veml7700_lux(70)<0 && config_writes==before);
    light_error=1; assert(veml7700_raw(50)<0);
    light_error=0; assert(veml7700_raw(100)==100 && light_config==0x1800);
}
"""

def main():
    with tempfile.TemporaryDirectory() as name:
        stage = Path(name)
        stubs = {
            "core_riscv.h": "#define __I volatile const\n#define __O volatile\n#define __IO volatile\n#define __get_MSTATUS() 8\ntypedef int32_t s32; typedef enum {NoREADY,READY} ErrorStatus; typedef enum {DISABLE,ENABLE} FunctionalState; typedef enum {RESET,SET} FlagStatus,ITStatus;",
            "system_ch32v30x.h": "",
            "ch32v30x_conf.h": CHIP,
            "ch32v30x_it.h": "#define interrupt(x) unused\n#define GET_INT_SP() ((void)0)\n#define FREE_INT_SP() ((void)0)",
            "rtthread.h": RT,
            "rthw.h": "#pragma once\ntypedef long rt_base_t;\n#define rt_hw_interrupt_disable() 0\n#define rt_hw_interrupt_enable(x) ((void)0)\n#define rt_hw_us_delay(x) ((void)0)",

            "i2c.h": "#define I2C_OK 0\n#define I2C_ERR_TIMEOUT (-2)\nint i2c_read_reg(uint8_t,uint8_t,uint8_t*,uint16_t);\nint i2c_write_reg(uint8_t,uint8_t,const uint8_t*,uint16_t);",
            "board_test.c": HARNESS,
        }
        for filename, content in stubs.items():
            (stage / filename).write_text(content, encoding="utf-8")
        sources = []
        for path in ["BSP/dma.c", "BSP/gpio.c", "BSP/tim.c", "HAL/led.c", "HAL/buzzer.c", "BSP/adc.c", "BSP/dac.c", "HAL/mic.c", "HAL/ch422.c",
                     "HAL/speaker.c", "HAL/veml7700.c"]:
            target = Path(shutil.copy(ROOT / path, stage))
            # Host pointers are wider and allocated by the OS, not the MCU linker script.
            if target.name in ("adc.c", "dac.c"):
                source = target.read_text(encoding="utf-8")
                source = source.replace("(uintptr_t)_eusrstack", "UINTPTR_MAX").replace("addr < SRAM_BASE", "!addr")
                target.write_text(source, encoding="utf-8")
            sources.append(str(target))
        includes = [stage, ROOT/"BSP", ROOT/"HAL", ROOT/"Peripheral/inc", ROOT/"LVGL/GUI", ROOT/"LVGL/GUI/lvgl"]
        compiler = os.environ.get("CC", "gcc")
        exe = stage / ("board-test.exe" if os.name == "nt" else "board-test")
        subprocess.run([compiler, "-std=gnu99", "-pthread", "-Werror=implicit-function-declaration",
                        "-DLV_SPRINTF_USE_FLOAT=1", *["-I"+str(p) for p in includes], *sources,
                        str(ROOT/"LVGL/GUI/lvgl/src/misc/lv_printf.c"), str(stage/"board_test.c"),
                        "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    print("PASS: ADC/MIC serialization and recovery, separate DMA outputs, scalar and circular DMA, PCM refill and errors, RGB PWM, buzzer PWM, CH422 bus and six VEML7700 integration times at fixed gain")


if __name__ == "__main__":
    main()
