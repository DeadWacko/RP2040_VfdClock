/**
 * @file main.c
 * @brief Пример использования дисплея: инициализация, показ времени, эффекты и вывод текста.
 */

#include "pico/stdlib.h"
#include "display/display.h"
#include <stdio.h>

int main() {
    stdio_init_all();
    sleep_ms(1000); // Задержка для стабильной инициализации

    // Инициализируем дисплей
    display_init();
    // Запускаем таймер обновления времени (каждые 200 мс)
    display_start_time_timer();

    // Демонстрация различных эффектов и выводов
    printf("[MAIN] Запущено демонстрационное приложение дисплея");

    // Выводим цифровое время (в режиме TIME) на 10 секунд
    sleep_ms(10000);

    // Эффект при подключении к Wi‑Fi
    effect_1_connecting_wifi();
    sleep_ms(2000);

    // Эффект NTP-синхронизации
    effect_2_ntp_sync();
    sleep_ms(2000);

    // Бегущая строка цифр
    display_scrolling_digits("12345");
    sleep_ms(2000);

    // Бегущая строка текста
    display_show_text("HELLO WORLD");
    sleep_ms(2000);

    // Статический вывод текста
    display_print("TEST");
    sleep_ms(3000);

    // Статический вывод цифрового значения
    display_print_digital(2025);
    sleep_ms(3000);

    // Эффект "волна"
    effect_3_wave();
    sleep_ms(2000);

    // Эффект "заполнение центра"
    effect_4_fill_center();
    sleep_ms(2000);

    // изменение яркости
    for(int i = 0; i< 100; i++){
        display_set_brightness(i);
        sleep_ms(100);
    }

    sleep_ms(2000);

    // включение отображение точки
    display_set_dot_blinking(true);

    sleep_ms(4000);

    // выключение отображения точки
    display_set_dot_blinking(false);
    sleep_ms(4000);

    // отображаем время вернувшись в режим Time



    // Бесконечный цикл для демонстрации работы таймеров дисплея
    while (true) {
        sleep_ms(1000);
    }

    return 0;
}
