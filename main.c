#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/rtc.h"

#include "display.h"
#include "ntp_async.h"
#include "wifi_credentials.h"

#include "buttons.h"
/**
 * Демонстрация:
 * 1) Включаем эффект_1 (подключаемся к Wi-Fi)
 * 2) Когда подключились — переходим к normal time
 * 3) Сразу вызываем ntp_async_start(), 
 *    перед этим (по желанию) запускаем effect_2. 
 * 4) После эффекта_2 => возвращаемся к normal time.
 * 5) Как только NTP придёт — RTC обновится.
 * 
 * Если хотите периодически вызывать ntp_async_start(),
 * можно запустить repeating_timer, 
 * либо вызывать вручную в while(true).
 */ 


// Пины: GP16,GP17,GP18
// Допустим, хотим, чтобы long-press:
// - для кнопки 1: 2 секунды,
// - для кнопки 2: 3 секунды,
// - для кнопки 3: 5 секунд.
button_config_t btn_configs[BUTTON_ID_COUNT] = {
        {16, 2000},
        {17, 3000},
        {18, 5000}
    };
const uint UPDATE_PERIOD_MS = 10;



int main() {
     stdio_init_all();
    sleep_ms(2000);

    printf("\n=== NTP + Effects Demo ===\n");
    // 1) Инициализируем RTC (задаём начальное время)
    rtc_init();
    datetime_t start_t = {
        .year = 2025,
        .month=2,
        .day=20,
        .dotw=2,
        .hour=12,
        .min=34,
        .sec=0
    };
    rtc_set_datetime(&start_t);

    // 2) Инициализация дисплея
    display_init();
    display_start_time_timer();

    // 3) Эффект №1 (подключение Wi-Fi)
    printf("[MAIN] Start effect_1 (Wi-Fi connect)\n");
    effect_1_connecting_wifi();  // Блокирующая, ~3 сек

    // 4) Инициализация Wi-Fi, NTP
    if (!ntp_async_init(WIFI_SSID, WIFI_PASSWORD)) {
        printf("[MAIN] Wi-Fi init failed or connect fail.\n");
        while(true) { sleep_ms(1000); }
    }

    // 5) Эффект №2 (NTP sync)
    printf("[MAIN] Start effect_2 (NTP sync)\n");
    effect_2_ntp_sync();  // тоже блокирующая

    // 6) Запуск самого запроса (асинхронно)
    ntp_async_start();
    
    //Тест бегущей строки.
    // printf("My IP is: %s\n", wifi_get_ip_str());

    //вызов бегущей строки цифр с отображением ip 3 раза:
    // for(int i = 0; i <4; i++){
    //     display_scrolling_digits(wifi_get_ip_str());
    // }

    //инициализируем кнопки.
        buttons_init(btn_configs);


    // 7) Основной цикл — ничего не делаем, 
    //    Wi-Fi работает в background (threadsafe).
    while(true) {
        // Обновляем кнопки
        buttons_update();
        button_event_t evt;
        while ((evt = buttons_get_event()) != BUTTON_EVENT_NONE) {
            switch(evt) {
                case BUTTON_EVENT_SHORT_PRESS_1:
                    printf("Short press 1\n");
                    display_scrolling_digits("1");
                    break;
                case BUTTON_EVENT_SHORT_PRESS_2:
                    printf("Short press 2\n");
                    display_scrolling_digits("2");
                    break;
                case BUTTON_EVENT_SHORT_PRESS_3:
                    printf("Short press 3 \n");
                    display_scrolling_digits("3");
                    
                    break;
                case BUTTON_EVENT_LONG_PRESS_1:
                    printf("Long press 1\n");
                    display_scrolling_digits("4");
                    break;
                case BUTTON_EVENT_LONG_PRESS_2:
                    printf("Long press 2\n");
                    display_scrolling_digits("5");
                    break;
                case BUTTON_EVENT_LONG_PRESS_3:
                    printf("Long press 3\n");
                    display_scrolling_digits("6");
                    break;
                default:
                    break;
            }
        }
        sleep_ms(10);
    }
    return 0;
}