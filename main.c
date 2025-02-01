#include <stdio.h>
#include "pico/stdlib.h"

#include "buttons.h"

#define BUTTON_GPIO_1 16
#define BUTTON_GPIO_2 17
#define BUTTON_GPIO_3 18

int main() {
    stdio_init_all();
    sleep_ms(2000);

    button_config_t button_configs[BUTTON_ID_COUNT] = {
        {BUTTON_GPIO_1, 1000},
        {BUTTON_GPIO_2, 1500},
        {BUTTON_GPIO_3, 2000}
    };

    buttons_init(button_configs);
    printf("Button system initialized.\n");

    //  Проверяем, были ли кнопки зажаты при старте
    if (buttons_has_boot_event()) {
        button_event_t event;
        while ((event = buttons_get_event()) != BUTTON_EVENT_NONE) {
            if (event >= BUTTON_EVENT_HOLD_ON_BOOT_1 && event <= BUTTON_EVENT_HOLD_ON_BOOT_3) {
                printf("⚠ Button was HELD during startup: %s ⚠\n", button_event_to_string(event));
            }
        }
    }

    while (1) {
        buttons_update();

        button_event_t event;
        while ((event = buttons_get_event()) != BUTTON_EVENT_NONE) {
            printf("Button Event: %s\n", button_event_to_string(event));
        }

        sleep_ms(10);
    }

    return 0;
}
