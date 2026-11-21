#ifndef APP_H
#define APP_H

/* 板载设备的应用入口。也可只包含对应设备的 app_xxx.h。 */
#ifdef __cplusplus
extern "C" {
#endif
#include "app_key.h"
#include "app_rgb.h"
#include "app_buzzer.h"
#include "app_ntc.h"
#include "app_mic.h"
#include "app_analog_input.h"
#include "app_analog_output.h"
#include "app_light.h"
#include "app_imu.h"
#include "app_pd.h"
#include "app_fan.h"
#include "app_heater.h"
#include "app_motor.h"
#include "app_encoder.h"
#include "app_ultrasonic.h"
#include "app_segment.h"
#include "app_speaker.h"
#include "app_ble.h"
#include "app_voice.h"
#include "app_nfc.h"
#include "app_rtc.h"
#include "app_ethernet.h"
#include "app_usb_hub.h"
#ifdef __cplusplus
}
#endif
#endif
