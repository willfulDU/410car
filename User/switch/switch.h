#ifndef __SWITCH_H
#define __SWITCH_H

#include "stm32f10x.h"

/* DNR状态定义 */
typedef enum
{
    DNR_NEUTRAL = 0,   // 空档
    DNR_FORWARD,       // 前进
    DNR_REVERSE        // 后退
} DNR_State;

/* 左右转灯状态定义 */
typedef enum
{
    LIGHT_NONE = 0,    // 无灯
    LIGHT_LEFT,        // 左转灯
    LIGHT_RIGHT        // 右转灯
} Light_State;

void Switch_Init(void);

DNR_State Get_DNR(void);
Light_State Get_LeftRight(void);

#endif
