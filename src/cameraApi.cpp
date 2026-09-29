// Small versioned JSON API layered on top of the upstream web server.

#include "appGlobals.h"

#if defined(CAMERA_MODEL_XIAO_ESP32S3)
#define API_BOARD "XIAO_ESP32S3_SENSE"
#else
#define API_BOARD CAM_BOARD
#endif

static esp_err_t sendApiJson(httpd_req_t* req, const char* json) {
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_sendstr(req, json);
}

static esp_err_t sendApiError(httpd_req_t* req, const char* status,
                              const char* code, const char* message) {
  httpd_resp_set_status(req, status);
  char json[192];
  snprintf(json, sizeof(json),
           "{\"success\":false,\"error\":{\"code\":\"%s\",\"message\":\"%s\"}}",
           code, message);
  return sendApiJson(req, json);
}

static bool previewRequested = false;

static esp_err_t apiPreviewStartHandler(httpd_req_t* req) {
  if (isCapturing) {
    return sendApiError(req, "409 Conflict", "CAMERA_BUSY",
                        "Preview cannot be started while recording.");
  }
  previewRequested = true;
  return sendApiJson(req,
                    "{\"success\":true,\"state\":\"preview\",\"streamUrl\":\"/stream\"}");
}

static esp_err_t apiPreviewStopHandler(httpd_req_t* req) {
  previewRequested = false;
  stopLiveStream();
  return sendApiJson(req, "{\"success\":true,\"state\":\"idle\"}");
}

static esp_err_t apiStreamHandler(httpd_req_t* req) {
  if (!previewRequested) {
    return sendApiError(req, "409 Conflict", "PREVIEW_NOT_ACTIVE",
                        "Preview must be started before connecting to the stream.");
  }
  return appSpecificSustainHandler(req);
}

static esp_err_t apiInfoHandler(httpd_req_t* req) {
  bool storagePresent = false;
  uint64_t totalBytes = 0;
  uint64_t freeBytes = 0;

#ifndef NO_SD
  storagePresent = SD_MMC.cardType() != CARD_NONE;
  if (storagePresent) {
    totalBytes = STORAGE.totalBytes();
    freeBytes = totalBytes - STORAGE.usedBytes();
  }
#endif

  char json[384];
  snprintf(json, sizeof(json),
           "{\"apiVersion\":\"1\",\"firmwareVersion\":\"%s\","
           "\"deviceId\":\"camera-%012llX\",\"cameraModel\":\"%s\","
           "\"board\":\"%s\",\"storage\":{\"present\":%s,"
           "\"totalBytes\":%llu,\"freeBytes\":%llu}}",
           APP_VER,
           (unsigned long long)ESP.getEfuseMac(),
           camModel,
           API_BOARD,
           storagePresent ? "true" : "false",
           (unsigned long long)totalBytes,
           (unsigned long long)freeBytes);
  return sendApiJson(req, json);
}

static esp_err_t apiStatusHandler(httpd_req_t* req) {
  const bool recordingActive = isCapturing;
  // A start request is an enabled/requested preview even before its demand-driven
  // GET /stream client connects. Also report an existing upstream live stream.
  const bool previewActive = !recordingActive && (previewRequested || isLiveStreamActive());
  const char* state = recordingActive ? "recording" : previewActive ? "preview" : "idle";

  char json[160];
  snprintf(json, sizeof(json),
           "{\"state\":\"%s\",\"previewActive\":%s,"
           "\"recordingActive\":%s,\"recording\":null}",
           state,
           previewActive ? "true" : "false",
           recordingActive ? "true" : "false");
  return sendApiJson(req, json);
}

esp_err_t registerCameraApi(httpd_handle_t server) {
  httpd_uri_t infoUri = {.uri = "/api/v1/info", .method = HTTP_GET, .handler = apiInfoHandler, .user_ctx = NULL};
  httpd_uri_t apiStatusUri = {.uri = "/api/v1/status", .method = HTTP_GET, .handler = apiStatusHandler, .user_ctx = NULL};
  httpd_uri_t previewStartUri = {.uri = "/api/v1/preview/start", .method = HTTP_POST, .handler = apiPreviewStartHandler, .user_ctx = NULL};
  httpd_uri_t previewStopUri = {.uri = "/api/v1/preview/stop", .method = HTTP_POST, .handler = apiPreviewStopHandler, .user_ctx = NULL};
  httpd_uri_t streamUri = {.uri = "/stream", .method = HTTP_GET, .handler = apiStreamHandler, .user_ctx = NULL};

  esp_err_t res = httpd_register_uri_handler(server, &infoUri);
  if (res == ESP_OK) res = httpd_register_uri_handler(server, &apiStatusUri);
  if (res == ESP_OK) res = httpd_register_uri_handler(server, &previewStartUri);
  if (res == ESP_OK) res = httpd_register_uri_handler(server, &previewStopUri);
  if (res == ESP_OK) res = httpd_register_uri_handler(server, &streamUri);
  return res;
}
