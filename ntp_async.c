#include <stdio.h>
#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/rtc.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "pico/time.h"  // Для повторяющихся таймеров

#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"

#include "ntp_async.h"
#include "lwip/err.h"     // Для err_t
#include "lwip/ip_addr.h" // Для ip_addr_t

// ------------------- Константы -------------------
static const char *ntp_servers[MAX_NTP_SERVERS] = {
    "195.123.221.21",
    "129.6.15.28",
    "216.239.35.4"
};

#define NTP_PORT               123         ///< Порт NTP
#define NTP_MSG_LEN            48          ///< Длина NTP-сообщения
#define NTP_DELTA              2208988800UL///< Разница между эпохами 1900 и 1970 (в секундах)
#define TIMEZONE_OFFSET_HOURS  3           ///< Сдвиг часового пояса (в часах)
#define NTP_TIMEOUT_MS         5000        ///< Таймаут ответа NTP (в мс)

volatile bool g_ntp_in_progress = false;  ///< Флаг синхронизации
static struct udp_pcb *g_ntp_pcb = NULL;    ///< UDP PCB для NTP
static alarm_id_t g_ntp_timeout_alarm = 0;    ///< Идентификатор таймаута
static int ntp_server_index = 0;              ///< Текущий индекс NTP-сервера

// ------------------- Автоматический опрос NTP -------------------
static alarm_id_t g_ntp_poll_alarm = 0;       ///< Идентификатор таймера для опроса NTP-сервера

/**
 * @brief Обработчик таймера для опроса NTP-сервера.
 *
 * По истечении NTP_POLL_INTERVAL_MS вызывается эта функция, которая запускает новый опрос.
 *
 * @param id        Идентификатор будильника (не используется).
 * @param user_data Дополнительные данные (не используются).
 * @return 0, чтобы будильник не повторялся.
 */
static int64_t ntp_poll_callback(alarm_id_t id, void *user_data) {
    LOG_INFO("[NTP] ntp_poll_callback: запуск следующего опроса NTP");
    ntp_async_start();
    g_ntp_poll_alarm = 0;
    return 0;
}

// ------------------- Унифицированная LED-индикация -------------------
#if ENABLE_LED_INDICATION

// Глобальный флаг для включения/отключения LED-индикации (по умолчанию включена)
static bool g_led_indication_enabled = true;

static repeating_timer_t led_timer;           ///< Таймер для мигания LED
static led_state_t current_led_state = LED_OFF; ///< Текущее состояние LED
static uint32_t led_blink_period_ms = 0;        ///< Период мигания (в мс) для непрерывных режимов
static bool led_on_state = false;               ///< Текущее логическое состояние пина LED

/**
 * @brief Обработчик таймера для мигания LED.
 *
 * Переключает состояние LED (ON/OFF).
 *
 * @param rt Указатель на repeating_timer_t (не используется).
 * @return true, чтобы таймер продолжал работу.
 */
static bool led_timer_callback(repeating_timer_t *rt) {
    led_on_state = !led_on_state;
    gpio_put(LED_PIN, led_on_state ? 1 : 0);
    return true;
}

/**
 * @brief Устанавливает режим работы LED.
 *
 * В зависимости от режима LED либо мигает с заданным периодом, либо выполняет
 * кратковременную последовательность миганий.
 *
 * Если LED-индикация отключена, функция сразу выключает LED.
 *
 * @param state Желаемое состояние LED.
 */
void led_set_state(led_state_t state) {
#if ENABLE_LED_INDICATION
    if (!g_led_indication_enabled) {
        gpio_put(LED_PIN, 0);
        return;
    }

    cancel_repeating_timer(&led_timer);
    current_led_state = state;
    gpio_put(LED_PIN, 0);
    led_on_state = false;
    
    if (state == LED_OFF) {
        return;
    } else if (state == LED_WIFI_CONNECT) {
        led_blink_period_ms = 500;
        if (!add_repeating_timer_ms(led_blink_period_ms, led_timer_callback, NULL, &led_timer)) {
            LOG_ERROR("[LED] Не удалось добавить таймер для LED_WIFI_CONNECT");
        } else {
            LOG_INFO("[LED] LED_WIFI_CONNECT установлен (мигание 500 мс)");
        }
    } else if (state == LED_NTP_SYNC) {
        led_blink_period_ms = 100;
        if (!add_repeating_timer_ms(led_blink_period_ms, led_timer_callback, NULL, &led_timer)) {
            LOG_ERROR("[LED] Не удалось добавить таймер для LED_NTP_SYNC");
        } else {
            LOG_INFO("[LED] LED_NTP_SYNC установлен (мигание 100 мс)");
        }
    } else if (state == LED_SUCCESS) {
        for (int i = 0; i < 3; i++) {
            gpio_put(LED_PIN, 1);
            sleep_ms(100);
            gpio_put(LED_PIN, 0);
            sleep_ms(100);
        }
        LOG_INFO("[LED] LED_SUCCESS выполнен");
    } else if (state == LED_ERROR) {
        for (int i = 0; i < 3; i++) {
            gpio_put(LED_PIN, 1);
            sleep_ms(500);
            gpio_put(LED_PIN, 0);
            sleep_ms(500);
        }
        LOG_INFO("[LED] LED_ERROR выполнен");
        // Явно выключаем LED после режима ошибки:
        gpio_put(LED_PIN, 0);
    }
#endif
}

/**
 * @brief Включает или отключает LED-индикацию.
 *
 * При отключении таймеры отменяются, а LED сразу выключается.
 *
 * @param enable Если true – индикация включена, если false – отключена.
 */
void led_indication_enable(bool enable) {
#if ENABLE_LED_INDICATION
    g_led_indication_enabled = enable;
    if (!enable) {
        cancel_repeating_timer(&led_timer);
        gpio_put(LED_PIN, 0);
        LOG_INFO("[LED] LED-индикация отключена");
    } else {
        LOG_INFO("[LED] LED-индикация включена");
    }
#endif
}

#endif // ENABLE_LED_INDICATION

/**
 * @brief Инициализирует пин LED для индикации.
 */
void led_init(void) {
#if ENABLE_LED_INDICATION
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);
    LOG_INFO("[LED] Пин %d инициализирован для LED", LED_PIN);
#endif
}

// ------------------- Обработка NTP -------------------

/**
 * @brief Обработчик входящего UDP-пакета от NTP-сервера.
 *
 * Извлекает отметку времени, конвертирует его в UNIX-время, устанавливает RTC,
 * а затем показывает успешную индикацию. После завершения обработки запускается
 * таймер для следующего опроса NTP-сервера через NTP_POLL_INTERVAL_MS мс.
 *
 * @param arg   Дополнительный аргумент (не используется).
 * @param pcb   Указатель на структуру udp_pcb.
 * @param p     Полученный пакет.
 * @param addr  IP-адрес отправителя.
 * @param port  Порт отправителя.
 */
static void ntp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                              const ip_addr_t *addr, u16_t port) {
    if (!p) return;
    if (!g_ntp_in_progress) {
        pbuf_free(p);
        return;
    }

    uint8_t secbuf[4];
    pbuf_copy_partial(p, secbuf, 4, 40);
    uint32_t s1900 = (secbuf[0] << 24) | (secbuf[1] << 16) | (secbuf[2] << 8) | secbuf[3];
    uint32_t s1970 = s1900 - NTP_DELTA;
    time_t raw_time = (time_t)s1970;
    
    raw_time += TIMEZONE_OFFSET_HOURS * 3600;
    struct tm *t = gmtime(&raw_time);
    if (t) {
        datetime_t dt = {
            .year  = (int16_t)(t->tm_year + 1900),
            .month = (int8_t)(t->tm_mon + 1),
            .day   = (int8_t)t->tm_mday,
            .dotw  = (int8_t)((t->tm_wday == 0) ? 7 : t->tm_wday),
            .hour  = (int8_t)t->tm_hour,
            .min   = (int8_t)t->tm_min,
            .sec   = (int8_t)t->tm_sec
        };
        rtc_set_datetime(&dt);
        LOG_INFO("[NTP] Синхронизация прошла успешно: %04d-%02d-%02d %02d:%02d:%02d",
                 dt.year, dt.month, dt.day, dt.hour, dt.min, dt.sec);
    }

#if ENABLE_LED_INDICATION
    led_set_state(LED_SUCCESS);
#endif

    g_ntp_in_progress = false;
    ntp_server_index = 0;  // Сброс индекса сервера после успеха

    pbuf_free(p);

    // Отменяем таймаут, если он был запущен.
    if (g_ntp_timeout_alarm) {
        cancel_alarm(g_ntp_timeout_alarm);
        g_ntp_timeout_alarm = 0;
    }
    // Запускаем таймер для следующего опроса через NTP_POLL_INTERVAL_MS мс,
    // с fire_if_past = true, чтобы будильник сработал даже если время уже прошло.
    g_ntp_poll_alarm = add_alarm_in_ms(NTP_POLL_INTERVAL_MS, ntp_poll_callback, NULL, true);
}

/**
 * @brief Обработчик таймаута ожидания ответа от NTP-сервера.
 *
 * Если время ожидания превышено, выводится сообщение об ошибке, выбирается следующий сервер
 * (циклически) и, независимо от результата, следующий опрос будет запущен через NTP_POLL_INTERVAL_MS мс.
 *
 * @param id        Идентификатор будильника (не используется).
 * @param user_data Дополнительные данные (не используются).
 * @return 0, чтобы будильник не повторялся.
 */
static int64_t ntp_timeout_callback(alarm_id_t id, void *user_data) {
    LOG_ERROR("[NTP] Таймаут ответа от сервера %s", ntp_servers[ntp_server_index]);

    // Переключаемся на следующий сервер (циклически)
    ntp_server_index = (ntp_server_index + 1) % MAX_NTP_SERVERS;
    g_ntp_in_progress = false;
#if ENABLE_LED_INDICATION
    led_set_state(LED_ERROR);
#endif
    // Запускаем следующий опрос через NTP_POLL_INTERVAL_MS мс,
    // с fire_if_past = true.
    g_ntp_poll_alarm = add_alarm_in_ms(NTP_POLL_INTERVAL_MS, ntp_poll_callback, NULL, true);
    g_ntp_timeout_alarm = 0;
    return 0;
}

/**
 * @brief Отправляет NTP-запрос на указанный сервер.
 *
 * Формирует 48-байтовый запрос с первым байтом 0x1B и отправляет его через UDP.
 *
 * @param server_addr Указатель на IP-адрес NTP-сервера.
 * @return ERR_OK при успешной отправке или код ошибки lwIP.
 */
static err_t ntp_send_request(const ip_addr_t *server_addr) {
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
    if (!p) return ERR_MEM;

    memset(p->payload, 0, NTP_MSG_LEN);
    ((uint8_t*)p->payload)[0] = 0x1B;  // Режим клиента NTP

    err_t e = udp_sendto(g_ntp_pcb, p, server_addr, NTP_PORT);
    pbuf_free(p);
    return e;
}

/**
 * @brief Инициализирует Wi‑Fi, устанавливает соединение и настраивает UDP PCB для NTP.
 *
 * Перед подключением включается индикация режима подключения (LED_WIFI_CONNECT).
 * При успешном подключении индикация выключается.
 *
 * @param ssid     SSID Wi‑Fi сети.
 * @param password Пароль Wi‑Fi.
 * @return true, если подключение прошло успешно; false – в случае ошибки.
 */
bool ntp_async_init(const char *ssid, const char *password) {
    if (cyw43_arch_init()) {
        LOG_ERROR("[NTP] Ошибка инициализации Wi‑Fi");
        return false;
    }
    cyw43_arch_enable_sta_mode();

    LOG_INFO("[NTP] Подключение к Wi‑Fi: %s", ssid);
#if ENABLE_LED_INDICATION
    led_set_state(LED_WIFI_CONNECT);
#endif

    if (cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        LOG_ERROR("[NTP] Ошибка подключения к Wi‑Fi");
#if ENABLE_LED_INDICATION
        led_set_state(LED_ERROR);
#endif
        return false;
    }

    LOG_INFO("[NTP] Wi‑Fi подключён!");
#if ENABLE_LED_INDICATION
    led_set_state(LED_OFF);
#endif

    g_ntp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    udp_recv(g_ntp_pcb, ntp_recv_callback, NULL);
    return true;
}

/**
 * @brief Запускает асинхронный запрос времени через NTP.
 *
 * Если синхронизация уже идёт, повторный вызов игнорируется. При старте индицируется
 * режим синхронизации (LED_NTP_SYNC). В случае ошибки отправки запроса индикация переключается на LED_ERROR.
 */
void ntp_async_start(void) {
    if (g_ntp_in_progress) {
        LOG_DEBUG("[NTP] Синхронизация уже запущена");
        return;
    }

    ip_addr_t ntp_addr;
    if (!ip4addr_aton(ntp_servers[ntp_server_index], &ntp_addr)) {
        LOG_ERROR("[NTP] Некорректный IP: %s", ntp_servers[ntp_server_index]);
#if ENABLE_LED_INDICATION
        led_set_state(LED_ERROR);
#endif
        return;
    }

    g_ntp_in_progress = true;
#if ENABLE_LED_INDICATION
    led_set_state(LED_NTP_SYNC);
#endif

    err_t e = ntp_send_request(&ntp_addr);
    if (e != ERR_OK) {
        LOG_ERROR("[NTP] Ошибка отправки запроса: %d", e);
        g_ntp_in_progress = false;
#if ENABLE_LED_INDICATION
        led_set_state(LED_ERROR);
#endif
        return;
    }
    LOG_INFO("[NTP] Запрос отправлен на сервер %s", ntp_servers[ntp_server_index]);
    g_ntp_timeout_alarm = add_alarm_in_ms(NTP_TIMEOUT_MS, ntp_timeout_callback, NULL, false);
}

/**
 * @brief Возвращает строку с IP-адресом интерфейса STA.
 *
 * @return Строка с IP-адресом, например, "192.168.1.100".
 */
const char* wifi_get_ip_str(void) {
    static char ip_str[16] = "0.0.0.0";
    struct netif *nf = &cyw43_state.netif[CYW43_ITF_STA];

    if (nf) {
        ip4addr_ntoa_r(netif_ip4_addr(nf), ip_str, sizeof(ip_str));
    }
    return ip_str;
}
