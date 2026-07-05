#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "StorageManager.h"

class SdStaticHandler : public AsyncWebHandler {
public:
    explicit SdStaticHandler(StorageManager* storage);

    bool canHandle(AsyncWebServerRequest* request) const override;
    void handleRequest(AsyncWebServerRequest* request) override;

private:
    StorageManager* _storage = nullptr;

    static bool mapRequestPath(const String& requestPath, String& sdPath);
    static const char* contentTypeForPath(const String& path);
};
