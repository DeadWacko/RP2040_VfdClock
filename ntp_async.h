#ifndef NTP_ASYNC_H
#define NTP_ASYNC_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>  // Для printf
#include "lwip/err.h"     // Для err_t
#include "lwip/ip_addr.h" // Для ip_addr_t

// ---------------- Уровни логирования ----------------
#define LOG_LEVEL_OFF   -1  ///< Полное отключение логов
#define LOG_LEVEL_ERROR  2   ///< Только ошибки
#define LOG_LEVEL_INFO   1   ///< Информация и ошибки
#define LOG_LEVEL_DEBUG  0   ///< Отладка, информация и ошибки

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_DEBUG  ///< Уровень логирования по умолчанию
#endif

// Макросы логирования (обёрнуты в do-while для безопасности)
#if LOG_LEVEL > LOG_LEVEL_OFF
    #define LOG_ERROR(fmt, ...) do { if (LOG_LEVEL <= LOG_LEVEL_ERROR) printf("[ERROR] " fmt "\n", ##__VA_ARGS__); } while(0)
    #define LOG_INFO(fmt, ...)  do { if (LOG_LEVEL <= LOG_LEVEL_INFO)  printf("[INFO] "  fmt "\n", ##__VA_ARGS__); } while(0)
    #define LOG_DEBUG(fmt, ...) do { if (LOG_LEVEL <= LOG_LEVEL_DEBUG) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__); } while(0)
#else
    #define LOG_ERROR(fmt, ...) do {} while(0)
    #define LOG_INFO(fmt, ...)  do {} while(0)
    #define LOG_DEBUG(fmt, ...) do {} while(0)
#endif

// ---------------- Параметры NTP ----------------
#define MAX_NTP_SERVERS 3  ///< Максимальное количество NTP-серверов

/// Интервал опроса NTP-сервера (в миллисекундах). После завершения опроса (успеха или ошибки)
/// следующий запрос будет запущен через NTP_POLL_INTERVAL_MS мс.
#define NTP_POLL_INTERVAL_MS 3600000

// ---------------- Конфигурация светодиода ----------------
/// Измените LED_PIN, если необходимо (например, для Pico встроенный LED на GPIO 25)
#define LED_PIN 3  ///< Номер пина для LED-индикации
#define ENABLE_LED_INDICATION 1
/**
 * @brief Перечисление состояний LED.
 */
typedef enum {
    LED_OFF = 0,          ///< Светодиод выключен
    LED_WIFI_CONNECT,     ///< Подключение к Wi‑Fi (медленное мигание)
    LED_NTP_SYNC,         ///< Синхронизация NTP (быстрое мигание)
    LED_SUCCESS,          ///< Успешная синхронизация (3 быстрых мигания)
    LED_ERROR             ///< Ошибка (3 медленных мигания)
} led_state_t;

/**
 * @brief Инициализирует пин LED.
 *
 * Настраивает пин LED_PIN как выход и устанавливает его в состояние OFF.
 */
void led_init(void);

/**
 * @brief Устанавливает режим индикации LED.
 *
 * В зависимости от выбранного режима LED либо мигает с определённой частотой,
 * либо выполняет кратковременную последовательность миганий.
 *
 * @param state Желаемое состояние LED.
 */
void led_set_state(led_state_t state);

/**
 * @brief Включает или отключает LED-индикацию.
 *
 * Если индикация отключена, вызовы led_set_state будут игнорироваться, а LED сразу выключается.
 *
 * @param enable Если true – индикация включена, если false – отключена.
 */
void led_indication_enable(bool enable);

/**
 * @brief Инициализация Wi‑Fi и создание UDP PCB для NTP.
 *
 * Настраивает Wi‑Fi, устанавливает режим STA и подключается к указанной сети.
 * При успешном подключении создаётся UDP PCB для работы с NTP.
 *
 * @param ssid     SSID Wi‑Fi сети.
 * @param password Пароль Wi‑Fi.
 * @return true, если подключение прошло успешно; false – в случае ошибки.
 */
bool ntp_async_init(const char *ssid, const char *password);

/**
 * @brief Запускает асинхронный запрос времени по NTP.
 *
 * Отправляет запрос на NTP-сервер и устанавливает таймаут ожидания ответа.
 */
void ntp_async_start(void);

/**
 * @brief Флаг, указывающий, что в данный момент идёт синхронизация времени.
 */
extern volatile bool g_ntp_in_progress;

/**
 * @brief Возвращает строку с текущим IP-адресом интерфейса STA.
 *
 * @return Строка с IP-адресом, например, "192.168.1.100".
 */
const char* wifi_get_ip_str(void);

#endif // NTP_ASYNC_H
