#include <stdio.h>
#include <time.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_sleep.h"

#include "lora.h"
#include "thread_leader.h"

#define NETWORK_KEY "00112233445566778899aabbccddeeff"
#define LEADER_NAME "LeaderDevice"
#define ACTIVE_WINDOW_SECONDS (300) // w segundos
#define MEASUREMENTS_COUNT 2
#define MEASUREMENT_PERIOD 120

static const char *TAG = "ot_leader_device";
static time_t last_sync_time = 0;

void setNextSleepTime()
{

    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    int minutes_to_next_hour = 60 - timeinfo.tm_min;

    int minutes_to_wake = (minutes_to_next_hour + 50) % 60;
    if (minutes_to_wake == 0)
    {
        minutes_to_wake = 60;
    }

    int seconds_to_wake = minutes_to_wake * 60 - timeinfo.tm_sec;
    int64_t sleep_time = seconds_to_wake * 1000000LL;

    ESP_LOGI(TAG, "Durmiendo por %lld segundos hasta la próxima ventana", sleep_time / 1000000);
    esp_sleep_enable_timer_wakeup(sleep_time);
    esp_deep_sleep_start();
}

esp_err_t app_main(int argc, char *argv[])
{
    ESP_LOGI(TAG, "Iniciando el dispositivo");
    handleWakeUp(ACTIVE_WINDOW_SECONDS, MEASUREMENTS_COUNT, MEASUREMENT_PERIOD);
    setNextSleepTime();
    return ESP_OK;
}
