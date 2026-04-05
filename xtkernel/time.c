#include <xt/time.h>
#include <xt/kernel.h>

static int is_leap_year(uint32_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static uint16_t days_in_month(uint32_t year, uint8_t month) {
    static const uint8_t days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year))
        return 29;
    return days[month - 1];
}

static uint16_t days_before_month(uint32_t year, uint8_t month) {
    uint16_t total = 0;
    for (uint8_t m = 1; m < month; ++m)
        total += days_in_month(year, m);
    return total;
}


// Преобразование XTTime в Unix timestamp (UTC)
XTResult XTEXPORT xtMakeTime(XTTime* time, uint64_t* unixtime) {
    XT_CHECK_ARG_IS_NULL(time);
    XT_CHECK_ARG_IS_NULL(unixtime);

    // Базовая валидация полей
    if (time->seconds > 59 || time->minutes > 59 || time->hour > 23 ||
        time->mday < 1 || time->mday > 31 || time->month < 1 || time->month > 12 ||
        time->year < 1970)   // Unix timestamp для uint64_t начинается с 1970
        return XT_INVALID_PARAMETER;

    // Проверка дня месяца с учётом конкретного месяца и года
    if (time->mday > days_in_month(time->year, time->month))
        return XT_INVALID_PARAMETER;

    // Суммируем дни за полные годы с 1970 по (year-1)
    uint64_t days = 0;
    for (uint32_t y = 1970; y < time->year; ++y) {
        days += is_leap_year(y) ? 366 : 365;
    }

    // Добавляем дни за полные месяцы текущего года
    days += days_before_month(time->year, time->month);

    // Добавляем дни в текущем месяце (mday-1, так как день начинается с 0)
    days += time->mday - 1;

    // Переводим дни в секунды и прибавляем время внутри дня
    *unixtime = days * 86400 +
                time->hour * 3600 +
                time->minutes * 60 +
                time->seconds;

    return XT_SUCCESS;
}

// Преобразование Unix timestamp в XTTime (UTC)
XTResult XTEXPORT xtGetTimeFromUnix(uint64_t unixtime, XTTime* time) {
    if (!time)
        return XT_INVALID_PARAMETER;

    // Отделяем дни от времени внутри суток
    uint64_t days = unixtime / 86400;
    uint32_t secs_of_day = unixtime % 86400;

    time->hour   = secs_of_day / 3600;
    time->minutes = (secs_of_day % 3600) / 60;
    time->seconds = secs_of_day % 60;

    // Определяем год, вычитая дни по годам, начиная с 1970
    uint32_t year = 1970;
    while (days >= (is_leap_year(year) ? 366 : 365)) {
        days -= is_leap_year(year) ? 366 : 365;
        ++year;
    }
    time->year = year;

    // Определяем месяц и день месяца
    uint8_t month = 1;
    while (month <= 12) {
        uint16_t dim = days_in_month(year, month);
        if (days < dim)
            break;
        days -= dim;
        ++month;
    }
    time->month = month;
    time->mday = days + 1;   // дни считаем с 1

    // Вычисляем день недели (wday): 0 = воскресенье, 1 = понедельник, ..., 6 = суббота
    // 1970-01-01 был четвергом (4 в такой шкале)
    uint64_t total_days_since_epoch = unixtime / 86400;
    time->wday = (total_days_since_epoch + 4) % 7;

    // Вычисляем день в году (yday: 0 = 1 января)
    time->yday = days_before_month(year, month) + (time->mday - 1);

    return XT_SUCCESS;
}