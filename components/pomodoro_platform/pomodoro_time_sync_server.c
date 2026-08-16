#include "pomodoro_time_sync_server.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    SemaphoreHandle_t lock;
    httpd_handle_t server;
    esp_netif_t *netif;
    esp_event_handler_instance_t wifi_handler;
    pomodoro_application_time_sync_submission_t submission;
    bool running;
    bool connected;
    bool submitted;
    bool failed;
} time_sync_server_t;

static const char *TAG = "pomodoro_time_sync";
static time_sync_server_t time_sync;

static const char sync_page[] =
    "<!doctype html><html><head><meta name=viewport content='width=device-width'>"
    "<title>Pomodoro time sync</title><style>body{font:18px system-ui;"
    "max-width:34rem;margin:3rem auto;padding:0 1rem;color:#171b22}"
    "button{font:inherit;padding:.8rem 1.2rem;border:0;border-radius:1rem;"
    "background:#ff5c5c;color:white}</style></head><body>"
    "<h1>Sync to Pomodoro</h1><p>This local page sends your iPhone's current "
    "time and daylight-saving schedule directly to the timer. No internet "
    "connection or account is used.</p><button id=s>Sync to Pomodoro</button>"
    "<p id=o></p><script>"
    "const off=t=>-new Date(t*1000).getTimezoneOffset();"
    "function transition(a,b,old){while(b-a>60){const m=Math.floor((a+b)/2);"
    "if(off(m)==old)a=m;else b=m}return Math.floor(b/60)*60}"
    "function payload(){const now=Math.floor(Date.now()/1000),end=now+"
    "Math.floor(365.2425*5*86400),z=Intl.DateTimeFormat().resolvedOptions()."
    "timeZone,t=[[0,off(now)]];let p=now,last=off(now);for(let n=now+86400;"
    "n<=end;n+=86400){const x=off(n);if(x!=last){const at=transition(p,n,last);"
    "t.push([at,x]);last=x}p=n}return{confirmed:true,utc:now,zone:z,"
    "expires:end,transitions:t}}"
    "document.getElementById('s').onclick=async()=>{const b=document."
    "getElementById('s'),o=document.getElementById('o');b.disabled=true;"
    "o.textContent='Syncing time…';try{const r=await fetch('/sync',{method:'POST',"
    "headers:{'Content-Type':'application/json'},body:JSON.stringify(payload())});"
    "if(!r.ok)throw Error();o.textContent='Time synced. You can return to the "
    "Pomodoro.'}catch(e){o.textContent=\"Couldn't sync time. Try again.\";"
    "b.disabled=false}}</script></body></html>";

static void wifi_event(void *argument, esp_event_base_t base,
                       int32_t event_id, void *event_data)
{
    (void) argument;
    (void) base;
    (void) event_data;
    if (event_id == WIFI_EVENT_AP_STACONNECTED && time_sync.lock != NULL &&
        xSemaphoreTake(time_sync.lock, pdMS_TO_TICKS(50)) == pdTRUE) {
        time_sync.connected = true;
        xSemaphoreGive(time_sync.lock);
    }
}

static esp_err_t page_handler(httpd_req_t *request)
{
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    return httpd_resp_send(request, sync_page, HTTPD_RESP_USE_STRLEN);
}

static void mark_failed(void)
{
    if (time_sync.lock != NULL &&
        xSemaphoreTake(time_sync.lock, pdMS_TO_TICKS(100)) == pdTRUE) {
        time_sync.failed = true;
        xSemaphoreGive(time_sync.lock);
    }
}

static bool json_uint64(const cJSON *object, const char *name, uint64_t *value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!cJSON_IsNumber(item) || item->valuedouble < 0.0 ||
        item->valuedouble > (double) UINT64_MAX) {
        return false;
    }
    *value = (uint64_t) item->valuedouble;
    return (double) *value == item->valuedouble;
}

static bool parse_submission(
    const char *body, pomodoro_application_time_sync_submission_t *submission)
{
    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return false;
    }
    const cJSON *confirmed = cJSON_GetObjectItemCaseSensitive(
        root, "confirmed");
    const cJSON *zone = cJSON_GetObjectItemCaseSensitive(root, "zone");
    const cJSON *transitions = cJSON_GetObjectItemCaseSensitive(
        root, "transitions");
    memset(submission, 0, sizeof(*submission));
    bool valid = cJSON_IsTrue(confirmed) && cJSON_IsString(zone) &&
        zone->valuestring != NULL && zone->valuestring[0] != '\0' &&
        strlen(zone->valuestring) < POMODORO_TIME_ZONE_NAME_CAPACITY &&
        cJSON_IsArray(transitions) &&
        cJSON_GetArraySize(transitions) > 0 &&
        cJSON_GetArraySize(transitions) <=
            POMODORO_TIME_ZONE_TRANSITION_CAPACITY &&
        json_uint64(root, "utc", &submission->utc_epoch_seconds) &&
        json_uint64(root, "expires",
                    &submission->time_zone.expires_utc_epoch_seconds);
    if (valid) {
        submission->time_zone.automatic = true;
        strlcpy(submission->time_zone.iana_name, zone->valuestring,
                sizeof(submission->time_zone.iana_name));
        submission->time_zone.transition_count =
            (uint8_t) cJSON_GetArraySize(transitions);
    }
    for (uint8_t index = 0U;
         valid && index < submission->time_zone.transition_count; index++) {
        const cJSON *entry = cJSON_GetArrayItem(transitions, index);
        if (!cJSON_IsArray(entry) || cJSON_GetArraySize(entry) != 2) {
            valid = false;
            break;
        }
        const cJSON *epoch = cJSON_GetArrayItem(entry, 0);
        const cJSON *offset = cJSON_GetArrayItem(entry, 1);
        valid = cJSON_IsNumber(epoch) && cJSON_IsNumber(offset) &&
            epoch->valuedouble >= 0.0 &&
            epoch->valuedouble <= (double) UINT64_MAX &&
            offset->valuedouble >= -720.0 && offset->valuedouble <= 840.0;
        if (valid) {
            submission->time_zone.transitions[index] =
                (pomodoro_utc_offset_transition_t) {
                    .effective_utc_epoch_seconds =
                        (uint64_t) epoch->valuedouble,
                    .offset_minutes = (int16_t) offset->valuedouble,
                };
            valid = (double) submission->time_zone.transitions[index]
                            .effective_utc_epoch_seconds == epoch->valuedouble &&
                (double) submission->time_zone.transitions[index]
                            .offset_minutes == offset->valuedouble;
        }
    }
    valid = valid && pomodoro_time_zone_schedule_is_valid(
        &submission->time_zone) &&
        submission->utc_epoch_seconds <=
            UINT64_MAX - POMODORO_TIME_ZONE_MINIMUM_FUTURE_SECONDS &&
        submission->time_zone.expires_utc_epoch_seconds >=
            submission->utc_epoch_seconds +
                POMODORO_TIME_ZONE_MINIMUM_FUTURE_SECONDS;
    cJSON_Delete(root);
    return valid;
}

static esp_err_t sync_handler(httpd_req_t *request)
{
    if (request->content_len == 0U || request->content_len > 2048U) {
        mark_failed();
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "Invalid payload");
    }
    char body[2049];
    size_t received = 0U;
    while (received < request->content_len) {
        int chunk = httpd_req_recv(request, body + received,
                                   request->content_len - received);
        if (chunk <= 0) {
            mark_failed();
            return httpd_resp_send_err(request,
                                       HTTPD_500_INTERNAL_SERVER_ERROR,
                                       "Receive failed");
        }
        received += (size_t) chunk;
    }
    body[received] = '\0';
    pomodoro_application_time_sync_submission_t submission;
    if (!parse_submission(body, &submission) || time_sync.lock == NULL ||
        xSemaphoreTake(time_sync.lock, pdMS_TO_TICKS(100)) != pdTRUE) {
        mark_failed();
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "Invalid time data");
    }
    time_sync.submission = submission;
    time_sync.submitted = true;
    xSemaphoreGive(time_sync.lock);
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, "{\"accepted\":true}");
}

static void random_text(char *destination, size_t length)
{
    static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    for (size_t index = 0U; index + 1U < length; index++) {
        destination[index] = alphabet[esp_random() % (sizeof(alphabet) - 1U)];
    }
    destination[length - 1U] = '\0';
}

bool pomodoro_time_sync_server_start(
    pomodoro_application_time_sync_session_t *session)
{
    if (session == NULL || time_sync.running) {
        return false;
    }
    memset(&time_sync, 0, sizeof(time_sync));
    time_sync.lock = xSemaphoreCreateMutex();
    if (time_sync.lock == NULL || esp_netif_init() != ESP_OK ||
        esp_event_loop_create_default() != ESP_OK) {
        pomodoro_time_sync_server_stop();
        return false;
    }
    time_sync.netif = esp_netif_create_default_wifi_ap();
    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    if (time_sync.netif == NULL || esp_wifi_init(&wifi_init) != ESP_OK ||
        esp_event_handler_instance_register(
            WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, wifi_event, NULL,
            &time_sync.wifi_handler) != ESP_OK) {
        pomodoro_time_sync_server_stop();
        return false;
    }
    char suffix[5];
    random_text(suffix, sizeof(suffix));
    random_text(session->password, 13U);
    snprintf(session->ssid, sizeof(session->ssid), "Pomodoro-%s", suffix);
    snprintf(session->join_uri, sizeof(session->join_uri),
             "WIFI:T:WPA;S:%s;P:%s;;", session->ssid, session->password);
    wifi_config_t wifi = {0};
    strlcpy((char *) wifi.ap.ssid, session->ssid, sizeof(wifi.ap.ssid));
    strlcpy((char *) wifi.ap.password, session->password,
            sizeof(wifi.ap.password));
    wifi.ap.ssid_len = strlen(session->ssid);
    wifi.ap.channel = 1U;
    wifi.ap.max_connection = 1U;
    wifi.ap.authmode = WIFI_AUTH_WPA2_PSK;
    wifi.ap.pmf_cfg.required = true;
    if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK ||
        esp_wifi_set_mode(WIFI_MODE_AP) != ESP_OK ||
        esp_wifi_set_config(WIFI_IF_AP, &wifi) != ESP_OK ||
        esp_wifi_start() != ESP_OK) {
        pomodoro_time_sync_server_stop();
        return false;
    }
    httpd_config_t http = HTTPD_DEFAULT_CONFIG();
    http.max_uri_handlers = 2U;
    if (httpd_start(&time_sync.server, &http) != ESP_OK) {
        pomodoro_time_sync_server_stop();
        return false;
    }
    const httpd_uri_t page = {
        .uri = "/", .method = HTTP_GET, .handler = page_handler,
    };
    const httpd_uri_t sync = {
        .uri = "/sync", .method = HTTP_POST, .handler = sync_handler,
    };
    if (httpd_register_uri_handler(time_sync.server, &page) != ESP_OK ||
        httpd_register_uri_handler(time_sync.server, &sync) != ESP_OK) {
        pomodoro_time_sync_server_stop();
        return false;
    }
    time_sync.running = true;
    ESP_LOGI(TAG, "Temporary SoftAP %s started", session->ssid);
    return true;
}

pomodoro_application_time_sync_poll_result_t
pomodoro_time_sync_server_poll(
    pomodoro_application_time_sync_submission_t *submission)
{
    if (!time_sync.running || time_sync.lock == NULL || submission == NULL ||
        xSemaphoreTake(time_sync.lock, pdMS_TO_TICKS(50)) != pdTRUE) {
        return POMODORO_APPLICATION_TIME_SYNC_ERROR;
    }
    pomodoro_application_time_sync_poll_result_t result =
        time_sync.failed ? POMODORO_APPLICATION_TIME_SYNC_ERROR :
        time_sync.submitted ? POMODORO_APPLICATION_TIME_SYNC_SUBMITTED :
        time_sync.connected ? POMODORO_APPLICATION_TIME_SYNC_CONNECTED :
                              POMODORO_APPLICATION_TIME_SYNC_WAITING;
    if (time_sync.submitted) {
        *submission = time_sync.submission;
    }
    xSemaphoreGive(time_sync.lock);
    return result;
}

bool pomodoro_time_sync_server_stop(void)
{
    bool stopped = true;
    if (time_sync.server != NULL) {
        stopped = httpd_stop(time_sync.server) == ESP_OK && stopped;
    }
    if (time_sync.wifi_handler != NULL) {
        stopped = esp_event_handler_instance_unregister(
            WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED,
            time_sync.wifi_handler) == ESP_OK && stopped;
    }
    if (time_sync.netif != NULL) {
        (void) esp_wifi_stop();
        (void) esp_wifi_deinit();
        esp_netif_destroy_default_wifi(time_sync.netif);
        (void) esp_event_loop_delete_default();
        (void) esp_netif_deinit();
    }
    SemaphoreHandle_t lock = time_sync.lock;
    memset(&time_sync, 0, sizeof(time_sync));
    if (lock != NULL) {
        vSemaphoreDelete(lock);
    }
    ESP_LOGI(TAG, "Temporary SoftAP stopped and session cleared");
    return stopped;
}
