#ifndef DISPLAY_H
#define DISPLAY_H

#include "hardware/rtc.h"

/**
 * Режимы отображения:
 * - TIME: показываем реальное время из RTC
 * - EFF1: эффект во время подключения к Wi-Fi
 * - EFF2: эффект во время NTP-синхронизации
 * - SCROLL: эффект "бегущая строка" (числовая)
 */
typedef enum {
    DISPLAY_MODE_TIME = 0,
    DISPLAY_MODE_EFF1,
    DISPLAY_MODE_EFF2,
    DISPLAY_MODE_SCROLL
} display_mode_t;

/**
 * Глобальная переменная, указывающая, что именно сейчас рисовать
 */
extern volatile display_mode_t g_display_mode;

/**
 * Инициализация GPIO + таймер для быстрой динамической индикации
 */
void display_init(void);

/**
 * Запуск таймера, который каждые 200 мс берёт время из RTC
 * (но только если g_display_mode == DISPLAY_MODE_TIME)
 */
void display_start_time_timer(void);

/**
 * Эффект №1 (например, пока подключаемся к Wi-Fi)
 * Блокирующий, ~несколько секунд.
 */
void effect_1_connecting_wifi(void);

/**
 * Эффект №2 (пока идёт NTP-синхронизация)
 * Блокирующий, ~несколько секунд.
 */
void effect_2_ntp_sync(void);

/**
 * @brief Бегущая строка цифр (например "12345").
 *        Во время прокрутки отключаем отображение реального времени,
 *        в конце возвращаемся в режим TIME.
 * @param digits - строка только из цифр ('0'...'9'), другие символы игнорируются.
 */
void display_scrolling_digits(const char *digits);

#endif // DISPLAY_H
