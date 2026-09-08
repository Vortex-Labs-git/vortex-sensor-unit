/**
 * @file time_func.c
 * @brief SNTP Time Synchronization and Timestamp Utilities
 *
 * This module:
 *  - Synchronizes system time using SNTP (NTP servers)
 *  - Configures timezone
 *  - Waits until valid time is obtained
 *  - Provides helper function to get ISO 8601 timestamps
 */

 #include <stddef.h>
#include <stdio.h>
#include <time.h>
#include "esp_sntp.h"
#include "esp_log.h"

#include "time_func.h"


static const char *TAG_TIME = "TimeSync";



/* ======================================================================== */
/* ========================== TIME SYNCHRONIZATION ======================== */
/* ======================================================================== */

/**
 * @brief Synchronize system time using SNTP
 *
 * This function:
 *  1. Configures SNTP in polling mode
 *  2. Sets multiple NTP servers (for redundancy)
 *  3. Initializes SNTP service
 *  4. Sets timezone to Asia/Colombo
 *  5. Blocks until a valid time is received
 *
 * The function waits in a loop until the year becomes >= 2020,
 * which ensures that the system time has been properly updated
 * from the NTP server (instead of default epoch time).
 *
 * @note This function blocks until time is synchronized.
 *       Should be called after WiFi connection is established.
 */
static bool s_sntp_started = false;   /* SNTP configured at least once */
static bool s_sync_task_running = false;
void obtain_time(void *pvParameters)
{
    (void) pvParameters;

    if (s_sync_task_running) {
        ESP_LOGW(TAG_TIME, "Time sync task already running, skipping");
        vTaskDelete(NULL);
        return;
    }
    s_sync_task_running = true;

    if (!s_sntp_started) {
        ESP_LOGI(TAG_TIME, "Initializing SNTP");
        esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_setservername(1, "time.nist.gov");
        esp_sntp_setservername(2, "time.google.com");
        esp_sntp_init();
        s_sntp_started = true;
    } else {
        ESP_LOGI(TAG_TIME, "SNTP already running, restarting sync");
        sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);  /* force a fresh sync */
        esp_sntp_restart();
    }


    time_t now = 0;
    struct tm timeinfo = {0};
    
    int retry = 0;
    const int retry_count = 30;

    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET &&
        retry < retry_count)
    {
        ESP_LOGI(TAG_TIME, "Waiting for SNTP sync...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        retry++;
    }

    time(&now);
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_year >= (2020 - 1900)) {
        setenv("TZ", "IST-5:30", 1);
        tzset();

        ESP_LOGI(TAG_TIME, "NTP Time synchronized successfully");
        ESP_LOGI(TAG_TIME, "Current ntp time: %04d-%02d-%02d %02d:%02d:%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    } else {
        ESP_LOGW(TAG_TIME, "NTP Time sync failed, using default system time");
    }

    vTaskDelete(NULL); // Delete task after finishing
}





/* ======================================================================== */
/* ========================== TIMESTAMP UTILITY =========================== */
/* ======================================================================== */

/**
 * @brief Get current local timestamp in ISO 8601 format
 *
 * Format:
 *     YYYY-MM-DDTHH:MM:SSZ
 *
 * Example:
 *     2026-02-18T14:25:30Z
 */
void get_current_timestamp(char *timestamp, size_t timestamp_size) {
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime); // Get current time
    timeinfo = localtime(&rawtime); // Convert to local time

    // Format the time to ISO 8601 format: YYYY-MM-DDTHH:MM:SSZ
    strftime(timestamp, timestamp_size, "%Y-%m-%dT%H:%M:%SZ", timeinfo);
}