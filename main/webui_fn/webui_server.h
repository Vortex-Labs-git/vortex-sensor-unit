#ifndef WEBUI_SERVER_H
#define WEBUI_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief Register the browser configuration UI on an already running server.
 *
 * Must be called AFTER httpd_start() succeeds. Adds a single GET handler
 * at "/" that serves the embedded page; the WebSocket at "/ws" is
 * registered separately and is untouched by this.
 *
 * @param server Handle returned by httpd_start().
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for a NULL handle,
 *         or the error from httpd_register_uri_handler().
 */
esp_err_t webui_register(httpd_handle_t server);

#endif // WEBUI_SERVER_H
