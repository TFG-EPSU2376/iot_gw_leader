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
#include "thread_utils.c"

#define UDP_PORT 40001
#define SLEEP_BIT (1 << 0)
static EventGroupHandle_t s_event_group;

static const char *TAG = "ot_leader_device";
static otInstance *sInstance = NULL;
static int measurements_count = 0;
static int measurement_period = 0;

#define UDP_DEST_ADDR "ff03::1"
static otUdpSocket sUdpSocket;

void lora_receive_mode(bool enable)
{
    if (enable)
    {
        lora_receive();
        ESP_LOGI(pcTaskGetName(NULL), "LoRa receiver enabled.");
    }
    else
    {
        lora_idle();
        ESP_LOGI(pcTaskGetName(NULL), "LoRa receiver disabled.");
    }
}

void sendUdpMessage(const void *aContext, const otMessage *message, const otMessageInfo *aMessageInfo)
{
    otError error = OT_ERROR_NONE;

    char ipString[OT_IP6_ADDRESS_STRING_SIZE];
    otIp6AddressToString(&aMessageInfo->mPeerAddr, ipString, sizeof(ipString));
    ESP_LOGI(TAG, "From IP: %s", ipString);

    otMessageInfo newMessageInfo;
    otInstance *aInstance = esp_openthread_get_instance();
    otIp6Address destinationAddr;

    otDeviceRole deviceRole = otThreadGetDeviceRole(aInstance);
    if (deviceRole == OT_DEVICE_ROLE_DISABLED || deviceRole == OT_DEVICE_ROLE_DETACHED)
    {
        ESP_LOGE(TAG, "Device is not connected to the Thread network");
    }

    memset(&newMessageInfo, 0, sizeof(newMessageInfo));

    error = otIp6AddressFromString(ipString, &destinationAddr);
    if (error != OT_ERROR_NONE)
    {
        ESP_LOGE(TAG, "Failed to parse destination address: %s", otThreadErrorToString(error));
    }
    newMessageInfo.mPeerAddr = aMessageInfo->mPeerAddr;
    newMessageInfo.mPeerPort = aMessageInfo->mPeerPort;

    otMessage *replyMessage = otUdpNewMessage(aInstance, NULL);
    if (message == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate message");
    }
    char *newMessageData = message;
    error = otMessageAppend(replyMessage, newMessageData, strlen(replyMessage));
    if (error != OT_ERROR_NONE)
    {
        ESP_LOGE(TAG, "Failed to append message: %s", otThreadErrorToString(error));
    }

    error = otUdpSend(aInstance, &sUdpSocket, replyMessage, &newMessageInfo);
    if (error != OT_ERROR_NONE)
    {
        ESP_LOGE(TAG, "Failed to send message: %s", otThreadErrorToString(error));
    }
}

void setUpLoraNetwork()
{
    if (lora_init() == 0)
    {
        ESP_LOGE(pcTaskGetName(NULL), "Does not recognize the module");
        while (1)
        {
            vTaskDelay(1);
        }
    }

    ESP_LOGI(pcTaskGetName(NULL), "Frequency is 868MHz");

    int cr = 1;
    int bw = 7;
    int sf = 7;

    lora_set_frequency(866e6);
    lora_enable_crc();
    lora_set_coding_rate(cr);
    lora_set_bandwidth(bw);
    lora_set_spreading_factor(sf);
}

void sendLoraMessage(const char *message)
{

    uint8_t buf[256];
    int send_len = sprintf((char *)buf, "%s", message);
    lora_send_packet(buf, send_len);
    ESP_LOGI(pcTaskGetName(NULL), "%d byte packet sent...", 255);
    int lost = lora_packet_lost();
    if (lost != 0)
    {
        ESP_LOGW(pcTaskGetName(NULL), "%d packets lost", lost);
    }
    else
    {
        ESP_LOGI(pcTaskGetName(NULL), "No packet lost");
    }
}

void handleUdpReceive(void *aContext, otMessage *aMessage,
                      const otMessageInfo *aMessageInfo)
{

    char buf[1500];
    int length;

    OT_UNUSED_VARIABLE(aContext);
    OT_UNUSED_VARIABLE(aMessage);
    OT_UNUSED_VARIABLE(aMessageInfo);
    ESP_LOGI(TAG, "Received UDP message");

    uint16_t messageLength = otMessageGetLength(aMessage) - otMessageGetOffset(aMessage);
    ESP_LOGI(TAG, "Message length: %d bytes", messageLength);

    ESP_LOGI(TAG, "From Port: %d", aMessageInfo->mPeerPort);

    length = otMessageRead(aMessage, otMessageGetOffset(aMessage), buf, sizeof(buf) - 1);
    buf[length] = '\0';
    ESP_LOGI(TAG, "Message: %s", buf);

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return;
    }

    cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (cJSON_IsString(type))
    {
        if (strcmp(type->valuestring, "device_connected") == 0)
        {
            char dateTime[64];
            getCurrentDateTime(dateTime, sizeof(dateTime));

            char response[256];
            snprintf(response, sizeof(response),
                     "{\"type\":\"accept\",\"date\":\"%s\",\"measurements\":%d,\"period\":%d}",
                     dateTime, measurements_count, measurement_period);
            ESP_LOGI(TAG, "Sent UDP response: %s", response);
            sendUdpMessage(aContext, response, aMessageInfo);
            sendLoraMessage(buf);
        }
        else if (strcmp(type->valuestring, "data") == 0)
        {
            cJSON *values = cJSON_GetObjectItemCaseSensitive(root, "values");
            if (cJSON_IsArray(values))
            {
                ESP_LOGI(TAG, "Sent Lora message: %s", buf);
                sendLoraMessage(buf);
                ESP_LOGI(TAG, "Sent LoRa message with data values");
            }
        }
    }

    cJSON_Delete(root);
}

void initUdp(otInstance *aInstance)
{
    otSockAddr listenSockAddr;

    memset(&sUdpSocket, 0, sizeof(sUdpSocket));
    memset(&listenSockAddr, 0, sizeof(listenSockAddr));

    listenSockAddr.mPort = UDP_PORT;

    otUdpOpen(aInstance, &sUdpSocket, handleUdpReceive, aInstance);
    otUdpBind(aInstance, &sUdpSocket, &listenSockAddr, OT_NETIF_THREAD);
}

void setNetworkConfiguration(otInstance *aInstance)
{

    otThreadSetNetworkName(sInstance, CONFIG_OPENTHREAD_NETWORK_NAME);
    otLinkSetPanId(sInstance, CONFIG_OPENTHREAD_NETWORK_PANID);
    otLinkSetChannel(sInstance, CONFIG_OPENTHREAD_NETWORK_CHANNEL);

    size_t len = 0;
    static char aNetworkName[] = CONFIG_OPENTHREAD_NETWORK_NAME;
    otOperationalDataset aDataset;

    memset(&aDataset, 0, sizeof(otOperationalDataset));

    aDataset.mActiveTimestamp.mSeconds = 1;
    aDataset.mActiveTimestamp.mTicks = 0;
    aDataset.mActiveTimestamp.mAuthoritative = false;
    aDataset.mComponents.mIsActiveTimestampPresent = true;

    aDataset.mChannel = CONFIG_OPENTHREAD_NETWORK_CHANNEL;
    aDataset.mComponents.mIsChannelPresent = true;
    aDataset.mPanId = CONFIG_OPENTHREAD_NETWORK_PANID;
    aDataset.mComponents.mIsPanIdPresent = true;
    len = strlen(CONFIG_OPENTHREAD_NETWORK_NAME);
    assert(len <= OT_NETWORK_NAME_MAX_SIZE);
    memcpy(aDataset.mNetworkName.m8, CONFIG_OPENTHREAD_NETWORK_NAME, len + 1);
    aDataset.mComponents.mIsNetworkNamePresent = true;

    len = hex_string_to_binary(CONFIG_OPENTHREAD_NETWORK_EXTPANID, aDataset.mExtendedPanId.m8,
                               sizeof(aDataset.mExtendedPanId.m8));
    aDataset.mComponents.mIsExtendedPanIdPresent = true;

    otIp6Prefix prefix;
    memset(&prefix, 0, sizeof(otIp6Prefix));
    if (otIp6PrefixFromString(CONFIG_OPENTHREAD_MESH_LOCAL_PREFIX, &prefix) == OT_ERROR_NONE)
    {
        memcpy(aDataset.mMeshLocalPrefix.m8, prefix.mPrefix.mFields.m8, sizeof(aDataset.mMeshLocalPrefix.m8));
        aDataset.mComponents.mIsMeshLocalPrefixPresent = true;
    }
    else
    {
        ESP_LOGE("Failed to parse mesh local prefix", CONFIG_OPENTHREAD_MESH_LOCAL_PREFIX);
    }

    len = hex_string_to_binary(CONFIG_OPENTHREAD_NETWORK_MASTERKEY, aDataset.mNetworkKey.m8,
                               sizeof(aDataset.mNetworkKey.m8));
    aDataset.mComponents.mIsNetworkKeyPresent = true;

    len = hex_string_to_binary(CONFIG_OPENTHREAD_NETWORK_PSKC, aDataset.mPskc.m8, sizeof(aDataset.mPskc.m8));
    aDataset.mComponents.mIsPskcPresent = true;

    // /* Set Extended Pan ID to C0DE1AB5C0DE1AB5 */
    // uint8_t extPanId[OT_EXT_PAN_ID_SIZE] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    // memcpy(aDataset.mExtendedPanId.m8, extPanId, sizeof(aDataset.mExtendedPanId));
    // aDataset.mComponents.mIsExtendedPanIdPresent = true;

    // /* Set network key to 1234C0DE1AB51234C0DE1AB51234C0DE */
    // uint8_t networkKey[OT_NETWORK_KEY_SIZE] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    //                                            0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    // memcpy(aDataset.mNetworkKey.m8, networkKey, sizeof(aDataset.mNetworkKey));
    // aDataset.mComponents.mIsNetworkKeyPresent = true;

    // Set Network Key
    // uint8_t networkKey[OT_NETWORK_KEY_SIZE];
    // size_t keyLength = strlen(NETWORK_KEY) / 2;
    // for (size_t i = 0; i < keyLength && i < OT_NETWORK_KEY_SIZE; i++) {
    //     sscanf(&NETWORK_KEY[i * 2], "%2hhx", &networkKey[i]);
    // }
    // memcpy(aDataset.mNetworkKey.m8, networkKey, sizeof(aDataset.mNetworkKey));
    // aDataset.mComponents.mIsNetworkKeyPresent = true;

    otDatasetSetActive(sInstance, &aDataset);
    uint8_t jitterValue = 20;
    otThreadSetRouterSelectionJitter(aInstance, jitterValue);
}

void cleanup(void)
{
    if (sInstance != NULL)
    {
        otThreadSetEnabled(sInstance, false);
        otInstanceFinalize(sInstance);
        esp_openthread_deinit();
    }
}

static void ot_task_worker(void *aContext)
{
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };

    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_OPENTHREAD();
    esp_netif_t *openthread_netif = esp_netif_new(&cfg);
    assert(openthread_netif != NULL);

    ESP_ERROR_CHECK(esp_openthread_init(&config));

    esp_openthread_lock_acquire(portMAX_DELAY);

    ESP_ERROR_CHECK(esp_netif_attach(openthread_netif, esp_openthread_netif_glue_init(&config)));
    (void)otLoggingSetLevel(CONFIG_LOG_DEFAULT_LEVEL);

    esp_openthread_lock_release();
    esp_openthread_launch_mainloop();

    esp_openthread_netif_glue_deinit();
    esp_netif_destroy(openthread_netif);
    esp_vfs_eventfd_unregister();
    vTaskDelete(NULL);
}

void handleWakeUp(int active_window_seconds, int m_count, int m_period)
{

    s_event_group = xEventGroupCreate();
    if (s_event_group == NULL)
    {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }
    esp_vfs_eventfd_config_t eventfd_config = {.max_fds = 4};
    ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    xTaskCreate(ot_task_worker, "ot_ts_wroker", 20480, xTaskGetCurrentTaskHandle(), 5, NULL);

    sInstance = esp_openthread_get_instance();

    setNetworkConfiguration(sInstance);

    otSetStateChangedCallback(sInstance, handleNetifStateChanged, sInstance);

    measurements_count = m_count;
    measurement_period = m_period;

    initUdp(sInstance);

    ESP_ERROR_CHECK(otIp6SetEnabled(sInstance, true));
    ESP_ERROR_CHECK(otThreadSetEnabled(sInstance, true));

    while (otThreadGetDeviceRole(sInstance) != OT_DEVICE_ROLE_LEADER)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(TAG, "Successfully joined the Thread network");
    setUpLoraNetwork();
    ESP_LOGI(TAG, "Iniciando modo activo por %d segundos", active_window_seconds);

    uint8_t receive_buf[256];
    int received_len = 0;
    int elapsed_time = 0;
    int received_confirmation = 0;
    int delay_interval = 100;
    uint8_t buf[256];
    lora_receive_mode(true);
    sendLoraMessage("{\"type\":\"connected\",\"values\":[\"GW_Leader\"]}");
    char *received_time = NULL;

    while (received_confirmation == 0)
    {
        lora_receive();
        if (lora_received())
        {
            int rxLen = lora_receive_packet(buf, sizeof(buf));
            ESP_LOGI(pcTaskGetName(NULL), "%d byte packet received:[%.*s]", rxLen, rxLen, buf);

            cJSON *root = cJSON_Parse((const char *)buf);
            if (root == NULL)
            {
                ESP_LOGE(pcTaskGetName(NULL), "Failed to parse JSON");
                continue;
            }

            cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
            if (cJSON_IsString(type) && (strcmp(type->valuestring, "accepted") == 0))
            {
                cJSON *values = cJSON_GetObjectItemCaseSensitive(root, "values");
                if (cJSON_IsArray(values) && cJSON_GetArraySize(values) == 2)
                {
                    cJSON *time_item = cJSON_GetArrayItem(values, 1);
                    if (cJSON_IsString(time_item))
                    {
                        received_time = strdup(time_item->valuestring);
                        received_confirmation = 1;
                        if (received_time != NULL)
                        {
                            esp_err_t err;
                            nvs_handle_t nvs_handle;
                            err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
                            ESP_ERROR_CHECK(err);
                            err = nvs_set_str(nvs_handle, "system_time", received_time);
                            ESP_ERROR_CHECK(err);
                            err = nvs_commit(nvs_handle);
                            ESP_ERROR_CHECK(err);

                            nvs_close(nvs_handle);
                            struct tm tm;
                            strptime(received_time, "%Y-%m-%dT%H:%M:%SZ", &tm);
                            time_t t = mktime(&tm);
                            struct timeval now = {.tv_sec = t};
                            settimeofday(&now, NULL);
                            ESP_LOGI(pcTaskGetName(NULL), "System time set to: %s", received_time);
                            free(received_time);
                        }
                    }
                }
            }

            cJSON_Delete(root);
        }
        vTaskDelay(1);
    }
    lora_receive_mode(false);

    vTaskDelay(pdMS_TO_TICKS(300000));

    ESP_LOGI(TAG, "Tiempo activo finalizado, preparando para dormir");

    otThreadSetEnabled(sInstance, false);
    while (otThreadGetDeviceRole(sInstance) != OT_DEVICE_ROLE_DISABLED)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
