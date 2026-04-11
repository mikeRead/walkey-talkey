#include "config_http_server.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "config_api_service.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "nvs_flash.h"

#define CONFIG_HTTP_MAX_BODY_BYTES (32 * 1024)
#define CONFIG_HTTP_SERVER_STACK_SIZE 6144
#define CONFIG_HTTP_RESPONSE_CHUNK_BYTES 1024
#define CONFIG_HTTP_DEFAULT_AP_SSID "walkey-talkey"
#define CONFIG_HTTP_DEFAULT_AP_PASSWORD "secretKEY"
#define CONFIG_HTTP_DEFAULT_HOSTNAME "walkey-talkey"
#define CONFIG_HTTP_DEFAULT_LOCAL_URL "walkey-talkey.local"
#define CONFIG_HTTP_STA_CONNECTED_BIT BIT0
#define CONFIG_HTTP_STA_FAILED_BIT BIT1
#define CONFIG_HTTP_STA_MAX_RETRIES 8
#define CONFIG_HTTP_STA_CONNECT_TIMEOUT_MS 20000
#define CONFIG_HTTP_WIFI_RELOAD_TASK_STACK_WORDS 4096
#define CONFIG_HTTP_WIFI_RELOAD_TASK_PRIORITY 2

static const char *TAG = "config_http";

static void *config_http_calloc_prefer_psram(size_t size)
{
    void *buffer = heap_caps_calloc(1, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buffer == NULL) {
        buffer = calloc(1, size);
    }
    return buffer;
}

static esp_err_t config_http_send_chunk_sequence(httpd_req_t *req, const char *data, size_t length)
{
    if ((req == NULL) || ((data == NULL) && (length > 0))) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t offset = 0;
    while (offset < length) {
        size_t chunk_len = length - offset;
        if (chunk_len > CONFIG_HTTP_RESPONSE_CHUNK_BYTES) {
            chunk_len = CONFIG_HTTP_RESPONSE_CHUNK_BYTES;
        }

        esp_err_t err = httpd_resp_send_chunk(req, data + offset, chunk_len);
        if (err != ESP_OK) {
            return err;
        }
        offset += chunk_len;
    }

    return ESP_OK;
}

static esp_err_t config_http_send_chunked_buffer(httpd_req_t *req, const char *data, size_t length)
{
    ESP_RETURN_ON_ERROR(config_http_send_chunk_sequence(req, data, length), TAG, "Chunk send failed");
    return httpd_resp_send_chunk(req, NULL, 0);
}

typedef enum {
    CONFIG_HTTP_NETWORK_MODE_NONE = 0,
    CONFIG_HTTP_NETWORK_MODE_STA,
    CONFIG_HTTP_NETWORK_MODE_AP,
} config_http_network_mode_t;

static esp_err_t config_http_server_init_mdns(config_http_network_mode_t mode);
static esp_err_t config_http_notify_ui(void);
static esp_err_t config_http_schedule_wifi_reload(void);

extern const uint8_t schema_download_start[] asm("_binary_mode_config_schema_json_start");
extern const uint8_t schema_download_end[] asm("_binary_mode_config_schema_json_end");
extern const uint8_t ai_guide_download_start[] asm("_binary_AI_GUIDE_md_start");
extern const uint8_t ai_guide_download_end[] asm("_binary_AI_GUIDE_md_end");
extern const uint8_t user_guide_download_start[] asm("_binary_USER_GUIDE_md_start");
extern const uint8_t user_guide_download_end[] asm("_binary_USER_GUIDE_md_end");

static httpd_handle_t s_http_server = NULL;
static esp_netif_t *s_wifi_ap_netif = NULL;
static esp_netif_t *s_wifi_sta_netif = NULL;
static bool s_wifi_ready = false;
static bool s_wifi_initialized = false;
static bool s_mdns_ready = false;
static EventGroupHandle_t s_wifi_event_group = NULL;
static esp_event_handler_instance_t s_wifi_event_handler_instance = NULL;
static esp_event_handler_instance_t s_ip_event_handler_instance = NULL;
static config_http_network_mode_t s_network_mode = CONFIG_HTTP_NETWORK_MODE_NONE;
static int s_sta_retry_count = 0;
static bool s_sta_connect_enabled = false;
EXT_RAM_BSS_ATTR static char s_ap_ssid[33] = CONFIG_HTTP_DEFAULT_AP_SSID;
EXT_RAM_BSS_ATTR static char s_ap_password[65] = CONFIG_HTTP_DEFAULT_AP_PASSWORD;
EXT_RAM_BSS_ATTR static char s_sta_ssid[33] = "";
EXT_RAM_BSS_ATTR static char s_sta_password[65] = "";
EXT_RAM_BSS_ATTR static char s_hostname[65] = CONFIG_HTTP_DEFAULT_HOSTNAME;
EXT_RAM_BSS_ATTR static char s_local_url[65] = CONFIG_HTTP_DEFAULT_LOCAL_URL;
EXT_RAM_BSS_ATTR static char s_base_url[96] = "http://192.168.4.1/";
EXT_RAM_BSS_ATTR static char s_display_address[96] = "Connecting...";
static config_http_reload_fn_t s_reload_fn = NULL;
static void *s_reload_user_data = NULL;
static config_http_notify_fn_t s_notify_ui_fn = NULL;
static void *s_notify_ui_user_data = NULL;
static TaskHandle_t s_wifi_reload_task = NULL;
static volatile bool s_wifi_reload_pending = false;

static const char *s_web_ui_html =
    "<!doctype html>\n"
    "<html lang=\"en\">\n"
    "<head>\n"
    "  <meta charset=\"utf-8\">\n"
    "  <meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
    "  <title>WalKEY-TalKEY Config</title>\n"
    "  <style>\n"
    "    :root { color-scheme: dark; }\n"
    "    body { margin: 0; font-family: monospace; background: #111827; color: #e5e7eb; }\n"
    "    main { max-width: 980px; margin: 0 auto; padding: 24px; }\n"
    "    h1 { font-size: 22px; margin: 0 0 8px; }\n"
    "    p { color: #9ca3af; margin: 0 0 16px; }\n"
    "    .row { display: flex; gap: 12px; flex-wrap: wrap; margin-bottom: 12px; }\n"
    "    button { border: 0; border-radius: 8px; padding: 10px 14px; background: #2563eb; color: #fff; cursor: pointer; }\n"
    "    button.secondary { background: #374151; }\n"
    "    button.danger { background: #b91c1c; }\n"
    "    button.small { padding: 6px 10px; font-size: 12px; }\n"
    "    textarea { width: 100%; min-height: 60vh; border: 1px solid #374151; border-radius: 10px; background: #030712; color: #e5e7eb; padding: 14px; box-sizing: border-box; }\n"
    "    pre { white-space: pre-wrap; border: 1px solid #374151; border-radius: 10px; background: #030712; color: #93c5fd; padding: 14px; min-height: 110px; }\n"
    "    .meta { display: flex; align-items: center; justify-content: space-between; gap: 12px; color: #86efac; }\n"
    "    .meta-details { display: flex; gap: 8px; flex-wrap: wrap; }\n"
    "    .meta-pill { border: 1px solid #374151; border-radius: 999px; padding: 4px 10px; background: #0f172a; }\n"
    "    .meta-actions { display: flex; gap: 8px; margin-left: auto; }\n"
    "    .panel-title { color: #9ca3af; margin: 12px 0 8px; }\n"
    "    .tree-panel { border: 1px solid #374151; border-radius: 10px; background: #030712; padding: 14px; margin-top: 12px; }\n"
    "    .tree-panel > summary { cursor: pointer; color: #93c5fd; }\n"
    "    .json-tree { margin-top: 12px; line-height: 1.5; }\n"
    "    .json-tree details { margin-left: 16px; }\n"
    "    .json-tree summary { cursor: pointer; }\n"
    "    .json-line { margin-left: 16px; }\n"
    "    .json-key { color: #93c5fd; }\n"
    "    .json-string { color: #86efac; }\n"
    "    .json-number { color: #fca5a5; }\n"
    "    .json-boolean { color: #fcd34d; }\n"
    "    .json-null { color: #9ca3af; }\n"
    "  </style>\n"
    "</head>\n"
    "<body>\n"
    "  <main>\n"
    "    <h1>WalKEY-TalKEY Config</h1>\n"
    "    <p>Edit the canonical mode JSON, validate it, save it, or restore the built-in firmware defaults.</p>\n"
    "    <p><strong>Documentation:</strong> <a href=\"/downloads/mode-config.schema.json\" target=\"_blank\" rel=\"noopener\">mode-config.schema.json</a> | <a href=\"/downloads/AI_GUIDE.md\" target=\"_blank\" rel=\"noopener\">AI_GUIDE.md</a> | <a href=\"/downloads/USER_GUIDE.md\" target=\"_blank\" rel=\"noopener\">USER_GUIDE.md</a></p>\n"
    "    <div class=\"row\">\n"
    "      <button id=\"reloadBtn\" class=\"secondary\">Reload</button>\n"
    "      <button id=\"validateBtn\">Validate</button>\n"
    "      <button id=\"saveBtn\">Save</button>\n"
    "      <button id=\"resetBtn\" class=\"danger\">Reset To Built-In Default</button>\n"
    "    </div>\n"
    "    <input id=\"importFileInput\" type=\"file\" accept=\".json,application/json\" style=\"display:none\">\n"
    "    <div class=\"meta\">\n"
    "      <div class=\"meta-details\" id=\"meta\"></div>\n"
    "      <div class=\"meta-actions\">\n"
    "        <button id=\"importBtn\" class=\"secondary small\">Import</button>\n"
    "        <button id=\"exportBtn\" class=\"secondary small\">Export</button>\n"
    "      </div>\n"
    "    </div>\n"
    "    <textarea id=\"editor\" spellcheck=\"false\"></textarea>\n"
    "    <details class=\"tree-panel\" open>\n"
    "      <summary>Collapsible JSON view</summary>\n"
    "      <div id=\"treeView\" class=\"json-tree\">Loading config...</div>\n"
    "    </details>\n"
    "    <pre id=\"status\">Loading config...</pre>\n"
    "  </main>\n"
    "  <script>\n"
    "    const editor = document.getElementById('editor');\n"
    "    const statusEl = document.getElementById('status');\n"
    "    const metaEl = document.getElementById('meta');\n"
    "    const importFileInput = document.getElementById('importFileInput');\n"
    "    const treeViewEl = document.getElementById('treeView');\n"
    "    const setStatus = (text) => { statusEl.textContent = text; };\n"
    "    const setMeta = (payload) => {\n"
    "      metaEl.textContent = '';\n"
    "      const parts = [];\n"
    "      if (payload.source) parts.push(payload.source);\n"
    "      if (payload.resetTarget) parts.push('reset ' + payload.resetTarget);\n"
    "      parts.forEach((part) => {\n"
    "        const pill = document.createElement('span');\n"
    "        pill.className = 'meta-pill';\n"
    "        pill.textContent = part;\n"
    "        metaEl.appendChild(pill);\n"
    "      });\n"
    "    };\n"
    "    const readPayload = async (response) => {\n"
    "      const payload = await response.json();\n"
    "      if (!response.ok || !payload.ok) {\n"
    "        throw new Error(JSON.stringify(payload, null, 2));\n"
    "      }\n"
    "      return payload;\n"
    "    };\n"
    "    const downloadTextFile = (filename, text) => {\n"
    "      const blob = new Blob([text], { type: 'application/json;charset=utf-8' });\n"
    "      const url = URL.createObjectURL(blob);\n"
    "      const anchor = document.createElement('a');\n"
    "      anchor.href = url;\n"
    "      anchor.download = filename;\n"
    "      document.body.appendChild(anchor);\n"
    "      anchor.click();\n"
    "      anchor.remove();\n"
    "      URL.revokeObjectURL(url);\n"
    "    };\n"
    "    const appendText = (parent, text, className) => {\n"
    "      const span = document.createElement('span');\n"
    "      if (className) span.className = className;\n"
    "      span.textContent = text;\n"
    "      parent.appendChild(span);\n"
    "    };\n"
    "    const appendPrimitive = (parent, value) => {\n"
    "      if (typeof value === 'string') {\n"
    "        appendText(parent, '\"' + value + '\"', 'json-string');\n"
    "        return;\n"
    "      }\n"
    "      if (typeof value === 'number') {\n"
    "        appendText(parent, String(value), 'json-number');\n"
    "        return;\n"
    "      }\n"
    "      if (typeof value === 'boolean') {\n"
    "        appendText(parent, String(value), 'json-boolean');\n"
    "        return;\n"
    "      }\n"
    "      appendText(parent, 'null', 'json-null');\n"
    "    };\n"
    "    const buildJsonNode = (key, value, depth) => {\n"
    "      const isArray = Array.isArray(value);\n"
    "      const isObject = value && typeof value === 'object';\n"
    "      if (!isObject) {\n"
    "        const line = document.createElement('div');\n"
    "        line.className = 'json-line';\n"
    "        if (key !== null) {\n"
    "          appendText(line, key, 'json-key');\n"
    "          appendText(line, ': ');\n"
    "        }\n"
    "        appendPrimitive(line, value);\n"
    "        return line;\n"
    "      }\n"
    "      const entries = isArray ? value.map((item, index) => ['[' + index + ']', item]) : Object.entries(value);\n"
    "      if (entries.length === 0) {\n"
    "        const line = document.createElement('div');\n"
    "        line.className = 'json-line';\n"
    "        if (key !== null) {\n"
    "          appendText(line, key, 'json-key');\n"
    "          appendText(line, ': ');\n"
    "        }\n"
    "        appendText(line, isArray ? '[]' : '{}', 'json-null');\n"
    "        return line;\n"
    "      }\n"
    "      const details = document.createElement('details');\n"
    "      details.open = depth < 2;\n"
    "      const summary = document.createElement('summary');\n"
    "      if (key !== null) {\n"
    "        appendText(summary, key, 'json-key');\n"
    "        appendText(summary, ': ');\n"
    "      }\n"
    "      appendText(summary, isArray ? '[' + entries.length + ']' : '{' + entries.length + '}');\n"
    "      details.appendChild(summary);\n"
    "      entries.forEach(([childKey, childValue]) => {\n"
    "        details.appendChild(buildJsonNode(childKey, childValue, depth + 1));\n"
    "      });\n"
    "      return details;\n"
    "    };\n"
    "    const renderJsonTree = (value) => {\n"
    "      treeViewEl.textContent = '';\n"
    "      treeViewEl.appendChild(buildJsonNode(null, value, 0));\n"
    "    };\n"
    "    const renderJsonTreeFromEditor = () => {\n"
    "      try {\n"
    "        renderJsonTree(JSON.parse(editor.value));\n"
    "      } catch (error) {\n"
    "        treeViewEl.textContent = 'Tree view updates when the editor contains valid JSON.';\n"
    "      }\n"
    "    };\n"
    "    const setEditorJson = (config) => {\n"
    "      editor.value = JSON.stringify(config, null, 2);\n"
    "      renderJsonTree(config);\n"
    "    };\n"
    "    const refreshEditor = async () => {\n"
    "      setStatus('Loading config...');\n"
    "      const payload = await readPayload(await fetch('/config', { cache: 'no-store' }));\n"
    "      setEditorJson(payload.config);\n"
    "      setMeta(payload);\n"
    "      setStatus('Loaded active config.');\n"
    "    };\n"
    "    document.getElementById('reloadBtn').onclick = () => refreshEditor().catch((error) => setStatus(String(error)));\n"
    "    document.getElementById('validateBtn').onclick = async () => {\n"
    "      setStatus('Validating...');\n"
    "      try {\n"
    "        const payload = await readPayload(await fetch('/config/validate', {\n"
    "          method: 'POST',\n"
    "          headers: { 'Content-Type': 'application/json' },\n"
    "          body: editor.value,\n"
    "        }));\n"
    "        setEditorJson(payload.config);\n"
    "        setMeta(payload);\n"
    "        setStatus('Config is valid and normalized.');\n"
    "      } catch (error) {\n"
    "        setStatus(String(error));\n"
    "      }\n"
    "    };\n"
    "    document.getElementById('saveBtn').onclick = async () => {\n"
    "      setStatus('Saving...');\n"
    "      try {\n"
    "        const payload = await readPayload(await fetch('/config', {\n"
    "          method: 'PUT',\n"
    "          headers: { 'Content-Type': 'application/json' },\n"
    "          body: editor.value,\n"
    "        }));\n"
    "        setEditorJson(payload.config);\n"
    "        setMeta(payload);\n"
    "        setStatus('Config saved and reloaded.');\n"
    "      } catch (error) {\n"
    "        setStatus(String(error));\n"
    "      }\n"
    "    };\n"
    "    document.getElementById('importBtn').onclick = () => importFileInput.click();\n"
    "    importFileInput.onchange = async () => {\n"
    "      const file = importFileInput.files && importFileInput.files[0];\n"
    "      if (!file) return;\n"
    "      setStatus('Importing and validating...');\n"
    "      try {\n"
    "        const text = await file.text();\n"
    "        const payload = await readPayload(await fetch('/config/validate', {\n"
    "          method: 'POST',\n"
    "          headers: { 'Content-Type': 'application/json' },\n"
    "          body: text,\n"
    "        }));\n"
    "        setEditorJson(payload.config);\n"
    "        setMeta(payload);\n"
    "        setStatus('Imported JSON is valid and loaded into the editor.');\n"
    "      } catch (error) {\n"
    "        setStatus(String(error));\n"
    "      } finally {\n"
    "        importFileInput.value = '';\n"
    "      }\n"
    "    };\n"
    "    document.getElementById('exportBtn').onclick = () => {\n"
    "      try {\n"
    "        const parsed = JSON.parse(editor.value);\n"
    "        downloadTextFile('mode-config.json', JSON.stringify(parsed, null, 2) + '\\n');\n"
    "        setStatus('Current editor JSON exported to mode-config.json.');\n"
    "      } catch (error) {\n"
    "        setStatus('Export failed: editor does not contain valid JSON.');\n"
    "      }\n"
    "    };\n"
    "    document.getElementById('resetBtn').onclick = async () => {\n"
    "      if (!confirm('Restore the built-in firmware default config?')) return;\n"
    "      setStatus('Restoring built-in defaults...');\n"
    "      try {\n"
    "        const payload = await readPayload(await fetch('/config/reset', { method: 'POST' }));\n"
    "        setEditorJson(payload.config);\n"
    "        setMeta(payload);\n"
    "        setStatus('Built-in default config restored.');\n"
    "      } catch (error) {\n"
    "        setStatus(String(error));\n"
    "      }\n"
    "    };\n"
    "    editor.addEventListener('input', renderJsonTreeFromEditor);\n"
    "    refreshEditor().catch((error) => setStatus(String(error)));\n"
    "  </script>\n"
    "</body>\n"
    "</html>\n";

static const char *config_http_json_error_code_name(mode_json_error_code_t code)
{
    switch (code) {
    case MODE_JSON_ERROR_PARSE:
        return "PARSE";
    case MODE_JSON_ERROR_DUPLICATE_KEY:
        return "DUPLICATE_KEY";
    case MODE_JSON_ERROR_UNKNOWN_FIELD:
        return "UNKNOWN_FIELD";
    case MODE_JSON_ERROR_OUT_OF_MEMORY:
        return "OUT_OF_MEMORY";
    case MODE_JSON_ERROR_INVALID_VALUE:
        return "INVALID_VALUE";
    case MODE_JSON_ERROR_INVALID_REFERENCE:
        return "INVALID_REFERENCE";
    case MODE_JSON_ERROR_RUNTIME_LIMIT:
        return "RUNTIME_LIMIT";
    case MODE_JSON_ERROR_NONE:
    default:
        return "NONE";
    }
}

static const char *config_http_service_status_name(config_api_service_status_t status)
{
    switch (status) {
    case CONFIG_API_SERVICE_INVALID_ARGUMENT:
        return "INVALID_ARGUMENT";
    case CONFIG_API_SERVICE_PARSE_FAILED:
        return "PARSE_FAILED";
    case CONFIG_API_SERVICE_EXPORT_FAILED:
        return "EXPORT_FAILED";
    case CONFIG_API_SERVICE_STORAGE_FAILED:
        return "STORAGE_FAILED";
    case CONFIG_API_SERVICE_OK:
    default:
        return "OK";
    }
}

static const char *config_http_storage_stage_name(mode_config_storage_stage_t stage)
{
    switch (stage) {
    case MODE_CONFIG_STORAGE_STAGE_MOUNT:
        return "mount";
    case MODE_CONFIG_STORAGE_STAGE_OPEN:
        return "open";
    case MODE_CONFIG_STORAGE_STAGE_WRITE:
        return "write";
    case MODE_CONFIG_STORAGE_STAGE_FLUSH:
        return "flush";
    case MODE_CONFIG_STORAGE_STAGE_NONE:
    default:
        return "unknown";
    }
}

static char *config_http_escape_json_string(const char *text)
{
    if (text == NULL) {
        text = "";
    }

    size_t length = 0;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        switch (*cursor) {
        case '\\':
        case '"':
        case '\n':
        case '\r':
        case '\t':
            length += 2;
            break;
        default:
            length += 1;
            break;
        }
    }

    char *buffer = (char *)config_http_calloc_prefer_psram(length + 1);
    if (buffer == NULL) {
        return NULL;
    }

    char *out = buffer;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        switch (*cursor) {
        case '\\':
            *out++ = '\\';
            *out++ = '\\';
            break;
        case '"':
            *out++ = '\\';
            *out++ = '"';
            break;
        case '\n':
            *out++ = '\\';
            *out++ = 'n';
            break;
        case '\r':
            *out++ = '\\';
            *out++ = 'r';
            break;
        case '\t':
            *out++ = '\\';
            *out++ = 't';
            break;
        default:
            *out++ = *cursor;
            break;
        }
    }

    *out = '\0';
    return buffer;
}

static char *config_http_wrap_service_error(const char *status_name, const char *message)
{
    char *escaped_message = config_http_escape_json_string(message);
    if (escaped_message == NULL) {
        return NULL;
    }

    size_t length = strlen(status_name) + strlen(escaped_message) + 64;
    char *buffer = (char *)config_http_calloc_prefer_psram(length);
    if (buffer != NULL) {
        (void)snprintf(buffer,
                       length,
                       "{\"ok\":false,\"status\":\"%s\",\"message\":\"%s\"}",
                       status_name,
                       escaped_message);
    }

    free(escaped_message);
    return buffer;
}

static char *config_http_wrap_json_error(const char *status_name, const mode_json_error_t *error)
{
    if (error == NULL) {
        return config_http_wrap_service_error(status_name, "Unknown JSON error");
    }

    char *escaped_path = config_http_escape_json_string(error->path);
    char *escaped_message = config_http_escape_json_string(error->message);
    if ((escaped_path == NULL) || (escaped_message == NULL)) {
        free(escaped_path);
        free(escaped_message);
        return NULL;
    }

    size_t length = strlen(status_name) + strlen(escaped_path) + strlen(escaped_message) + 160;
    char *buffer = (char *)config_http_calloc_prefer_psram(length);
    if (buffer != NULL) {
        (void)snprintf(buffer,
                       length,
                       "{\"ok\":false,\"status\":\"%s\",\"error\":{\"code\":\"%s\",\"path\":\"%s\",\"message\":\"%s\",\"offset\":%u}}",
                       status_name,
                       config_http_json_error_code_name(error->code),
                       escaped_path,
                       escaped_message,
                       (unsigned)error->offset);
    }

    free(escaped_path);
    free(escaped_message);
    return buffer;
}

static char *config_http_wrap_storage_error(const char *status_name, const mode_config_storage_error_t *error)
{
    if (error == NULL) {
        return config_http_wrap_service_error(status_name, "The firmware reported a storage failure, but no diagnostic details were captured.");
    }

    const char *message = (error->message[0] != '\0')
                              ? error->message
                              : "The firmware could not write the external config file, but it did not capture a more specific storage error.";

    const char *suggestion_a = "Check whether the `storage` SPIFFS partition is present in the flashed partition table and mounted at `/spiffs`.";
    const char *suggestion_b = "If this board is new or was flashed with a different layout before, erase flash or reflash the SPIFFS/storage partition so it can be formatted cleanly.";
    const char *suggestion_c = "If the problem continues, capture the serial log because it should include the exact mount or file I/O error name.";

    switch (error->stage) {
    case MODE_CONFIG_STORAGE_STAGE_OPEN:
        suggestion_a = "The filesystem mounted, but opening `/spiffs/mode-config.json` for writing failed. Check for a damaged SPIFFS image or a path/filesystem mismatch.";
        suggestion_b = "Reformat or reflash the `storage` SPIFFS partition, then try Save again.";
        break;
    case MODE_CONFIG_STORAGE_STAGE_WRITE:
        suggestion_a = "The file opened, but the write did not complete. The SPIFFS partition may be full, unhealthy, or rejecting writes.";
        suggestion_b = "Reformat or reflash the `storage` partition and retry the save.";
        break;
    case MODE_CONFIG_STORAGE_STAGE_FLUSH:
        suggestion_a = "The file write started, but flushing it to flash failed. This often points to filesystem corruption or a flash write problem.";
        suggestion_b = "Reformat or reflash the `storage` partition, then retry Save or Reset.";
        break;
    case MODE_CONFIG_STORAGE_STAGE_MOUNT:
    case MODE_CONFIG_STORAGE_STAGE_NONE:
    default:
        break;
    }

    char *escaped_message = config_http_escape_json_string(message);
    char *escaped_stage = config_http_escape_json_string(config_http_storage_stage_name(error->stage));
    char *escaped_path = config_http_escape_json_string(error->path);
    char *escaped_partition = config_http_escape_json_string(error->partition_label);
    char *escaped_esp_error = config_http_escape_json_string(error->esp_error);
    char *escaped_errno_message = config_http_escape_json_string(error->errno_message);
    char *escaped_suggestion_a = config_http_escape_json_string(suggestion_a);
    char *escaped_suggestion_b = config_http_escape_json_string(suggestion_b);
    char *escaped_suggestion_c = config_http_escape_json_string(suggestion_c);
    if ((escaped_message == NULL) ||
        (escaped_stage == NULL) ||
        (escaped_path == NULL) ||
        (escaped_partition == NULL) ||
        (escaped_esp_error == NULL) ||
        (escaped_errno_message == NULL) ||
        (escaped_suggestion_a == NULL) ||
        (escaped_suggestion_b == NULL) ||
        (escaped_suggestion_c == NULL)) {
        free(escaped_message);
        free(escaped_stage);
        free(escaped_path);
        free(escaped_partition);
        free(escaped_esp_error);
        free(escaped_errno_message);
        free(escaped_suggestion_a);
        free(escaped_suggestion_b);
        free(escaped_suggestion_c);
        return NULL;
    }

    size_t length = strlen(status_name) +
                    strlen(escaped_message) +
                    strlen(escaped_stage) +
                    strlen(escaped_path) +
                    strlen(escaped_partition) +
                    strlen(escaped_esp_error) +
                    strlen(escaped_errno_message) +
                    strlen(escaped_suggestion_a) +
                    strlen(escaped_suggestion_b) +
                    strlen(escaped_suggestion_c) +
                    384;
    char *buffer = (char *)config_http_calloc_prefer_psram(length);
    if (buffer != NULL) {
        (void)snprintf(buffer,
                       length,
                       "{\"ok\":false,\"status\":\"%s\",\"message\":\"%s\",\"detail\":{\"stage\":\"%s\",\"path\":\"%s\",\"partition\":\"%s\",\"formatAttempted\":%s,\"espError\":\"%s\",\"errnoValue\":%d,\"errnoMessage\":\"%s\",\"suggestions\":[\"%s\",\"%s\",\"%s\"]}}",
                       status_name,
                       escaped_message,
                       escaped_stage,
                       escaped_path,
                       escaped_partition,
                       error->format_attempted ? "true" : "false",
                       escaped_esp_error,
                       error->errno_value,
                       escaped_errno_message,
                       escaped_suggestion_a,
                       escaped_suggestion_b,
                       escaped_suggestion_c);
    }

    free(escaped_message);
    free(escaped_stage);
    free(escaped_path);
    free(escaped_partition);
    free(escaped_esp_error);
    free(escaped_errno_message);
    free(escaped_suggestion_a);
    free(escaped_suggestion_b);
    free(escaped_suggestion_c);
    return buffer;
}

static esp_err_t config_http_send_json(httpd_req_t *req, const char *status, const char *body)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_status(req, status);
    return config_http_send_chunked_buffer(req, body, strlen(body));
}

static esp_err_t config_http_send_streamed_config_payload(httpd_req_t *req,
                                                          const char *status,
                                                          const char *fields_prefix,
                                                          const char *canonical_json)
{
    if ((req == NULL) || (status == NULL) || (fields_prefix == NULL) || (canonical_json == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_status(req, status);

    ESP_RETURN_ON_ERROR(httpd_resp_send_chunk(req, "{\"ok\":true,", HTTPD_RESP_USE_STRLEN),
                        TAG, "JSON prefix send failed");
    ESP_RETURN_ON_ERROR(httpd_resp_send_chunk(req, fields_prefix, HTTPD_RESP_USE_STRLEN),
                        TAG, "JSON fields send failed");
    ESP_RETURN_ON_ERROR(httpd_resp_send_chunk(req, "\"config\":", HTTPD_RESP_USE_STRLEN),
                        TAG, "JSON config key send failed");
    ESP_RETURN_ON_ERROR(config_http_send_chunk_sequence(req, canonical_json, strlen(canonical_json)),
                        TAG, "JSON config payload send failed");
    ESP_RETURN_ON_ERROR(httpd_resp_send_chunk(req, "}", HTTPD_RESP_USE_STRLEN),
                        TAG, "JSON suffix send failed");
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t config_http_send_service_result_error(httpd_req_t *req,
                                                       const char *http_status,
                                                       const config_api_service_result_t *result)
{
    char *body = NULL;
    if ((result != NULL) && (result->status == CONFIG_API_SERVICE_PARSE_FAILED)) {
        body = config_http_wrap_json_error(config_http_service_status_name(result->status),
                                           &result->json_error);
    } else if ((result != NULL) && (result->status == CONFIG_API_SERVICE_STORAGE_FAILED)) {
        body = config_http_wrap_storage_error(config_http_service_status_name(result->status),
                                              &result->storage_error);
    } else {
        body = config_http_wrap_service_error((result != NULL)
                                                  ? config_http_service_status_name(result->status)
                                                  : "INTERNAL_ERROR",
                                              "Request failed");
    }

    if (body == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    esp_err_t err = config_http_send_json(req, http_status, body);
    free(body);
    return err;
}

static esp_err_t config_http_read_request_body(httpd_req_t *req, char **out_body)
{
    if ((req == NULL) || (out_body == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    if ((req->content_len <= 0) || (req->content_len > CONFIG_HTTP_MAX_BODY_BYTES)) {
        ESP_LOGW(TAG,
                 "Rejecting request body size %d (max %u)",
                 req->content_len,
                 (unsigned)CONFIG_HTTP_MAX_BODY_BYTES);
        return ESP_ERR_INVALID_SIZE;
    }

    char *buffer = (char *)config_http_calloc_prefer_psram((size_t)req->content_len + 1);
    if (buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    int total_read = 0;
    while (total_read < req->content_len) {
        int read = httpd_req_recv(req, buffer + total_read, req->content_len - total_read);
        if (read <= 0) {
            free(buffer);
            return ESP_FAIL;
        }
        total_read += read;
    }

    buffer[total_read] = '\0';
    *out_body = buffer;
    return ESP_OK;
}

static esp_err_t config_http_handle_root_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return config_http_send_chunked_buffer(req, s_web_ui_html, strlen(s_web_ui_html));
}

static esp_err_t config_http_handle_portal_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return config_http_send_chunked_buffer(req, s_web_ui_html, strlen(s_web_ui_html));
}

static esp_err_t config_http_handle_ping(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/plain");
    return config_http_send_chunked_buffer(req, "ok", 2);
}

static esp_err_t config_http_send_embedded_download(httpd_req_t *req,
                                                    const char *content_type,
                                                    const uint8_t *data_start,
                                                    const uint8_t *data_end)
{
    if ((req == NULL) || (content_type == NULL) || (data_start == NULL) || (data_end == NULL) || (data_end < data_start)) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t data_len = (size_t)(data_end - data_start);
    if ((data_len > 0U) && (data_start[data_len - 1] == '\0')) {
        data_len--;
    }

    httpd_resp_set_type(req, content_type);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return config_http_send_chunked_buffer(req, (const char *)data_start, data_len);
}

static esp_err_t config_http_handle_download_schema(httpd_req_t *req)
{
    return config_http_send_embedded_download(req,
                                              "application/json",
                                              schema_download_start,
                                              schema_download_end);
}

static esp_err_t config_http_handle_download_ai_guide(httpd_req_t *req)
{
    return config_http_send_embedded_download(req,
                                              "text/markdown; charset=utf-8",
                                              ai_guide_download_start,
                                              ai_guide_download_end);
}

static esp_err_t config_http_handle_download_user_guide(httpd_req_t *req)
{
    return config_http_send_embedded_download(req,
                                              "text/markdown; charset=utf-8",
                                              user_guide_download_start,
                                              user_guide_download_end);
}

static esp_err_t config_http_handle_get_config(httpd_req_t *req)
{
    config_api_service_result_t result = {0};
    if (!config_api_service_export_active(&result)) {
        config_api_service_result_clear(&result);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to export config");
    }

    char fields[80];
    (void)snprintf(fields,
                   sizeof(fields),
                   "\"source\":\"%s\",",
                   mode_config_source_name(result.source));
    esp_err_t err = config_http_send_streamed_config_payload(req, "200 OK", fields, result.json_text);
    config_api_service_result_clear(&result);
    return err;
}

static esp_err_t config_http_handle_get_config_canonical(httpd_req_t *req)
{
    config_api_service_result_t result = {0};
    if (!config_api_service_export_active_canonical(&result)) {
        config_api_service_result_clear(&result);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to export canonical config");
    }

    char fields[104];
    (void)snprintf(fields,
                   sizeof(fields),
                   "\"source\":\"%s\",\"format\":\"canonical\",",
                   mode_config_source_name(result.source));
    esp_err_t err = config_http_send_streamed_config_payload(req, "200 OK", fields, result.json_text);
    config_api_service_result_clear(&result);
    return err;
}

static esp_err_t config_http_handle_validate(httpd_req_t *req)
{
    char *request_body = NULL;
    esp_err_t err = config_http_read_request_body(req, &request_body);
    if (err != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected JSON body");
    }

    config_api_service_result_t result = {0};
    bool ok = config_api_service_validate_json(request_body, &result);
    free(request_body);
    if (!ok) {
        esp_err_t response_err = config_http_send_service_result_error(req, "400 Bad Request", &result);
        config_api_service_result_clear(&result);
        return response_err;
    }

    err = config_http_send_streamed_config_payload(req, "200 OK", "\"valid\":true,", result.json_text);
    config_api_service_result_clear(&result);
    return err;
}

static esp_err_t config_http_reload_runtime(void)
{
    if (s_reload_fn == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return s_reload_fn(s_reload_user_data);
}

static esp_err_t config_http_notify_ui(void)
{
    if (s_notify_ui_fn == NULL) {
        return ESP_OK;
    }

    return s_notify_ui_fn(s_notify_ui_user_data);
}

static esp_err_t config_http_handle_put_config(httpd_req_t *req)
{
    char *request_body = NULL;
    esp_err_t err = config_http_read_request_body(req, &request_body);
    if (err != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected JSON body");
    }

    config_api_service_result_t result = {0};
    bool ok = config_api_service_save_json(request_body, &result);
    free(request_body);
    if (!ok) {
        esp_err_t response_err = config_http_send_service_result_error(req, "400 Bad Request", &result);
        config_api_service_result_clear(&result);
        return response_err;
    }
    config_api_service_result_clear(&result);

    if (config_http_reload_runtime() != ESP_OK) {
        char *body = config_http_wrap_service_error("RELOAD_FAILED", "Config was saved but runtime reload failed");
        if (body == NULL) {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        }
        err = config_http_send_json(req, "500 Internal Server Error", body);
        free(body);
        return err;
    }

    if (!config_api_service_export_active(&result)) {
        config_api_service_result_clear(&result);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Config saved but export failed");
    }

    char fields[96];
    (void)snprintf(fields,
                   sizeof(fields),
                   "\"saved\":true,\"source\":\"%s\",",
                   mode_config_source_name(result.source));
    err = config_http_send_streamed_config_payload(req, "200 OK", fields, result.json_text);
    config_api_service_result_clear(&result);
    return err;
}

static esp_err_t config_http_handle_reset(httpd_req_t *req)
{
    config_api_service_result_t result = {0};
    if (!config_api_service_restore_builtin(&result)) {
        esp_err_t response_err = config_http_send_service_result_error(req, "500 Internal Server Error", &result);
        config_api_service_result_clear(&result);
        return response_err;
    }
    config_api_service_result_clear(&result);

    if (config_http_reload_runtime() != ESP_OK) {
        char *body = config_http_wrap_service_error("RELOAD_FAILED", "Built-in config was written but runtime reload failed");
        if (body == NULL) {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        }
        esp_err_t err = config_http_send_json(req, "500 Internal Server Error", body);
        free(body);
        return err;
    }

    if (!config_api_service_export_active(&result)) {
        config_api_service_result_clear(&result);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Built-in config restored but export failed");
    }

    char fields[128];
    (void)snprintf(fields,
                   sizeof(fields),
                   "\"reset\":true,\"resetTarget\":\"builtin\",\"source\":\"%s\",",
                   mode_config_source_name(result.source));
    esp_err_t err = config_http_send_streamed_config_payload(req, "200 OK", fields, result.json_text);
    config_api_service_result_clear(&result);
    if (err == ESP_OK) {
        esp_err_t reload_err = config_http_schedule_wifi_reload();
        if (reload_err != ESP_OK) {
            ESP_LOGW(TAG, "Wi-Fi reload schedule failed after reset: %s", esp_err_to_name(reload_err));
        }
    }
    return err;
}

static esp_err_t config_http_server_init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

static void config_http_copy_string(char *dst, size_t dst_size, const char *value, const char *fallback)
{
    const char *text = ((value != NULL) && (value[0] != '\0')) ? value : fallback;
    if (text == NULL) {
        text = "";
    }

    (void)snprintf(dst, dst_size, "%s", text);
}

static void config_http_set_base_url_from_host(const char *host)
{
    const char *resolved_host = ((host != NULL) && (host[0] != '\0')) ? host : "192.168.4.1";
    (void)snprintf(s_base_url, sizeof(s_base_url), "http://%s/", resolved_host);
}

static void config_http_set_display_address(const char *line)
{
    const char *resolved_line = ((line != NULL) && (line[0] != '\0')) ? line : "192.168.4.1";
    (void)snprintf(s_display_address, sizeof(s_display_address), "%s", resolved_line);
}

static void config_http_set_ap_display_address(const char *ip_address)
{
    const char *resolved_ip = ((ip_address != NULL) && (ip_address[0] != '\0')) ? ip_address : "192.168.4.1";
    const char *resolved_ssid = (s_ap_ssid[0] != '\0') ? s_ap_ssid : CONFIG_HTTP_DEFAULT_AP_SSID;
    (void)snprintf(s_display_address, sizeof(s_display_address), "AP: %s (%s)", resolved_ssid, resolved_ip);
}

static void config_http_load_wifi_config(const mode_wifi_config_t *wifi)
{
    config_http_copy_string(s_sta_ssid,
                            sizeof(s_sta_ssid),
                            (wifi != NULL) ? wifi->sta.ssid : NULL,
                            "");
    config_http_copy_string(s_sta_password,
                            sizeof(s_sta_password),
                            (wifi != NULL) ? wifi->sta.password : NULL,
                            "");
    config_http_copy_string(s_ap_ssid,
                            sizeof(s_ap_ssid),
                            (wifi != NULL) ? wifi->ap.ssid : NULL,
                            CONFIG_HTTP_DEFAULT_AP_SSID);
    config_http_copy_string(s_ap_password,
                            sizeof(s_ap_password),
                            (wifi != NULL) ? wifi->ap.password : NULL,
                            CONFIG_HTTP_DEFAULT_AP_PASSWORD);
    config_http_copy_string(s_hostname,
                            sizeof(s_hostname),
                            (wifi != NULL) ? wifi->hostname : NULL,
                            CONFIG_HTTP_DEFAULT_HOSTNAME);
    config_http_copy_string(s_local_url,
                            sizeof(s_local_url),
                            (wifi != NULL) ? wifi->local_url : NULL,
                            CONFIG_HTTP_DEFAULT_LOCAL_URL);
}

static void config_http_wifi_event_handler(void *arg,
                                           esp_event_base_t event_base,
                                           int32_t event_id,
                                           void *event_data)
{
    (void)arg;

    if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_START)) {
        if (!s_sta_connect_enabled) {
            return;
        }
        s_sta_retry_count = 0;
        (void)esp_wifi_connect();
        return;
    }

    if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_DISCONNECTED)) {
        if (!s_sta_connect_enabled) {
            return;
        }
        if (s_sta_retry_count < CONFIG_HTTP_STA_MAX_RETRIES) {
            s_sta_retry_count++;
            ESP_LOGW(TAG, "Router connect retry %d/%d", s_sta_retry_count, CONFIG_HTTP_STA_MAX_RETRIES);
            (void)esp_wifi_connect();
        } else if (s_wifi_event_group != NULL) {
            xEventGroupSetBits(s_wifi_event_group, CONFIG_HTTP_STA_FAILED_BIT);
        }
        return;
    }

    if ((event_base == IP_EVENT) && (event_id == IP_EVENT_STA_GOT_IP) && (event_data != NULL)) {
        const ip_event_got_ip_t *got_ip = (const ip_event_got_ip_t *)event_data;
        char ip_text[32];
        (void)snprintf(ip_text, sizeof(ip_text), IPSTR, IP2STR(&got_ip->ip_info.ip));
        if (s_local_url[0] != '\0') {
            (void)snprintf(s_display_address, sizeof(s_display_address), "%s (%s)", s_local_url, ip_text);
            config_http_set_base_url_from_host(s_local_url);
        } else {
            config_http_set_display_address(ip_text);
            config_http_set_base_url_from_host(ip_text);
        }
        s_network_mode = CONFIG_HTTP_NETWORK_MODE_STA;
        if (config_http_server_init_mdns(CONFIG_HTTP_NETWORK_MODE_STA) != ESP_OK) {
            ESP_LOGW(TAG, "mDNS HTTP service advertisement failed for STA mode");
        }
        s_sta_retry_count = 0;
        ESP_LOGI(TAG, "Config portal ready on router network: " IPSTR, IP2STR(&got_ip->ip_info.ip));
        if (s_wifi_event_group != NULL) {
            xEventGroupSetBits(s_wifi_event_group, CONFIG_HTTP_STA_CONNECTED_BIT);
        }
        (void)config_http_notify_ui();
    }
}

static esp_err_t config_http_server_init_mdns(config_http_network_mode_t mode)
{
    if (!s_mdns_ready) {
        esp_err_t err = mdns_init();
        if ((err != ESP_OK) && (err != ESP_ERR_INVALID_STATE)) {
            ESP_LOGW(TAG, "mDNS init failed: %s", esp_err_to_name(err));
            return err;
        }
        s_mdns_ready = true;
    }

    if (s_hostname[0] != '\0') {
        esp_err_t err = mdns_hostname_set(s_hostname);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "mDNS hostname set failed: %s", esp_err_to_name(err));
            return err;
        }
    }

    ESP_RETURN_ON_ERROR(mdns_instance_name_set("WalKEY-TalKEY"), TAG, "mDNS instance name set failed");

    esp_err_t err = mdns_service_remove("_http", "_tcp");
    if ((err != ESP_OK) && (err != ESP_ERR_NOT_FOUND) && (err != ESP_ERR_INVALID_STATE)) {
        ESP_LOGW(TAG, "mDNS HTTP remove failed: %s", esp_err_to_name(err));
    }

    mdns_txt_item_t service_txt[] = {
        { "path", "/" },
        { "mode", (mode == CONFIG_HTTP_NETWORK_MODE_STA) ? "sta" : "ap" },
    };
    ESP_RETURN_ON_ERROR(mdns_service_add(NULL, "_http", "_tcp", 80, service_txt, 2),
                        TAG,
                        "mDNS HTTP add failed");

    err = mdns_service_instance_name_set("_http", "_tcp", "WalKEY-TalKEY Config");
    if ((err != ESP_OK) && (err != ESP_ERR_NOT_FOUND)) {
        ESP_LOGW(TAG, "mDNS HTTP instance name set failed: %s", esp_err_to_name(err));
    }

    return ESP_OK;
}

static esp_err_t config_http_server_init_wifi_stack(void)
{
    if (s_wifi_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(config_http_server_init_nvs(), TAG, "NVS init failed");

    esp_err_t err = esp_netif_init();
    if ((err != ESP_OK) && (err != ESP_ERR_INVALID_STATE)) {
        return err;
    }

    err = esp_event_loop_create_default();
    if ((err != ESP_OK) && (err != ESP_ERR_INVALID_STATE)) {
        return err;
    }

    if (s_wifi_event_group == NULL) {
        s_wifi_event_group = xEventGroupCreate();
        if (s_wifi_event_group == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (s_wifi_sta_netif == NULL) {
        s_wifi_sta_netif = esp_netif_create_default_wifi_sta();
        if (s_wifi_sta_netif == NULL) {
            return ESP_FAIL;
        }
    }

    if (s_wifi_ap_netif == NULL) {
        s_wifi_ap_netif = esp_netif_create_default_wifi_ap();
        if (s_wifi_ap_netif == NULL) {
            return ESP_FAIL;
        }
    }

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_cfg), TAG, "Wi-Fi init failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "Wi-Fi storage config failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &config_http_wifi_event_handler,
                                                            NULL,
                                                            &s_wifi_event_handler_instance),
                        TAG,
                        "Wi-Fi event register failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &config_http_wifi_event_handler,
                                                            NULL,
                                                            &s_ip_event_handler_instance),
                        TAG,
                        "IP event register failed");
    s_wifi_initialized = true;
    return ESP_OK;
}

static esp_err_t config_http_server_start_sta(void)
{
    if (s_sta_ssid[0] == '\0') {
        return ESP_ERR_NOT_FOUND;
    }

    if (s_wifi_sta_netif != NULL) {
        (void)esp_netif_set_hostname(s_wifi_sta_netif, s_hostname);
    }

    wifi_config_t sta_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_OPEN,
        },
    };
    (void)snprintf((char *)sta_config.sta.ssid, sizeof(sta_config.sta.ssid), "%s", s_sta_ssid);
    (void)snprintf((char *)sta_config.sta.password, sizeof(sta_config.sta.password), "%s", s_sta_password);

    xEventGroupClearBits(s_wifi_event_group, CONFIG_HTTP_STA_CONNECTED_BIT | CONFIG_HTTP_STA_FAILED_BIT);
    s_sta_retry_count = 0;
    s_sta_connect_enabled = true;
    s_network_mode = CONFIG_HTTP_NETWORK_MODE_NONE;
    config_http_set_display_address("Connecting...");
    (void)config_http_notify_ui();

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Wi-Fi STA mode set failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &sta_config), TAG, "STA config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi STA start failed");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           CONFIG_HTTP_STA_CONNECTED_BIT | CONFIG_HTTP_STA_FAILED_BIT,
                                           pdTRUE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(CONFIG_HTTP_STA_CONNECT_TIMEOUT_MS));
    if ((bits & CONFIG_HTTP_STA_CONNECTED_BIT) != 0) {
        s_wifi_ready = true;
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Router Wi-Fi connection failed for SSID=%s, falling back to SoftAP", s_sta_ssid);
    s_sta_connect_enabled = false;
    (void)esp_wifi_stop();
    return ESP_FAIL;
}

static esp_err_t config_http_server_start_ap(void)
{
    s_sta_connect_enabled = false;
    wifi_config_t ap_config = {
        .ap = {
            .channel = 1,
            .max_connection = 4,
            .authmode = (s_ap_password[0] != '\0') ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN,
        },
    };
    (void)snprintf((char *)ap_config.ap.ssid, sizeof(ap_config.ap.ssid), "%s", s_ap_ssid);
    (void)snprintf((char *)ap_config.ap.password, sizeof(ap_config.ap.password), "%s", s_ap_password);
    ap_config.ap.ssid_len = (uint8_t)strlen(s_ap_ssid);

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "Wi-Fi AP mode set failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap_config), TAG, "AP config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi AP start failed");

    config_http_set_ap_display_address("192.168.4.1");
    config_http_set_base_url_from_host("192.168.4.1");

    esp_netif_ip_info_t ip_info = {0};
    if (esp_netif_get_ip_info(s_wifi_ap_netif, &ip_info) == ESP_OK) {
        char ip_text[32];
        (void)snprintf(ip_text, sizeof(ip_text), IPSTR, IP2STR(&ip_info.ip));
        config_http_set_ap_display_address(ip_text);
        config_http_set_base_url_from_host(ip_text);
        ESP_LOGI(TAG,
                 "Fallback config portal ready: SSID=%s password=%s URL=%s",
                 s_ap_ssid,
                 s_ap_password,
                 s_base_url);
        ESP_LOGI(TAG, "Fallback AP IP: " IPSTR, IP2STR(&ip_info.ip));
    }

    s_network_mode = CONFIG_HTTP_NETWORK_MODE_AP;
    if (config_http_server_init_mdns(CONFIG_HTTP_NETWORK_MODE_AP) != ESP_OK) {
        ESP_LOGW(TAG, "mDNS HTTP service advertisement failed for AP mode");
    }
    s_wifi_ready = true;
    (void)config_http_notify_ui();
    return ESP_OK;
}

static void config_http_server_stop_wifi(void)
{
    if (!s_wifi_initialized) {
        return;
    }

    s_sta_connect_enabled = false;
    (void)esp_wifi_disconnect();
    esp_err_t err = esp_wifi_stop();
    if ((err != ESP_OK) && (err != ESP_ERR_WIFI_NOT_INIT) && (err != ESP_ERR_WIFI_NOT_STARTED)) {
        ESP_LOGW(TAG, "Wi-Fi stop failed: %s", esp_err_to_name(err));
    }

    if (s_wifi_event_group != NULL) {
        xEventGroupClearBits(s_wifi_event_group, CONFIG_HTTP_STA_CONNECTED_BIT | CONFIG_HTTP_STA_FAILED_BIT);
    }
    s_network_mode = CONFIG_HTTP_NETWORK_MODE_NONE;
    s_wifi_ready = false;
    s_sta_retry_count = 0;
}

static esp_err_t config_http_server_apply_wifi(const mode_wifi_config_t *wifi)
{
    config_http_load_wifi_config(wifi);
    ESP_RETURN_ON_ERROR(config_http_server_init_wifi_stack(), TAG, "Wi-Fi stack init failed");

    config_http_server_stop_wifi();

    if (config_http_server_start_sta() == ESP_OK) {
        return ESP_OK;
    }

    return config_http_server_start_ap();
}

static void config_http_wifi_reload_task(void *arg)
{
    (void)arg;

    do {
        s_wifi_reload_pending = false;
        const mode_config_t *mode_config = mode_config_get();
        const mode_wifi_config_t *wifi = (mode_config != NULL) ? &mode_config->wifi : NULL;
        esp_err_t err = config_http_server_apply_wifi(wifi);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Deferred Wi-Fi reload failed: %s", esp_err_to_name(err));
        }
        (void)config_http_notify_ui();
    } while (s_wifi_reload_pending);

    s_wifi_reload_task = NULL;
    vTaskDelete(NULL);
}

static esp_err_t config_http_schedule_wifi_reload(void)
{
    s_wifi_reload_pending = true;
    if (s_wifi_reload_task != NULL) {
        return ESP_OK;
    }

    BaseType_t task_created = xTaskCreate(config_http_wifi_reload_task,
                                          "cfg_wifi_reload",
                                          CONFIG_HTTP_WIFI_RELOAD_TASK_STACK_WORDS,
                                          NULL,
                                          CONFIG_HTTP_WIFI_RELOAD_TASK_PRIORITY,
                                          &s_wifi_reload_task);
    if (task_created != pdPASS) {
        s_wifi_reload_pending = false;
        s_wifi_reload_task = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

/* ---------------------------------------------------------------------------
 * Granular REST API helpers
 * ------------------------------------------------------------------------ */

static cJSON *api_load_config_root(void)
{
    char *json = NULL;
    mode_config_source_t src;
    if (!mode_config_read_active_json(&json, &src)) {
        return NULL;
    }
    cJSON *root = cJSON_Parse(json);
    free(json);
    return root;
}

static esp_err_t api_save_and_reload(httpd_req_t *req, cJSON *root)
{
    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!text) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON print failed");
    }

    config_api_service_result_t result = {0};
    bool ok = config_api_service_save_json(text, &result);
    free(text);
    if (!ok) {
        esp_err_t err = config_http_send_service_result_error(req, "400 Bad Request", &result);
        config_api_service_result_clear(&result);
        return err;
    }
    config_api_service_result_clear(&result);

    if (config_http_reload_runtime() != ESP_OK) {
        ESP_LOGW(TAG, "Config saved but runtime reload failed");
    }
    return ESP_OK;
}

static esp_err_t api_send_cjson(httpd_req_t *req, cJSON *obj)
{
    char *text = cJSON_PrintUnformatted(obj);
    if (!text) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON print failed");
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = config_http_send_chunked_buffer(req, text, strlen(text));
    cJSON_free(text);
    return err;
}

static esp_err_t api_get_query_id(httpd_req_t *req, char *buf, size_t buf_len)
{
    size_t qlen = httpd_req_get_url_query_len(req);
    if (qlen == 0 || qlen >= 128) {
        return ESP_ERR_NOT_FOUND;
    }
    char query[128];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }
    if (httpd_query_key_value(query, "id", buf, buf_len) != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}

/* ---------------------------------------------------------------------------
 * GET /api/modes -- list all modes summary
 * ------------------------------------------------------------------------ */

static esp_err_t config_http_handle_api_get_modes(httpd_req_t *req)
{
    cJSON *root = api_load_config_root();
    if (!root) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read config");
    }

    cJSON *modes = cJSON_GetObjectItem(root, "modes");
    cJSON *result = cJSON_CreateArray();
    if (modes && cJSON_IsArray(modes)) {
        cJSON *m;
        cJSON_ArrayForEach(m, modes) {
            cJSON *entry = cJSON_CreateObject();
            cJSON *id = cJSON_GetObjectItem(m, "id");
            cJSON *label = cJSON_GetObjectItem(m, "label");
            cJSON *order = cJSON_GetObjectItem(m, "cycleOrder");
            cJSON *bindings = cJSON_GetObjectItem(m, "bindings");
            if (id) cJSON_AddStringToObject(entry, "id", id->valuestring ? id->valuestring : "");
            if (label) cJSON_AddStringToObject(entry, "label", label->valuestring ? label->valuestring : "");
            if (order) cJSON_AddNumberToObject(entry, "cycleOrder", order->valuedouble);
            cJSON_AddNumberToObject(entry, "bindingCount",
                                    bindings && cJSON_IsArray(bindings) ? cJSON_GetArraySize(bindings) : 0);
            cJSON_AddItemToArray(result, entry);
        }
    }

    cJSON_Delete(root);
    esp_err_t err = api_send_cjson(req, result);
    cJSON_Delete(result);
    return err;
}

/* ---------------------------------------------------------------------------
 * GET /api/mode?id=X -- read single mode
 * ------------------------------------------------------------------------ */

static esp_err_t config_http_handle_api_get_mode(httpd_req_t *req)
{
    char mode_id[64];
    if (api_get_query_id(req, mode_id, sizeof(mode_id)) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ?id= parameter");
    }

    cJSON *root = api_load_config_root();
    if (!root) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read config");
    }

    cJSON *modes = cJSON_GetObjectItem(root, "modes");
    cJSON *found = NULL;
    if (modes && cJSON_IsArray(modes)) {
        cJSON *m;
        cJSON_ArrayForEach(m, modes) {
            cJSON *id = cJSON_GetObjectItem(m, "id");
            if (id && id->valuestring && strcmp(id->valuestring, mode_id) == 0) {
                found = cJSON_Duplicate(m, 1);
                break;
            }
        }
    }
    cJSON_Delete(root);

    if (!found) {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Mode not found");
    }

    esp_err_t err = api_send_cjson(req, found);
    cJSON_Delete(found);
    return err;
}

/* ---------------------------------------------------------------------------
 * PUT /api/mode?id=X -- replace single mode
 * ------------------------------------------------------------------------ */

static esp_err_t config_http_handle_api_put_mode(httpd_req_t *req)
{
    char mode_id[64];
    if (api_get_query_id(req, mode_id, sizeof(mode_id)) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ?id= parameter");
    }

    char *body = NULL;
    if (config_http_read_request_body(req, &body) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected JSON body");
    }
    cJSON *new_mode = cJSON_Parse(body);
    free(body);
    if (!new_mode) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    cJSON *root = api_load_config_root();
    if (!root) {
        cJSON_Delete(new_mode);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read config");
    }

    cJSON *modes = cJSON_GetObjectItem(root, "modes");
    bool replaced = false;
    if (modes && cJSON_IsArray(modes)) {
        int idx = 0;
        cJSON *m;
        cJSON_ArrayForEach(m, modes) {
            cJSON *id = cJSON_GetObjectItem(m, "id");
            if (id && id->valuestring && strcmp(id->valuestring, mode_id) == 0) {
                cJSON_ReplaceItemInArray(modes, idx, new_mode);
                replaced = true;
                break;
            }
            idx++;
        }
    }

    if (!replaced) {
        cJSON_Delete(new_mode);
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Mode not found");
    }

    esp_err_t err = api_save_and_reload(req, root);
    if (err != ESP_OK) return err;
    return config_http_send_json(req, "200 OK", "{\"ok\":true,\"updated\":true}");
}

/* ---------------------------------------------------------------------------
 * POST /api/mode -- add new mode
 * ------------------------------------------------------------------------ */

static esp_err_t config_http_handle_api_post_mode(httpd_req_t *req)
{
    char *body = NULL;
    if (config_http_read_request_body(req, &body) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected JSON body");
    }
    cJSON *new_mode = cJSON_Parse(body);
    free(body);
    if (!new_mode) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    cJSON *root = api_load_config_root();
    if (!root) {
        cJSON_Delete(new_mode);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read config");
    }

    cJSON *modes = cJSON_GetObjectItem(root, "modes");
    if (!modes || !cJSON_IsArray(modes)) {
        modes = cJSON_AddArrayToObject(root, "modes");
    }
    cJSON_AddItemToArray(modes, new_mode);

    esp_err_t err = api_save_and_reload(req, root);
    if (err != ESP_OK) return err;
    return config_http_send_json(req, "200 OK", "{\"ok\":true,\"created\":true}");
}

/* ---------------------------------------------------------------------------
 * DELETE /api/mode?id=X -- remove mode
 * ------------------------------------------------------------------------ */

static esp_err_t config_http_handle_api_delete_mode(httpd_req_t *req)
{
    char mode_id[64];
    if (api_get_query_id(req, mode_id, sizeof(mode_id)) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ?id= parameter");
    }

    cJSON *root = api_load_config_root();
    if (!root) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read config");
    }

    cJSON *active = cJSON_GetObjectItem(root, "activeMode");
    if (active && active->valuestring && strcmp(active->valuestring, mode_id) == 0) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Cannot delete the active mode");
    }

    cJSON *modes = cJSON_GetObjectItem(root, "modes");
    bool deleted = false;
    if (modes && cJSON_IsArray(modes)) {
        int idx = 0;
        cJSON *m;
        cJSON_ArrayForEach(m, modes) {
            cJSON *id = cJSON_GetObjectItem(m, "id");
            if (id && id->valuestring && strcmp(id->valuestring, mode_id) == 0) {
                cJSON_DeleteItemFromArray(modes, idx);
                deleted = true;
                break;
            }
            idx++;
        }
    }

    if (!deleted) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Mode not found");
    }

    esp_err_t err = api_save_and_reload(req, root);
    if (err != ESP_OK) return err;
    return config_http_send_json(req, "200 OK", "{\"ok\":true,\"deleted\":true}");
}

/* ---------------------------------------------------------------------------
 * GET /api/wifi -- read wifi config
 * ------------------------------------------------------------------------ */

static esp_err_t config_http_handle_api_get_wifi(httpd_req_t *req)
{
    cJSON *root = api_load_config_root();
    if (!root) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read config");
    }
    cJSON *wifi = cJSON_DetachItemFromObject(root, "wifi");
    cJSON_Delete(root);
    if (!wifi) {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "No wifi section");
    }
    esp_err_t err = api_send_cjson(req, wifi);
    cJSON_Delete(wifi);
    return err;
}

/* ---------------------------------------------------------------------------
 * PUT /api/wifi -- update wifi config (merge)
 * ------------------------------------------------------------------------ */

static esp_err_t config_http_handle_api_put_wifi(httpd_req_t *req)
{
    char *body = NULL;
    if (config_http_read_request_body(req, &body) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected JSON body");
    }
    cJSON *patch = cJSON_Parse(body);
    free(body);
    if (!patch) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    cJSON *root = api_load_config_root();
    if (!root) {
        cJSON_Delete(patch);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read config");
    }

    cJSON *wifi = cJSON_GetObjectItem(root, "wifi");
    if (!wifi) {
        cJSON_AddItemToObject(root, "wifi", patch);
    } else {
        cJSON *child = patch->child;
        while (child) {
            cJSON *next = child->next;
            cJSON *dup = cJSON_Duplicate(child, 1);
            cJSON_DeleteItemFromObject(wifi, child->string);
            cJSON_AddItemToObject(wifi, child->string, dup);
            child = next;
        }
        cJSON_Delete(patch);
    }

    esp_err_t err = api_save_and_reload(req, root);
    if (err != ESP_OK) return err;
    return config_http_send_json(req, "200 OK", "{\"ok\":true,\"updated\":true}");
}

/* ---------------------------------------------------------------------------
 * GET /api/defaults -- read touch defaults
 * ------------------------------------------------------------------------ */

static esp_err_t config_http_handle_api_get_defaults(httpd_req_t *req)
{
    cJSON *root = api_load_config_root();
    if (!root) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read config");
    }
    cJSON *defaults = cJSON_DetachItemFromObject(root, "defaults");
    cJSON_Delete(root);
    if (!defaults) {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "No defaults section");
    }
    esp_err_t err = api_send_cjson(req, defaults);
    cJSON_Delete(defaults);
    return err;
}

/* ---------------------------------------------------------------------------
 * PUT /api/defaults -- update touch defaults (merge)
 * ------------------------------------------------------------------------ */

static esp_err_t config_http_handle_api_put_defaults(httpd_req_t *req)
{
    char *body = NULL;
    if (config_http_read_request_body(req, &body) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected JSON body");
    }
    cJSON *patch = cJSON_Parse(body);
    free(body);
    if (!patch) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    cJSON *root = api_load_config_root();
    if (!root) {
        cJSON_Delete(patch);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read config");
    }

    cJSON *defaults = cJSON_GetObjectItem(root, "defaults");
    if (!defaults) {
        cJSON_AddItemToObject(root, "defaults", patch);
    } else {
        cJSON *child = patch->child;
        while (child) {
            cJSON *next = child->next;
            cJSON *dup = cJSON_Duplicate(child, 1);
            cJSON_DeleteItemFromObject(defaults, child->string);
            cJSON_AddItemToObject(defaults, child->string, dup);
            child = next;
        }
        cJSON_Delete(patch);
    }

    esp_err_t err = api_save_and_reload(req, root);
    if (err != ESP_OK) return err;
    return config_http_send_json(req, "200 OK", "{\"ok\":true,\"updated\":true}");
}

/* ---------------------------------------------------------------------------
 * PUT /api/active-mode -- set active mode
 * ------------------------------------------------------------------------ */

static esp_err_t config_http_handle_api_put_active_mode(httpd_req_t *req)
{
    char *body = NULL;
    if (config_http_read_request_body(req, &body) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected JSON body");
    }
    cJSON *patch = cJSON_Parse(body);
    free(body);
    if (!patch) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    cJSON *am = cJSON_GetObjectItem(patch, "activeMode");
    if (!am || !cJSON_IsString(am)) {
        cJSON_Delete(patch);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected {\"activeMode\":\"...\"}");
    }

    cJSON *root = api_load_config_root();
    if (!root) {
        cJSON_Delete(patch);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read config");
    }

    cJSON_DeleteItemFromObject(root, "activeMode");
    cJSON_AddStringToObject(root, "activeMode", am->valuestring);
    cJSON_Delete(patch);

    esp_err_t err = api_save_and_reload(req, root);
    if (err != ESP_OK) return err;
    return config_http_send_json(req, "200 OK", "{\"ok\":true,\"updated\":true}");
}

/* ---------------------------------------------------------------------------
 * GET /api/boot-mode -- read boot mode definition
 * ------------------------------------------------------------------------ */

static esp_err_t config_http_handle_api_get_boot_mode(httpd_req_t *req)
{
    cJSON *root = api_load_config_root();
    if (!root) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read config");
    }
    cJSON *bm = cJSON_DetachItemFromObject(root, "bootMode");
    cJSON_Delete(root);
    if (!bm) {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "No bootMode section");
    }
    esp_err_t err = api_send_cjson(req, bm);
    cJSON_Delete(bm);
    return err;
}

/* ---------------------------------------------------------------------------
 * GET /api/global-bindings -- read global bindings
 * ------------------------------------------------------------------------ */

static esp_err_t config_http_handle_api_get_global_bindings(httpd_req_t *req)
{
    cJSON *root = api_load_config_root();
    if (!root) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read config");
    }
    cJSON *gb = cJSON_DetachItemFromObject(root, "globalBindings");
    cJSON_Delete(root);
    if (!gb) {
        gb = cJSON_CreateArray();
    }
    esp_err_t err = api_send_cjson(req, gb);
    cJSON_Delete(gb);
    return err;
}

esp_err_t config_http_server_start(config_http_reload_fn_t reload_fn,
                                   void *reload_user_data,
                                   config_http_notify_fn_t notify_ui_fn,
                                   void *notify_ui_user_data)
{
    if (s_http_server != NULL) {
        return ESP_OK;
    }

    s_reload_fn = reload_fn;
    s_reload_user_data = reload_user_data;
    s_notify_ui_fn = notify_ui_fn;
    s_notify_ui_user_data = notify_ui_user_data;

    const mode_config_t *mode_config = mode_config_get();
    const mode_wifi_config_t *wifi = (mode_config != NULL) ? &mode_config->wifi : NULL;
    ESP_RETURN_ON_ERROR(config_http_server_apply_wifi(wifi), TAG, "Wi-Fi init failed");

    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    server_config.max_uri_handlers = 24;
    server_config.stack_size = CONFIG_HTTP_SERVER_STACK_SIZE;

    ESP_RETURN_ON_ERROR(httpd_start(&s_http_server, &server_config), TAG, "HTTP server start failed");

    static const httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = config_http_handle_root_get,
        .user_ctx = NULL,
    };
    static const httpd_uri_t ping_uri = {
        .uri = "/ping",
        .method = HTTP_GET,
        .handler = config_http_handle_ping,
        .user_ctx = NULL,
    };
    static const httpd_uri_t portal_uri = {
        .uri = "/portal",
        .method = HTTP_GET,
        .handler = config_http_handle_portal_get,
        .user_ctx = NULL,
    };
    static const httpd_uri_t get_config_uri = {
        .uri = "/config",
        .method = HTTP_GET,
        .handler = config_http_handle_get_config,
        .user_ctx = NULL,
    };
    static const httpd_uri_t get_config_canonical_uri = {
        .uri = "/config/canonical",
        .method = HTTP_GET,
        .handler = config_http_handle_get_config_canonical,
        .user_ctx = NULL,
    };
    static const httpd_uri_t put_config_uri = {
        .uri = "/config",
        .method = HTTP_PUT,
        .handler = config_http_handle_put_config,
        .user_ctx = NULL,
    };
    static const httpd_uri_t validate_uri = {
        .uri = "/config/validate",
        .method = HTTP_POST,
        .handler = config_http_handle_validate,
        .user_ctx = NULL,
    };
    static const httpd_uri_t reset_uri = {
        .uri = "/config/reset",
        .method = HTTP_POST,
        .handler = config_http_handle_reset,
        .user_ctx = NULL,
    };
    static const httpd_uri_t download_schema_uri = {
        .uri = "/downloads/mode-config.schema.json",
        .method = HTTP_GET,
        .handler = config_http_handle_download_schema,
        .user_ctx = NULL,
    };
    static const httpd_uri_t download_ai_guide_uri = {
        .uri = "/downloads/AI_GUIDE.md",
        .method = HTTP_GET,
        .handler = config_http_handle_download_ai_guide,
        .user_ctx = NULL,
    };
    static const httpd_uri_t download_user_guide_uri = {
        .uri = "/downloads/USER_GUIDE.md",
        .method = HTTP_GET,
        .handler = config_http_handle_download_user_guide,
        .user_ctx = NULL,
    };

    static const httpd_uri_t api_get_modes_uri = { .uri = "/api/modes", .method = HTTP_GET, .handler = config_http_handle_api_get_modes };
    static const httpd_uri_t api_get_mode_uri = { .uri = "/api/mode", .method = HTTP_GET, .handler = config_http_handle_api_get_mode };
    static const httpd_uri_t api_put_mode_uri = { .uri = "/api/mode", .method = HTTP_PUT, .handler = config_http_handle_api_put_mode };
    static const httpd_uri_t api_post_mode_uri = { .uri = "/api/mode", .method = HTTP_POST, .handler = config_http_handle_api_post_mode };
    static const httpd_uri_t api_delete_mode_uri = { .uri = "/api/mode", .method = HTTP_DELETE, .handler = config_http_handle_api_delete_mode };
    static const httpd_uri_t api_get_wifi_uri = { .uri = "/api/wifi", .method = HTTP_GET, .handler = config_http_handle_api_get_wifi };
    static const httpd_uri_t api_put_wifi_uri = { .uri = "/api/wifi", .method = HTTP_PUT, .handler = config_http_handle_api_put_wifi };
    static const httpd_uri_t api_get_defaults_uri = { .uri = "/api/defaults", .method = HTTP_GET, .handler = config_http_handle_api_get_defaults };
    static const httpd_uri_t api_put_defaults_uri = { .uri = "/api/defaults", .method = HTTP_PUT, .handler = config_http_handle_api_put_defaults };
    static const httpd_uri_t api_put_active_mode_uri = { .uri = "/api/active-mode", .method = HTTP_PUT, .handler = config_http_handle_api_put_active_mode };
    static const httpd_uri_t api_get_boot_mode_uri = { .uri = "/api/boot-mode", .method = HTTP_GET, .handler = config_http_handle_api_get_boot_mode };
    static const httpd_uri_t api_get_global_bindings_uri = { .uri = "/api/global-bindings", .method = HTTP_GET, .handler = config_http_handle_api_get_global_bindings };

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &root_uri), TAG, "Register / failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &ping_uri), TAG, "Register /ping failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &portal_uri), TAG, "Register /portal failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &get_config_uri), TAG, "Register GET /config failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &get_config_canonical_uri), TAG, "Register GET /config/canonical failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &put_config_uri), TAG, "Register PUT /config failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &validate_uri), TAG, "Register /config/validate failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &reset_uri), TAG, "Register /config/reset failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &download_schema_uri), TAG, "Register schema download failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &download_ai_guide_uri), TAG, "Register AI_GUIDE download failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &download_user_guide_uri), TAG, "Register USER_GUIDE download failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &api_get_modes_uri), TAG, "Register GET /api/modes failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &api_get_mode_uri), TAG, "Register GET /api/mode failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &api_put_mode_uri), TAG, "Register PUT /api/mode failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &api_post_mode_uri), TAG, "Register POST /api/mode failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &api_delete_mode_uri), TAG, "Register DELETE /api/mode failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &api_get_wifi_uri), TAG, "Register GET /api/wifi failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &api_put_wifi_uri), TAG, "Register PUT /api/wifi failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &api_get_defaults_uri), TAG, "Register GET /api/defaults failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &api_put_defaults_uri), TAG, "Register PUT /api/defaults failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &api_put_active_mode_uri), TAG, "Register PUT /api/active-mode failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &api_get_boot_mode_uri), TAG, "Register GET /api/boot-mode failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_http_server, &api_get_global_bindings_uri), TAG, "Register GET /api/global-bindings failed");

    return ESP_OK;
}

esp_err_t config_http_server_stop(void)
{
    if (s_http_server == NULL) {
        return ESP_OK;
    }

    esp_err_t err = httpd_stop(s_http_server);
    s_http_server = NULL;
    return err;
}

const char *config_http_server_ap_ssid(void)
{
    return s_ap_ssid;
}

const char *config_http_server_ap_password(void)
{
    return s_ap_password;
}

const char *config_http_server_base_url(void)
{
    return s_base_url;
}

const char *config_http_server_display_address(void)
{
    return s_display_address;
}

bool config_http_server_uses_sta(void)
{
    return s_network_mode == CONFIG_HTTP_NETWORK_MODE_STA;
}
