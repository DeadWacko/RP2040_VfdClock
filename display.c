#include "display.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/rtc.h"

// -------------------------------------------------
// Глобальный режим. Изначально показываем время.
volatile display_mode_t g_display_mode = DISPLAY_MODE_TIME;

// Пины, настройка
#define DATA_PIN      15
#define CLOCK_PIN     14
#define LATCH_PIN     13
#define DIGIT_COUNT   4
#define FAST_REFRESH  120  // Гц

static struct repeating_timer g_fast_timer;
static struct repeating_timer g_time_timer;

static volatile uint8_t display_buffer[DIGIT_COUNT] = {10,10,10,10};
static volatile uint8_t current_digit = 0;

/*
 * Коды для сегментов (0..9 и clear=10)
 */
static const uint8_t SEGMENT_CODES[11] = {
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
    0b00000000  // clear (10)
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

static void latch() {
    gpio_put(LATCH_PIN, 1);
    gpio_put(LATCH_PIN, 0);
}

/**
 * Быстрая перерисовка (каждые ~2 мс),
 * Мультиплексирование четырёх разрядов.
 */
static void display_update() {
    // 1) Гасим
    shift_out(0x00);
    shift_out(0x00);
    latch();

    // 2) Включаем нужный разряд
    shift_out(1 << current_digit);
    shift_out(SEGMENT_CODES[display_buffer[current_digit]]);
    latch();

    // 3) Следующий разряд
    current_digit++;
    if (current_digit >= DIGIT_COUNT) {
        current_digit = 0;
    }
}

/**
 * Таймерный колбэк, который рисует дисплей (быстро)
 */
static bool fast_timer_cb(struct repeating_timer *t) {
    display_update();
    return true;
}

/**
 * Таймерный колбэк, каждые 200 мс берём время из RTC
 * но только если g_display_mode == TIME.
 */
static bool time_timer_cb(struct repeating_timer *t) {
    if (g_display_mode == DISPLAY_MODE_TIME) {
        datetime_t now;
        rtc_get_datetime(&now);

        int min = now.min;
        int sec = now.sec;
        display_buffer[0] = (min / 10) % 10;
        display_buffer[1] = (min % 10);
        display_buffer[2] = (sec / 10) % 10;
        display_buffer[3] = (sec % 10);
    }
    return true;
}

// ------------------------------------------------------
// Публичные функции
// ------------------------------------------------------
void display_init(void) {
    init_gpio();
    // Запуск "быстрого" таймера ~120 Гц * 4 = 480 Гц
    add_repeating_timer_ms((1000 / (FAST_REFRESH * DIGIT_COUNT)),
                           fast_timer_cb, NULL, &g_fast_timer);
}

void display_start_time_timer(void) {
    add_repeating_timer_ms(200, time_timer_cb, NULL, &g_time_timer);
}

// ------------------------------------------------------
// Эффект 1: пока подключаемся к Wi-Fi
// ------------------------------------------------------
void effect_1_connecting_wifi(void) {
    g_display_mode = DISPLAY_MODE_EFF1;

    // Пример: мигаем все сегменты "8" -> clear, повторяем 5 раз
    for (int i = 0; i < 5; i++) {
        // Все "8"
        for (int d = 0; d < DIGIT_COUNT; d++) {
            display_buffer[d] = 8;
        }
        sleep_ms(300);

        // Clear
        for (int d = 0; d < DIGIT_COUNT; d++) {
            display_buffer[d] = 10;
        }
        sleep_ms(300);
    }

    // Возвращаемся к TIME
    g_display_mode = DISPLAY_MODE_TIME;
}

// ------------------------------------------------------
// Эффект 2: пока идёт NTP-синхронизация
// ------------------------------------------------------
void effect_2_ntp_sync(void) {
    g_display_mode = DISPLAY_MODE_EFF2;

    // Пример: "бегущая восьмёрка" слева направо и обратно
    int pattern[] = {0,1,2,3,2,1};
    int pattern_len = 6;
    // Повторим 3 раза
    for (int loop = 0; loop < 3; loop++) {
        for (int i = 0; i < pattern_len; i++) {
            // Очистим все
            for (int d = 0; d < DIGIT_COUNT; d++) {
                display_buffer[d] = 10;
            }
            // В нужный разряд ставим "8"
            display_buffer[ pattern[i] ] = 8;
            sleep_ms(150);
        }
    }

    // В конце мигаем полным "8888"
    for (int d = 0; d < DIGIT_COUNT; d++) display_buffer[d] = 8;
    sleep_ms(500);

    // Clear
    for (int d = 0; d < DIGIT_COUNT; d++) display_buffer[d] = 10;
    sleep_ms(300);

    g_display_mode = DISPLAY_MODE_TIME;
}

// ------------------------------------------------------
// Новая функция: бегущая строка из цифр
// ------------------------------------------------------
void display_scrolling_digits(const char *digits) {
    // Если вдруг передали NULL или пустую строку — выходим
    if (!digits || !digits[0]) {
        return;
    }

    g_display_mode = DISPLAY_MODE_SCROLL;

    // Собираем только цифры в локальный буфер
    static const int MAX_SCROLL_DIGITS = 64; // или любое ограничение
    uint8_t scroll_buf[MAX_SCROLL_DIGITS];
    int len = 0;

    while (*digits && len < MAX_SCROLL_DIGITS) {
        if (*digits >= '0' && *digits <= '9') {
            scroll_buf[len++] = *digits - '0';
        }
        digits++;
    }
    // Теперь у нас в scroll_buf лежат "сырые" цифры (0..9), длиной len

    // Логика прокрутки:
    // Шаги прокрутки (step) идут от 0 до (len + DIGIT_COUNT - 1).
    // На каждом шаге мы отображаем окно из 4 символов (или меньше),
    // постепенно сдвигаясь.
    for (int step = 0; step < len + DIGIT_COUNT; step++) {
        for (int d = 0; d < DIGIT_COUNT; d++) {
            int idx = step + d - (DIGIT_COUNT - 1);
            if (idx < 0 || idx >= len) {
                display_buffer[d] = 10; // clear
            } else {
                display_buffer[d] = scroll_buf[idx];
            }
        }
        sleep_ms(300); // Скорость прокрутки
    }

    // Восстанавливаем режим TIME
    g_display_mode = DISPLAY_MODE_TIME;
}
