#include "buttons.h"
#include <stdio.h>
#include <string.h>



// Определение структуры состояния кнопки
typedef struct {
    uint32_t pin;
    uint32_t long_press_ms;
    uint8_t raw_state;
    uint8_t state;
    uint8_t isDebouncing;
    uint32_t last_state_change;
    uint8_t long_press_reported;
} button_t;

// Конфигурация кнопок
static button_t buttons[BUTTON_ID_COUNT] = {0};

// Очередь событий кнопок
static button_event_t button_event_queue[BUTTON_ID_COUNT] = {BUTTON_EVENT_NONE};

// Инициализация кнопок
void buttons_init(const button_config_t *configs) {
    for (int i = 0; i < BUTTON_ID_COUNT; i++) {
        buttons[i].pin = configs[i].pin_gpio;
        buttons[i].long_press_ms = configs[i].long_press_ms;
        buttons[i].raw_state = 0;
        buttons[i].state = 0;
        buttons[i].isDebouncing = 0;
        buttons[i].last_state_change = to_ms_since_boot(get_absolute_time());
        buttons[i].long_press_reported = 0;
        gpio_init(buttons[i].pin);
        gpio_set_dir(buttons[i].pin, GPIO_IN);
        gpio_pull_up(buttons[i].pin); // Включаем подтяжку
    }
    LOG_INFO("Buttons initialized.");
}

// Обновление состояний кнопок
void buttons_update(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    for (int i = 0; i < BUTTON_ID_COUNT; i++) {
        uint8_t currentState = !gpio_get(buttons[i].pin); // Инверсия, т.к. pull-up
        // Детекция изменения состояния
        if (currentState != buttons[i].raw_state) {
            buttons[i].raw_state = currentState;
            buttons[i].last_state_change = now;
            buttons[i].isDebouncing = !buttons[i].isDebouncing;
        }
        // Дебаунс
        if (buttons[i].isDebouncing && (now - buttons[i].last_state_change > 50)) {
            if (!currentState) {
                if(buttons[i].state == 1){
                    button_event_queue[i] = (button_event_t)(BUTTON_EVENT_SHORT_PRESS_1 + i);
                    LOG_INFO("Button %d pressed", i + 1);
                }
            }
            buttons[i].state = currentState;
            buttons[i].isDebouncing = 0;
        }
        // Долгое нажатие
        if (buttons[i].state == 1 && (now - buttons[i].last_state_change > buttons[i].long_press_ms)) {
            buttons[i].state = 2;
            button_event_queue[i] = (button_event_t)(BUTTON_EVENT_LONG_PRESS_1 + i);
            LOG_INFO("Button %d long press detected", i + 1);
        }
    }
}

// Получение события кнопки
button_event_t buttons_get_event(void) {
    for (int i = 0; i < BUTTON_ID_COUNT; i++) {
        if (button_event_queue[i] != BUTTON_EVENT_NONE) {
            button_event_t event = button_event_queue[i];
            button_event_queue[i] = BUTTON_EVENT_NONE;
            return event;
        }
    }
    return BUTTON_EVENT_NONE;
}

// Преобразование события в строку
const char* button_event_to_string(button_event_t event) {
    switch (event) {
        case BUTTON_EVENT_SHORT_PRESS_1: return "Short Press 1";
        case BUTTON_EVENT_SHORT_PRESS_2: return "Short Press 2";
        case BUTTON_EVENT_SHORT_PRESS_3: return "Short Press 3";
        case BUTTON_EVENT_LONG_PRESS_1: return "Long Press 1";
        case BUTTON_EVENT_LONG_PRESS_2: return "Long Press 2";
        case BUTTON_EVENT_LONG_PRESS_3: return "Long Press 3";
        case BUTTON_EVENT_HOLD_ON_BOOT_1: return "Hold on Boot 1";
        case BUTTON_EVENT_HOLD_ON_BOOT_2: return "Hold on Boot 2";
        case BUTTON_EVENT_HOLD_ON_BOOT_3: return "Hold on Boot 3";
        default: return "No Event";
    }
}

// Проверка на удержание при старте
bool buttons_has_boot_event(void) {
    
    bool flag = false;

    for (int i = 0; i < BUTTON_ID_COUNT; i++) {
        if (!gpio_get(buttons[i].pin)) { // Кнопка нажата на старте
            flag = true;
            break;

        }
    }
    bool flag_1 = false;
    do{
        flag_1 = false;
        for (int i = 0; i < BUTTON_ID_COUNT; i++) {
            if (!gpio_get(buttons[i].pin)) { // Кнопка нажата на старте
                flag_1 = true;
                break;
                
            }
        }
    }while(flag_1);
    return flag;
}

