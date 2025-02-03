#ifndef DISPLAY_H
#define DISPLAY_H

#include "hardware/rtc.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * Режимы отображения:
 * - TIME: показываем реальное время из RTC
 * - EFF1: эффект во время подключения к Wi‑Fi
 * - EFF2: эффект во время NTP-синхронизации
 * - SCROLL: эффект "бегущая строка" (числовая)
 * - STATIC: статический вывод информации (без прокрутки)
 */
typedef enum {
    DISPLAY_MODE_TIME = 0,
    DISPLAY_MODE_EFF1,
    DISPLAY_MODE_EFF2,
    DISPLAY_MODE_SCROLL,
    DISPLAY_MODE_STATIC
} display_mode_t;

/**
 * Глобальная переменная, определяющая, какой режим сейчас используется для вывода.
 */
extern volatile display_mode_t g_display_mode;

/**
 * Инициализация GPIO и таймеров дисплея.
 */
void display_init(void);

/**
 * Запуск таймера, который каждые 200 мс обновляет время из RTC,
 * если g_display_mode == DISPLAY_MODE_TIME.
 */
void display_start_time_timer(void);

/**
 * Эффект №1 (например, во время подключения к Wi‑Fi).
 * Блокирующий эффект (~несколько секунд).
 */
void effect_1_connecting_wifi(void);

/**
 * Эффект №2 (во время NTP-синхронизации).
 * Блокирующий эффект (~несколько секунд).
 */
void effect_2_ntp_sync(void);

/**
 * @brief Эффект бегущей строки с цифрами (например, "12345").
 *        Во время прокрутки режим переключается, по окончании возвращается в TIME.
 * @param digits Строка, содержащая цифры ('0'... '9'), пробелы и точки.
 */
void display_scrolling_digits(const char *digits);

/**
 * @brief Эффект бегущей строки с текстом.
 * @param text Строка для отображения (будет прокручиваться).
 */
void display_show_text(const char *text);

/**
 * @brief Устанавливает яркость дисплея.
 * @param brightness Яркость в процентах (0–100).
 */
void display_set_brightness(uint8_t brightness);

/**
 * @brief Включает или отключает моргание точки (раз в секунду).
 * @param enable True – включить, False – выключить.
 */
void display_set_dot_blinking(bool enable);

/**
 * @brief Вывод статического текста на дисплей (без эффекта бегущей строки).
 * Если длина текста меньше числа разрядов – оставшиеся разряды очищаются;
 * если больше – текст обрезается.
 *
 * @param text Строка для отображения.
 */
void display_print(const char *text);

/**
 * Вывод цифрового значения на дисплей (используя сегментные коды цифр).
 * @param number Числовое значение.
 */
void display_print_digital(int number);

/**
 * Новый эффект "волна".
 */
void effect_3_wave(void);

/**
 * Новый эффект "заполнение центра".
 */
void effect_4_fill_center(void);

#endif // DISPLAY_H
