
#ifndef NTP_ASYNC_H
#define NTP_ASYNC_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Инициализация Wi-Fi (threadsafe_background) + создание UDP PCB
 * @return true если ок
 */
bool ntp_async_init(const char *ssid, const char *password);

/**
 * @brief Запуск асинхронной NTP-синхронизации (отправка запроса, ожидание ответа в колбэке)
 */
void ntp_async_start(void);

/**
 * @brief Флаг: сейчас ли идёт синхронизация
 */
extern volatile bool g_ntp_in_progress;

#endif // NTP_ASYNC_H