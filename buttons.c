#include "buttons.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

#define DEBOUNCE_PERIOD_MS      10
#define DEBOUNCE_THRESHOLD_MS   30
#define MAX_EVENTS              8

/**
 * Структура состояния одной кнопки.
 * Добавлено поле prev_stable_state для хранения предыдущего устойчивого состояния.
 */
typedef struct {
    uint32_t pin_gpio;
    uint32_t long_press_ms;

    bool stable_state;      // Текущее устойчивое состояние (нажата или отпущена)
    uint32_t stable_time;   // Сколько мс прошло с момента появления нового «сыро
                             // го» состояния (для антидребезга)

    bool prev_state_raw;    // Предыдущее «сырое» (необработанное) состояние
    uint32_t press_start_ms;// Время начала нажатия (для определения длительности)
    bool event_generated;   // Флаг, что событие (long press) уже сгенерировано

    bool prev_stable_state; // Сохранённое устойчивое состояние из предыдущего цикла
} button_state_t;

static button_state_t g_buttons[BUTTON_ID_COUNT];
static button_event_t g_events_queue[MAX_EVENTS];
static int g_queue_head = 0;
static int g_queue_tail = 0;

/**
 * Добавляет событие в кольцевую очередь.
 * При переполнении выводится сообщение, событие пропускается.
 */
static void push_event(button_event_t evt) {
    if (evt == BUTTON_EVENT_NONE) return;
    int next_tail = (g_queue_tail + 1) % MAX_EVENTS;
    if (next_tail == g_queue_head) {
        // Очередь переполнена – событие не будет добавлено
        printf("[buttons] Event queue overflow, skip=%d\n", evt);
        return;
    }
    g_events_queue[g_queue_tail] = evt;
    g_queue_tail = next_tail;
}

void buttons_init(const button_config_t *configs) {
    memset(g_buttons, 0, sizeof(g_buttons));
    for (int i = 0; i < BUTTON_ID_COUNT; i++) {
        g_buttons[i].pin_gpio      = configs[i].pin_gpio;
        g_buttons[i].long_press_ms = configs[i].long_press_ms;

        gpio_init(g_buttons[i].pin_gpio);
        gpio_set_dir(g_buttons[i].pin_gpio, GPIO_IN);
        // Предполагаем, что кнопка замыкается на GND, поэтому включаем pull-up
        gpio_pull_up(g_buttons[i].pin_gpio);

        // Инициализируем состояния – считаем, что кнопка отпущена
        g_buttons[i].stable_state     = false;
        g_buttons[i].prev_state_raw   = false;
        g_buttons[i].prev_stable_state = false;
        g_buttons[i].stable_time      = 0;
        g_buttons[i].press_start_ms   = 0;
        g_buttons[i].event_generated  = false;
    }
    memset(g_events_queue, 0, sizeof(g_events_queue));
    g_queue_head = 0;
    g_queue_tail = 0;
}

void buttons_update(void) {
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());

    // 1) Считываем сырые состояния кнопок (при pull-up: 0 = нажато, 1 = отпущено)
    bool raw_state[BUTTON_ID_COUNT];
    for (int i = 0; i < BUTTON_ID_COUNT; i++) {
        raw_state[i] = (gpio_get(g_buttons[i].pin_gpio) == 0);
    }

    // 2) Антидребезг: обновляем устойчивое состояние для каждой кнопки
    for (int i = 0; i < BUTTON_ID_COUNT; i++) {
        bool new_raw = raw_state[i];
        if (new_raw == g_buttons[i].prev_state_raw) {
            g_buttons[i].stable_time += DEBOUNCE_PERIOD_MS;
            if (g_buttons[i].stable_time >= DEBOUNCE_THRESHOLD_MS) {
                g_buttons[i].stable_state = new_raw;
            }
        } else {
            g_buttons[i].prev_state_raw = new_raw;
            g_buttons[i].stable_time = 0;
        }
    }

    // 3) Генерация событий на основе переходов состояний
    for (int i = 0; i < BUTTON_ID_COUNT; i++) {
        bool prev_stable = g_buttons[i].prev_stable_state;
        bool cur_stable  = g_buttons[i].stable_state;

        // Если произошёл переход с отпущенной кнопки на нажатую:
        if (!prev_stable && cur_stable) {
            g_buttons[i].press_start_ms = now_ms;
            g_buttons[i].event_generated = false;
        }
        // Если произошёл переход с нажатой кнопки на отпущенную:
        if (prev_stable && !cur_stable) {
            if (!g_buttons[i].event_generated) {
                switch (i) {
                    case 0: push_event(BUTTON_EVENT_SHORT_PRESS_1); break;
                    case 1: push_event(BUTTON_EVENT_SHORT_PRESS_2); break;
                    case 2: push_event(BUTTON_EVENT_SHORT_PRESS_3); break;
                }
            }
            g_buttons[i].event_generated = false;
        }
        // Если кнопка удерживается, проверяем длительность удержания:
        if (cur_stable) {
            uint32_t held_time = now_ms - g_buttons[i].press_start_ms;
            if (!g_buttons[i].event_generated && (held_time >= g_buttons[i].long_press_ms)) {
                switch (i) {
                    case 0: push_event(BUTTON_EVENT_LONG_PRESS_1); break;
                    case 1: push_event(BUTTON_EVENT_LONG_PRESS_2); break;
                    case 2: push_event(BUTTON_EVENT_LONG_PRESS_3); break;
                }
                g_buttons[i].event_generated = true;
            }
        }
        // Сохраняем текущее устойчивое состояние для следующего цикла
        g_buttons[i].prev_stable_state = cur_stable;
    }
}

button_event_t buttons_get_event(void) {
    if (g_queue_head == g_queue_tail) {
        return BUTTON_EVENT_NONE;
    }
    button_event_t evt = g_events_queue[g_queue_head];
    g_queue_head = (g_queue_head + 1) % MAX_EVENTS;
    return evt;
}
