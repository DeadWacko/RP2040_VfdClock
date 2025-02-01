#include "buttons.h"
#include <stdio.h>
#include <string.h>

#define DEBOUNCE_TIME_MS 30
#define CHECK_PERIOD_MS 10
#define MAX_EVENTS 8

typedef struct {
    uint32_t pin_gpio;
    uint32_t long_press_ms;
    bool stable_state;
    absolute_time_t last_change_time;
    absolute_time_t press_start_time;
    bool was_held_on_boot;
} button_state_t;

static button_state_t g_buttons[BUTTON_ID_COUNT];
static button_event_t g_events_queue[MAX_EVENTS];
static int g_queue_head = 0;
static int g_queue_tail = 0;
static bool has_pending_boot_event = false;  // Флаг наличия событий при старте

// Функция добавления события в очередь
static void push_event(button_event_t evt) {
    if (evt == BUTTON_EVENT_NONE) return;

    int next_tail = (g_queue_tail + 1) % MAX_EVENTS;
    if (next_tail == g_queue_head) {
        LOG_ERROR("Queue overflow! Skipping event: %s", button_event_to_string(evt));
        return;
    }

    g_events_queue[g_queue_tail] = evt;
    g_queue_tail = next_tail;
    
    LOG_DEBUG("Event added to queue: %s", button_event_to_string(evt));
}

// Инициализация кнопок
void buttons_init(const button_config_t *configs) {
    memset(g_buttons, 0, sizeof(g_buttons));
    sleep_ms(50); // Ждем стабилизации GPIO

    has_pending_boot_event = false;  // Обнуляем флаг

    for (int i = 0; i < BUTTON_ID_COUNT; i++) {
        g_buttons[i].pin_gpio = configs[i].pin_gpio;
        g_buttons[i].long_press_ms = configs[i].long_press_ms;

        gpio_init(g_buttons[i].pin_gpio);
        gpio_set_dir(g_buttons[i].pin_gpio, GPIO_IN);
        gpio_pull_up(g_buttons[i].pin_gpio);

        g_buttons[i].last_change_time = get_absolute_time();

        bool is_held = (gpio_get(g_buttons[i].pin_gpio) == 0);
        sleep_ms(10);
        is_held &= (gpio_get(g_buttons[i].pin_gpio) == 0);
        sleep_ms(10);
        is_held &= (gpio_get(g_buttons[i].pin_gpio) == 0);

        LOG_INFO("Button %d state on boot: %d", i, is_held);

        if (is_held) {
            g_buttons[i].was_held_on_boot = true;
            button_event_t boot_event = (button_event_t)(BUTTON_EVENT_HOLD_ON_BOOT_1 + i);
            push_event(boot_event);
            has_pending_boot_event = true;  // Устанавливаем флаг
        }

        g_buttons[i].stable_state = is_held;
    }
}

// Обновление кнопок
void buttons_update(void) {
    absolute_time_t now = get_absolute_time();

    for (int i = 0; i < BUTTON_ID_COUNT; i++) {
        bool raw_state = gpio_get(g_buttons[i].pin_gpio) == 0;

        if (absolute_time_diff_us(g_buttons[i].last_change_time, now) / 1000 < DEBOUNCE_TIME_MS) {
            continue;
        }

        if (raw_state != g_buttons[i].stable_state) {
            g_buttons[i].last_change_time = now;
            g_buttons[i].stable_state = raw_state;

            if (raw_state) {
                g_buttons[i].press_start_time = now;
            } else {
                uint64_t press_duration = absolute_time_diff_us(g_buttons[i].press_start_time, now) / 1000;

                if (g_buttons[i].was_held_on_boot) {
                    g_buttons[i].was_held_on_boot = false;
                } else {
                    button_event_t evt = (press_duration >= g_buttons[i].long_press_ms) ?
                        (button_event_t)(BUTTON_EVENT_LONG_PRESS_1 + i) :
                        (button_event_t)(BUTTON_EVENT_SHORT_PRESS_1 + i);
                    push_event(evt);
                }
            }
        }
    }
}

// Проверка наличия событий "кнопка зажата при старте"
bool buttons_has_boot_event(void) {
    return has_pending_boot_event;
}

// Получение события из очереди
button_event_t buttons_get_event(void) {
    if (g_queue_head == g_queue_tail) {
        return BUTTON_EVENT_NONE;
    }

    button_event_t evt = g_events_queue[g_queue_head];
    g_queue_head = (g_queue_head + 1) % MAX_EVENTS;
    return evt;
}

// Преобразование события в строку
const char* button_event_to_string(button_event_t event) {
    static const char* event_names[] = {
        "NONE", "SHORT_PRESS_1", "SHORT_PRESS_2", "SHORT_PRESS_3",
        "LONG_PRESS_1", "LONG_PRESS_2", "LONG_PRESS_3",
        "HOLD_ON_BOOT_1", "HOLD_ON_BOOT_2", "HOLD_ON_BOOT_3"
    };

    return (event < sizeof(event_names) / sizeof(event_names[0])) ? event_names[event] : "UNKNOWN_EVENT";
}
