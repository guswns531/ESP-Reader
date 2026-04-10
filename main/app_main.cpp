#include "app_controller.h"

#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void)
{
    static AppController app;
    ESP_ERROR_CHECK(app.init());

    while (true) {
        app.tick();
        vTaskDelay(pdMS_TO_TICKS(80));
    }
}
