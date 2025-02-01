#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Идентификаторы кнопок.
 */
typedef enum {
    BUTTON_ID_1 = 0,
    BUTTON_ID_2,
    BUTTON_ID_3,
    BUTTON_ID_COUNT
} button_id_t;

/**
 * События, которые могут возникать от кнопок.
 */
typedef enum {
    BUTTON_EVENT_NONE = 0,
    // Индивидуальные нажатия
    BUTTON_EVENT_SHORT_PRESS_1,
    BUTTON_EVENT_SHORT_PRESS_2,
    BUTTON_EVENT_SHORT_PRESS_3,
    BUTTON_EVENT_LONG_PRESS_1,
    BUTTON_EVENT_LONG_PRESS_2,
    BUTTON_EVENT_LONG_PRESS_3
} button_event_t;

/**
 * Конфигурация для одной кнопки:
 * - pin_gpio      : номер GPIO (например, 16)
 * - long_press_ms : время (в мс), свыше которого нажатие считается «долгим»
 */
typedef struct {
    uint32_t pin_gpio;
    uint32_t long_press_ms;
} button_config_t;

/**
 * Инициализация кнопок согласно переданному массиву конфигураций.
 */
void buttons_init(const button_config_t *configs);

/**
 * Функция, которую следует вызывать периодически (например, каждые 10 мс) для
 * обновления состояний кнопок и формирования событий.
 */
void buttons_update(void);

/**
 * Получить следующее событие кнопки из очереди.
 * Если событий нет, возвращается BUTTON_EVENT_NONE.
 */
button_event_t buttons_get_event(void);

#endif // BUTTONS_H
