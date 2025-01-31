#include <stdio.h>
#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/rtc.h"
#include "hardware/timer.h"

#include "lwip/pbuf.h"
#include "lwip/udp.h"

#include "ntp_async.h"

#define NTP_SERVER_IP          "195.123.221.21" 
#define NTP_PORT               123
#define NTP_MSG_LEN            48
#define NTP_DELTA              2208988800UL
#define TIMEZONE_OFFSET_HOURS  3
#define NTP_TIMEOUT_MS         5000

volatile bool g_ntp_in_progress = false;

static struct udp_pcb *g_ntp_pcb = NULL;
static alarm_id_t g_ntp_timeout_alarm = 0;

// Колбэк таймаута
static int64_t ntp_timeout_callback(alarm_id_t id, void *user_data) {
    printf("[NTP] Timeout!\n");
    g_ntp_in_progress = false;
    g_ntp_timeout_alarm = 0;
    return 0;
}

static void ntp_recv_callback(void *arg,
                              struct udp_pcb *pcb,
                              struct pbuf *p,
                              const ip_addr_t *addr,
                              u16_t port)
{
    if (!p) return;
    if (!g_ntp_in_progress) {
        // уже завершили
        pbuf_free(p);
        return;
    }
    if (port == NTP_PORT && p->tot_len == NTP_MSG_LEN) {
        uint8_t mode = pbuf_get_at(p, 0) & 0x07;
        if (mode == 4) {
            uint8_t secbuf[4];
            pbuf_copy_partial(p, secbuf, 4, 40);
            uint32_t s1900 = (secbuf[0] << 24) | (secbuf[1] << 16)
                           | (secbuf[2] << 8) | secbuf[3];
            uint32_t s1970 = s1900 - NTP_DELTA;
            time_t raw_time = (time_t)s1970;

            raw_time += TIMEZONE_OFFSET_HOURS * 3600;
            struct tm *t = gmtime(&raw_time);
            if (t) {
                datetime_t dt = {
                    .year  = (int16_t)(t->tm_year + 1900),
                    .month = (int8_t)(t->tm_mon + 1),
                    .day   = (int8_t)t->tm_mday,
                    .dotw  = (int8_t)((t->tm_wday == 0)?7:t->tm_wday),
                    .hour  = (int8_t)t->tm_hour,
                    .min   = (int8_t)t->tm_min,
                    .sec   = (int8_t)t->tm_sec
                };
                rtc_set_datetime(&dt);
                printf("[NTP] Sync OK: %04d-%02d-%02d %02d:%02d:%02d\n",
                       dt.year, dt.month, dt.day,
                       dt.hour, dt.min, dt.sec);
            }
        }
    }

    if (g_ntp_timeout_alarm > 0) {
        cancel_alarm(g_ntp_timeout_alarm);
        g_ntp_timeout_alarm = 0;
    }

    g_ntp_in_progress = false;
    pbuf_free(p);
}

static err_t ntp_send_request(const ip_addr_t *server_addr)
{
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
    if (!p) {
        return ERR_MEM;
    }
    memset(p->payload, 0, NTP_MSG_LEN);
    // LI=0,VN=3,Mode=3 => 0x1B
    ((uint8_t*)p->payload)[0] = 0x1B;
    err_t e = udp_sendto(g_ntp_pcb, p, server_addr, NTP_PORT);
    pbuf_free(p);
    return e;
}

// ------------------
// Публичные функции
// ------------------
bool ntp_async_init(const char *ssid, const char *password)
{
    if (cyw43_arch_init()) {
        printf("[NTP] Wi-Fi init fail\n");
        return false;
    }
    cyw43_arch_enable_sta_mode();
    printf("[NTP] Connecting to Wi-Fi: %s\n", ssid);
    if (cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("[NTP] Connect fail\n");
        return false;
    }
    printf("[NTP] Wi-Fi connected!\n");

    g_ntp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (!g_ntp_pcb) {
        printf("[NTP] udp_new_ip_type fail\n");
        return false;
    }
    udp_recv(g_ntp_pcb, ntp_recv_callback, NULL);
    return true;
}

void ntp_async_start(void)
{
    if (g_ntp_in_progress) {
        printf("[NTP] Already in progress\n");
        return;
    }
    ip_addr_t ntp_addr;
    if (!ip4addr_aton(NTP_SERVER_IP, &ntp_addr)) {
        printf("[NTP] Bad IP\n");
        return;
    }
    g_ntp_in_progress = true;

    err_t e = ntp_send_request(&ntp_addr);
    if (e != ERR_OK) {
        printf("[NTP] Send error=%d\n", e);
        g_ntp_in_progress = false;
        return;
    }
    printf("[NTP] Request sent...\n");
    g_ntp_timeout_alarm = add_alarm_in_ms(NTP_TIMEOUT_MS, ntp_timeout_callback, NULL, false);
}