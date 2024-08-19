#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_openthread.h"
#include "esp_openthread_cli.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_types.h"
#include <openthread/thread.h>
#include <openthread/ip6.h>
#include <openthread/thread_ftd.h>
#include "nvs_flash.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/uart_types.h"
#include "openthread/error.h"
#include "openthread/logging.h"
#include "project_config.h"
#include "esp_vfs.h"
#include "esp_vfs_eventfd.h"

#include <openthread-core-config.h>
#include <openthread/config.h>
#include <openthread/cli.h>
#include <openthread/diag.h>
#include <openthread/tasklet.h>
#include <openthread/instance.h>
#include <openthread/message.h>
#include <openthread/udp.h>
#include <openthread/platform/logging.h>
#include <openthread/platform/misc.h>
#include <openthread/dataset_ftd.h>
#include "esp_sleep.h"
#include "freertos/event_groups.h"
#include "lora.h"
#include <cJSON.h>
#include <time.h>

static int hex_digit_to_int(char hex)
{
    if ('A' <= hex && hex <= 'F')
    {
        return 10 + hex - 'A';
    }
    if ('a' <= hex && hex <= 'f')
    {
        return 10 + hex - 'a';
    }
    if ('0' <= hex && hex <= '9')
    {
        return hex - '0';
    }
    return -1;
}

static size_t hex_string_to_binary(const char *hex_string, uint8_t *buf, size_t buf_size)
{
    int num_char = strlen(hex_string);

    if (num_char != buf_size * 2)
    {
        return 0;
    }
    for (size_t i = 0; i < num_char; i += 2)
    {
        int digit0 = hex_digit_to_int(hex_string[i]);
        int digit1 = hex_digit_to_int(hex_string[i + 1]);

        if (digit0 < 0 || digit1 < 0)
        {
            return 0;
        }
        buf[i / 2] = (digit0 << 4) + digit1;
    }

    return buf_size;
}

void handleNetifStateChanged(uint32_t aFlags, void *aContext)
{
    if ((aFlags & OT_CHANGED_THREAD_ROLE) != 0)
    {
        otDeviceRole changedRole = otThreadGetDeviceRole(aContext);
        printf("Role changed to %d\n", changedRole);
    }
}

void getCurrentDateTime(char *buffer, size_t bufferSize)
{
    time_t now = time(NULL);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    strftime(buffer, bufferSize, "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
}
