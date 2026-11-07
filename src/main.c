#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>

#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ---------- Піни ----------

#define RED_PIN     GPIO_NUM_4
#define GREEN_PIN   GPIO_NUM_5
#define BLUE_PIN    GPIO_NUM_6
#define BUTTON_PIN  GPIO_NUM_7

// ---------- Інтервали, мс ----------

#define RED_INTERVAL       200
#define GREEN_INTERVAL     500
#define BLUE_INTERVAL      1000
#define DEBOUNCE_INTERVAL  50

// ---------- Режими ----------

typedef enum {
    MODE_ALL,
    MODE_RED,
    MODE_GREEN,
    MODE_BLUE
} Mode;

// Спільний режим: кнопка змінює, задачі LED читають.
// _Atomic забезпечує безпечний доступ із різних задач.
static _Atomic Mode currentMode = MODE_ALL;

// ---------- Читання режиму ----------

Mode getMode(void) {
    return atomic_load(&currentMode);
}

// ---------- Перемикання режиму ----------

void nextMode(void) {
    Mode mode = getMode();

    switch (mode) {
        case MODE_ALL:
            mode = MODE_RED;
            break;

        case MODE_RED:
            mode = MODE_GREEN;
            break;

        case MODE_GREEN:
            mode = MODE_BLUE;
            break;

        case MODE_BLUE:
            mode = MODE_ALL;
            break;
    }

    atomic_store(&currentMode, mode);

    switch (mode) {
        case MODE_ALL:
            printf("Mode: All\n");
            break;

        case MODE_RED:
            printf("Mode: Red\n");
            break;

        case MODE_GREEN:
            printf("Mode: Green\n");
            break;

        case MODE_BLUE:
            printf("Mode: Blue\n");
            break;
    }
}

// ---------- Задача червоного LED ----------

void redTask(void *parameter) {
    bool state = false;
    int64_t previousMillis = esp_timer_get_time() / 1000;

    while (true) {
        int64_t now = esp_timer_get_time() / 1000;
        Mode mode = getMode();

        if (mode == MODE_ALL || mode == MODE_RED) {
            if (now - previousMillis >= RED_INTERVAL) {
                previousMillis = now;
                state = !state;
                gpio_set_level(RED_PIN, state);
            }
        } else {
            state = false;
            gpio_set_level(RED_PIN, 0);
            previousMillis = now;
        }

        vTaskDelay(1);
    }
}

// ---------- Задача зеленого LED ----------

void greenTask(void *parameter) {
    bool state = false;
    int64_t previousMillis = esp_timer_get_time() / 1000;

    while (true) {
        int64_t now = esp_timer_get_time() / 1000;
        Mode mode = getMode();

        if (mode == MODE_ALL || mode == MODE_GREEN) {
            if (now - previousMillis >= GREEN_INTERVAL) {
                previousMillis = now;
                state = !state;
                gpio_set_level(GREEN_PIN, state);
            }
        } else {
            state = false;
            gpio_set_level(GREEN_PIN, 0);
            previousMillis = now;
        }

        vTaskDelay(1);
    }
}

// ---------- Задача синього LED ----------

void blueTask(void *parameter) {
    bool state = false;
    int64_t previousMillis = esp_timer_get_time() / 1000;

    while (true) {
        int64_t now = esp_timer_get_time() / 1000;
        Mode mode = getMode();

        if (mode == MODE_ALL || mode == MODE_BLUE) {
            if (now - previousMillis >= BLUE_INTERVAL) {
                previousMillis = now;
                state = !state;
                gpio_set_level(BLUE_PIN, state);
            }
        } else {
            state = false;
            gpio_set_level(BLUE_PIN, 0);
            previousMillis = now;
        }

        vTaskDelay(1);
    }
}

// ---------- Задача кнопки ----------

void buttonTask(void *parameter) {
    int previousReading = 1;
    int stableButtonState = 1;
    int64_t lastButtonChange = 0;

    while (true) {
        int64_t now = esp_timer_get_time() / 1000;
        int reading = gpio_get_level(BUTTON_PIN);

        // Кожна зміна сигналу починає відлік заново
        if (reading != previousReading) {
            lastButtonChange = now;
            previousReading = reading;
        }

        // Приймаємо стан після 50 мс без змін
        if (now - lastButtonChange >= DEBOUNCE_INTERVAL) {
            if (reading != stableButtonState) {
                stableButtonState = reading;

                if (stableButtonState == 0) {
                    printf("Button: pressed\n");
                    nextMode();
                } else {
                    printf("Button: released\n");
                }
            }
        }

        vTaskDelay(1);
    }
}

// ---------- Початок програми ----------

void app_main(void) {
    gpio_reset_pin(RED_PIN);
    gpio_reset_pin(GREEN_PIN);
    gpio_reset_pin(BLUE_PIN);
    gpio_reset_pin(BUTTON_PIN);

    gpio_set_level(RED_PIN, 0);
    gpio_set_level(GREEN_PIN, 0);
    gpio_set_level(BLUE_PIN, 0);

    gpio_set_direction(RED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(GREEN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(BLUE_PIN, GPIO_MODE_OUTPUT);

    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY);

    printf("ESP32-S3 started\n");
    printf("Mode: All\n");

    // Створюємо задачі та перевіряємо успішність
    BaseType_t result;

    result = xTaskCreate(redTask, "Red", 2048, NULL, 1, NULL);
    if (result != pdPASS) {
        printf("Cannot create Red task\n");
        abort();
    }

    result = xTaskCreate(greenTask, "Green", 2048, NULL, 1, NULL);
    if (result != pdPASS) {
        printf("Cannot create Green task\n");
        abort();
    }

    result = xTaskCreate(blueTask, "Blue", 2048, NULL, 1, NULL);
    if (result != pdPASS) {
        printf("Cannot create Blue task\n");
        abort();
    }

    result = xTaskCreate(buttonTask, "Button", 4096, NULL, 1, NULL);
    if (result != pdPASS) {
        printf("Cannot create Button task\n");
        abort();
    }
}