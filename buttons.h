#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"

// Уровни логирования
#define LOG_LEVEL_OFF   -1  // Полное отключение логов
#define LOG_LEVEL_ERROR  2  // Только ошибки
#define LOG_LEVEL_INFO   1  // Информация и ошибки
#define LOG_LEVEL_DEBUG  0  // Отладка, информация и ошибки

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_OFF  // Уровень логирования по умолчанию
#endif

// Макросы логирования
#if LOG_LEVEL > LOG_LEVEL_OFF
    #define LOG_ERROR(fmt, ...) if (LOG_LEVEL <= LOG_LEVEL_ERROR) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
    #define LOG_INFO(fmt, ...)  if (LOG_LEVEL <= LOG_LEVEL_INFO) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
    #define LOG_DEBUG(fmt, ...) if (LOG_LEVEL <= LOG_LEVEL_DEBUG) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_ERROR(fmt, ...) 
    #define LOG_INFO(fmt, ...)  
    #define LOG_DEBUG(fmt, ...) 
#endif

// Идентификаторы кнопок
typedef enum {
    BUTTON_ID_1 = 0,
    BUTTON_ID_2,
    BUTTON_ID_3,
    BUTTON_ID_COUNT
} button_id_t;

// События кнопок
typedef enum {
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_SHORT_PRESS_1,
    BUTTON_EVENT_SHORT_PRESS_2,
    BUTTON_EVENT_SHORT_PRESS_3,
    BUTTON_EVENT_LONG_PRESS_1,
    BUTTON_EVENT_LONG_PRESS_2,
    BUTTON_EVENT_LONG_PRESS_3,
    BUTTON_EVENT_HOLD_ON_BOOT_1,
    BUTTON_EVENT_HOLD_ON_BOOT_2,
    BUTTON_EVENT_HOLD_ON_BOOT_3
} button_event_t;

// Конфигурация кнопки
typedef struct {
    uint32_t pin_gpio;
    uint32_t long_press_ms;
} button_config_t;

// Функции API
void buttons_init(const button_config_t *configs);
void buttons_update(void);
button_event_t buttons_get_event(void);
const char* button_event_to_string(button_event_t event);



bool buttons_has_boot_event(void);  // Функция проверки событий при старте

#endif // BUTTONS_H
