#ifndef __XT_TIME_H__
#define __XT_TIME_H__

#include <xt/result.h>
#include <stdint.h>

typedef struct XTTime {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hour;
    uint8_t mday;
    uint8_t wday;
    uint8_t month;
    uint16_t yday;
    uint32_t year;
} XTTime;

XTResult xtMakeTime(XTTime* time, uint64_t* unixtime);
XTResult xtGetTimeFromUnix(uint64_t unixtime, XTTime* time);
XTResult xtGetTime(uint64_t* unixtime, uint64_t* nanoseconds);
XTResult xtSetTime(uint64_t unixtime, uint64_t nanoseconds);

#endif