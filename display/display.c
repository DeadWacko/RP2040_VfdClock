#include "display.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/rtc.h"

// Для логирования (если в проекте нет глобальных определений, можно использовать printf)
#ifndef LOG_INFO
    #define LOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#endif
#ifndef LOG_ERROR
    #define LOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#endif

// -------------------------------------------------
// Глобальный режим. Изначально режим TIME.
volatile display_mode_t g_display_mode = DISPLAY_MODE_TIME;

// Пины дисплея и параметры
#define DATA_PIN      15
#define CLOCK_PIN     14
#define LATCH_PIN     13
#define DIGIT_COUNT   4
#define FAST_REFRESH  120  // Частота обновления (Гц)

volatile uint8_t display_brightness = 100; // Яркость дисплея (0-100%)

static struct repeating_timer g_fast_timer;
static struct repeating_timer g_time_timer;
// Таймер для моргания точки
static struct repeating_timer dot_timer;

static bool dot_blink_flag = false;
static uint8_t res = 0b00000000;

// Буфер для значений каждого разряда дисплея.
// В режимах TIME, EFF1, EFF2, SCROLL здесь хранятся индексы для массива SEGMENT_CODES,
// а в режиме STATIC – непосредственно сегментные коды.
static volatile uint8_t display_buffer[DIGIT_COUNT] = {8, 8, 8, 0};
static volatile uint8_t current_digit = 0;

// Сегментные коды для цифр 0..9, clear и точки (индекс 10 = clear, 11 = точка)
static const uint8_t SEGMENT_CODES[12] = {
    0b01111011, // 0
    0b00000011, // 1
    0b01011110, // 2
    0b01001111, // 3
    0b00100111, // 4
    0b01101101, // 5
    0b01111101, // 6
    0b01000011, // 7
    0b01111111, // 8
    0b01101111, // 9
    0b00000000, // clear
    0b10000000  // point
};

// Таблица символов для отображения (A-Z, 0-9, -, _ и пробел)
static const uint8_t CHAR_MAP[128] = {
    ['0'] = 0b01111011, ['1'] = 0b00000011, ['2'] = 0b01011110, ['3'] = 0b01001111,
    ['4'] = 0b00100111, ['5'] = 0b01101101, ['6'] = 0b01111101, ['7'] = 0b01000011,
    ['8'] = 0b01111111, ['9'] = 0b01101111,
    ['A'] = 0b01110111, ['B'] = 0b00111101, ['C'] = 0b01111001, ['D'] = 0b00011111,
    ['E'] = 0b01111101, ['F'] = 0b01110101, ['G'] = 0b01111011, ['H'] = 0b00110111,
    ['I'] = 0b00000110, ['J'] = 0b00011110, ['K'] = 0b01110110, ['L'] = 0b00111001,
    ['M'] = 0b01010100, ['N'] = 0b00110111, ['O'] = 0b00111111, ['P'] = 0b01110011,
    ['Q'] = 0b01100111, ['R'] = 0b00110001, ['S'] = 0b01101101, ['T'] = 0b00111100,
    ['U'] = 0b00111110, ['V'] = 0b00011110, ['W'] = 0b00101010, ['X'] = 0b00110110,
    ['Y'] = 0b00101111, ['Z'] = 0b01011010,
    ['-'] = 0b00000010, ['_'] = 0b00001000, [' '] = 0b00000000
};

static void init_gpio(void) {
    gpio_init(DATA_PIN);
    gpio_init(CLOCK_PIN);
    gpio_init(LATCH_PIN);
    gpio_set_dir(DATA_PIN, GPIO_OUT);
    gpio_set_dir(CLOCK_PIN, GPIO_OUT);
    gpio_set_dir(LATCH_PIN, GPIO_OUT);
}

static void shift_out(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        gpio_put(DATA_PIN, (data >> (7 - i)) & 1);
        gpio_put(CLOCK_PIN, 1);
        gpio_put(CLOCK_PIN, 0);
    }
}

static void latch(void) {
    gpio_put(LATCH_PIN, 1);
    gpio_put(LATCH_PIN, 0);
}

// Для регулировки яркости и минимизации артефактов мы будем использовать один будильник для очистки дисплея.
static alarm_id_t display_clear_alarm = 0;
static int64_t display_clear(alarm_id_t id, void *user_data) {
    shift_out(0x00);
    shift_out(0x00);
    latch();
    display_clear_alarm = 0;
    return 0;
}

/**
 * Обновление дисплея. Выбирается активный разряд, отправляются данные,
 * затем планируется кратковременная очистка дисплея для регулирования яркости.
 */
static void display_update(void) {
    // Отправляем выбор разряда:
    shift_out(1 << current_digit);

    if (g_display_mode == DISPLAY_MODE_STATIC) {
        // Если режим STATIC – используем непосредственно код из буфера
        res = display_buffer[current_digit];
        if (dot_blink_flag && current_digit == 1) {
            res |= (1 << 7);
        }
    } else {
        // Иначе буфер содержит индекс для SEGMENT_CODES
        res = SEGMENT_CODES[display_buffer[current_digit]];
        if (dot_blink_flag && current_digit == 1) {
            res |= (1 << 7);
        }
    }
    shift_out(res);
    latch();

    // Переходим к следующему разряду
    current_digit++;
    if (current_digit >= DIGIT_COUNT) {
        current_digit = 0;
    }
    
    // Если предыдущий будильник на очистку ещё активен, отменяем его.
    if (display_clear_alarm) {
        cancel_alarm(display_clear_alarm);
        display_clear_alarm = 0;
    }
    // Планируем очистку дисплея через время, зависящее от яркости.
    display_clear_alarm = add_alarm_in_us(((display_brightness * 2000) / 100), display_clear, NULL, true);
}

static bool fast_timer_cb(struct repeating_timer *t) {
    display_update();
    return true;
}

static bool time_timer_cb(struct repeating_timer *t) {
    if (g_display_mode == DISPLAY_MODE_TIME) {
        datetime_t now;
        rtc_get_datetime(&now);
        int hour = now.hour;
        int min = now.min;
        display_buffer[0] = (hour / 10) % 10;
        display_buffer[1] = (hour % 10);
        display_buffer[2] = (min / 10) % 10;
        display_buffer[3] = (min % 10);
    }
    return true;
}

static bool dot_timer_callback(struct repeating_timer *rt) {
    dot_blink_flag = !dot_blink_flag;
    return true;
}

void display_init(void) {
    init_gpio();
    // Запуск быстрого таймера: период = 1 сек / (FAST_REFRESH * DIGIT_COUNT)
    add_repeating_timer_us(1000000 / (FAST_REFRESH * DIGIT_COUNT),
                           fast_timer_cb, NULL, &g_fast_timer);
    LOG_INFO("[DISPLAY] Дисплей и быстрый таймер инициализированы");
    // Таймер для моргания точки запускается при вызове display_set_dot_blinking()
}

void display_start_time_timer(void) {
    add_repeating_timer_ms(200, time_timer_cb, NULL, &g_time_timer);
    LOG_INFO("[DISPLAY] Таймер обновления времени запущен");
}

void display_set_brightness(uint8_t brightness) {
    if (brightness > 100) brightness = 100;
    display_brightness = brightness;
    LOG_INFO("[DISPLAY] Яркость установлена на %d%%", brightness);
}

void display_set_dot_blinking(bool enable) {
    static bool dot_blinking_active = false;
    if (enable && !dot_blinking_active) {
        add_repeating_timer_ms(1000, dot_timer_callback, NULL, &dot_timer);
        dot_blinking_active = true;
        LOG_INFO("[DISPLAY] Моргание точки включено");
    } else if (!enable && dot_blinking_active) {
        cancel_repeating_timer(&dot_timer);
        dot_blinking_active = false;
        dot_blink_flag = false;
        LOG_INFO("[DISPLAY] Моргание точки выключено");
    }
}

void effect_1_connecting_wifi(void) {
    LOG_INFO("[DISPLAY] Запуск эффекта 1 (подключение к Wi‑Fi)");
    g_display_mode = DISPLAY_MODE_EFF1;
    for (int i = 0; i < 5; i++) {
        for (int d = 0; d < DIGIT_COUNT; d++) {
            display_buffer[d] = 8;
        }
        sleep_ms(300);
        for (int d = 0; d < DIGIT_COUNT; d++) {
            display_buffer[d] = 10;
        }
        sleep_ms(300);
    }
    g_display_mode = DISPLAY_MODE_TIME;
    LOG_INFO("[DISPLAY] Эффект 1 завершён");
}

void effect_2_ntp_sync(void) {
    LOG_INFO("[DISPLAY] Запуск эффекта 2 (NTP-синхронизация)");
    g_display_mode = DISPLAY_MODE_EFF2;
    int pattern[] = {0,1,2,3,2,1};
    int pattern_len = 6;
    for (int loop = 0; loop < 3; loop++) {
        for (int i = 0; i < pattern_len; i++) {
            for (int d = 0; d < DIGIT_COUNT; d++) {
                display_buffer[d] = 10;
            }
            display_buffer[pattern[i]] = 8;
            sleep_ms(150);
        }
    }
    for (int d = 0; d < DIGIT_COUNT; d++) display_buffer[d] = 8;
    sleep_ms(500);
    for (int d = 0; d < DIGIT_COUNT; d++) display_buffer[d] = 10;
    sleep_ms(300);
    g_display_mode = DISPLAY_MODE_TIME;
    LOG_INFO("[DISPLAY] Эффект 2 завершён");
}

void display_scrolling_digits(const char *digits) {
    if (!digits || !digits[0]) return;
    LOG_INFO("[DISPLAY] Запуск бегущей строки цифр");
    g_display_mode = DISPLAY_MODE_SCROLL;
    static const int MAX_SCROLL_DIGITS = 64;
    uint8_t scroll_buf[MAX_SCROLL_DIGITS];
    int len = 0;
    while (*digits && (len < MAX_SCROLL_DIGITS)) {
        if (*digits == 0) break;
        if (*digits >= '0' && *digits <= '9') {
            scroll_buf[len++] = (uint8_t)(*digits - '0');
        } else if (*digits == ' ') {
            scroll_buf[len++] = 10;
        } else if (*digits == '.') {
            scroll_buf[len++] = 11;
        }
        digits++;
    }
    for (int step = 0; step < (len + DIGIT_COUNT); step++) {
        for (int d = 0; d < DIGIT_COUNT; d++) {
            int idx = step + d - (DIGIT_COUNT - 1);
            display_buffer[d] = (idx < 0 || idx >= len) ? 10 : scroll_buf[idx];
        }
        sleep_ms(300);
    }
    g_display_mode = DISPLAY_MODE_TIME;
    LOG_INFO("[DISPLAY] Бегущая строка цифр завершена");
}

void display_show_text(const char *text) {
    LOG_INFO("[DISPLAY] Запуск бегущей строки текста");
    g_display_mode = DISPLAY_MODE_SCROLL;
    static const int MAX_TEXT_LEN = 64;
    uint8_t text_buffer[MAX_TEXT_LEN];
    int len = 0;
    while (*text && len < MAX_TEXT_LEN) {
        char ch = *text;
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        text_buffer[len++] = (ch < 128) ? CHAR_MAP[(uint8_t)ch] : 0b00000000;
        text++;
    }
    for (int step = 0; step < (len + DIGIT_COUNT); step++) {
        for (int d = 0; d < DIGIT_COUNT; d++) {
            int idx = step + d - (DIGIT_COUNT - 1);
            display_buffer[d] = (idx >= 0 && idx < len) ? text_buffer[idx] : 10;
        }
        sleep_ms(300);
    }
    g_display_mode = DISPLAY_MODE_TIME;
    LOG_INFO("[DISPLAY] Бегущая строка текста завершена");
}

void display_print(const char *text) {
    LOG_INFO("[DISPLAY] Вывод статического текста: %s", text);
    g_display_mode = DISPLAY_MODE_STATIC;
    for (int i = 0; i < DIGIT_COUNT; i++) {
        if (text && text[i] != '\0') {
            char ch = text[i];
            if (ch >= 'a' && ch <= 'z') ch -= 32;
            display_buffer[i] = (ch < 128) ? CHAR_MAP[(uint8_t)ch] : 0b00000000;
        } else {
            display_buffer[i] = 10; // clear
        }
    }
}

void display_print_digital(int number) {
    LOG_INFO("[DISPLAY] Вывод цифрового значения: %d", number);
    g_display_mode = DISPLAY_MODE_STATIC;
    for (int i = 0; i < DIGIT_COUNT; i++) {
         display_buffer[i] = SEGMENT_CODES[10]; // clear
    }
    int is_negative = 0;
    if (number < 0) {
         is_negative = 1;
         number = -number;
    }
    int pos = DIGIT_COUNT - 1;
    if (number == 0) {
         display_buffer[pos] = SEGMENT_CODES[0];
         pos--;
    } else {
         while (number > 0 && pos >= 0) {
              int digit = number % 10;
              number /= 10;
              display_buffer[pos] = SEGMENT_CODES[digit];
              pos--;
         }
    }
    if (is_negative && pos >= 0) {
         display_buffer[pos] = 0b00000010; // знак минуса
    }
}

void effect_3_wave(void) {
    LOG_INFO("[DISPLAY] Запуск эффекта 'волна'");
    g_display_mode = DISPLAY_MODE_EFF1;
    for (int i = 0; i < 10; i++) {
        for (int d = 0; d < DIGIT_COUNT; d++) {
            display_buffer[d] = (i + d) % 10;
        }
        sleep_ms(150);
    }
    g_display_mode = DISPLAY_MODE_TIME;
    LOG_INFO("[DISPLAY] Эффект 'волна' завершён");
}

void effect_4_fill_center(void) {
    LOG_INFO("[DISPLAY] Запуск эффекта 'заполнение центра'");
    g_display_mode = DISPLAY_MODE_EFF2;
    for (int i = 0; i < 10; i++) {
        display_buffer[1] = i;
        display_buffer[2] = i;
        sleep_ms(150);
    }
    g_display_mode = DISPLAY_MODE_TIME;
    LOG_INFO("[DISPLAY] Эффект 'заполнение центра' завершён");
}
