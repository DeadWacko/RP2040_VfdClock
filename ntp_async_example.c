/**
 * @file main.c
 * @brief Пример использования асинхронного NTP-синхронизатора с управлением LED-индикацией
 * и автоматическим опросом NTP-сервера.
 */

#include "pico/stdlib.h"
#include "ntp_async.h"
#include "wifi_credentials.h"
int main() {
    stdio_init_all();
    sleep_ms(1000); // Задержка для стабильной инициализации

    // Инициализируем LED
    led_init();
    
    // Включаем LED-индикацию (если требуется, можно отключить: led_indication_enable(false);)
    led_indication_enable(true);

    // Задайте свои параметры Wi‑Fi


    if (!ntp_async_init(WIFI_SSID, WIFI_PASSWORD)) {
        printf("[MAIN] Ошибка инициализации Wi‑Fi/NTP\n");
        return -1;
    }

    printf("[MAIN] Wi‑Fi подключён, IP: %s\n", wifi_get_ip_str());
    sleep_ms(1000);

    // Запускаем первый запрос времени через NTP
    ntp_async_start();

    // Главный цикл
    while (true) {
        sleep_ms(1000);
    }

    return 0;
}
