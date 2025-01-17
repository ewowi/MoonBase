/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2024 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <ESP32SvelteKit.h>

ESP32SvelteKit::ESP32SvelteKit(PsychicHttpServer *server, unsigned int numberEndpoints) : _server(server),
                                                                                          _numberEndpoints(numberEndpoints),
                                                                                          _featureService(server),
                                                                                          _securitySettingsService(server, &ESPFS),
                                                                                          _wifiSettingsService(server, &ESPFS, &_securitySettingsService, &_socket),
                                                                                          _wifiScanner(server, &_securitySettingsService),
                                                                                          _wifiStatus(server, &_securitySettingsService),
                                                                                          _apSettingsService(server, &ESPFS, &_securitySettingsService),
                                                                                          _apStatus(server, &_securitySettingsService, &_apSettingsService),
                                                                                          _socket(server, &_securitySettingsService, AuthenticationPredicates::IS_AUTHENTICATED),
                                                                                          _notificationService(&_socket),
#if FT_ENABLED(FT_NTP)
                                                                                          _ntpSettingsService(server, &ESPFS, &_securitySettingsService),
                                                                                          _ntpStatus(server, &_securitySettingsService),
#endif
#if FT_ENABLED(FT_UPLOAD_FIRMWARE)
                                                                                          _uploadFirmwareService(server, &_securitySettingsService),
#endif
#if FT_ENABLED(FT_DOWNLOAD_FIRMWARE)
                                                                                          _downloadFirmwareService(server, &_securitySettingsService, &_socket),
#endif
#if FT_ENABLED(FT_MQTT)
                                                                                          _mqttSettingsService(server, &ESPFS, &_securitySettingsService),
                                                                                          _mqttStatus(server, &_mqttSettingsService, &_securitySettingsService),
#endif
#if FT_ENABLED(FT_SECURITY)
                                                                                          _authenticationService(server, &_securitySettingsService),
#endif
#if FT_ENABLED(FT_SLEEP)
                                                                                          _sleepService(server, &_securitySettingsService),
#endif
#if FT_ENABLED(FT_BATTERY)
                                                                                          _batteryService(&_socket),
#endif
#if FT_ENABLED(FT_ANALYTICS)
                                                                                          _analyticsService(&_socket),
#endif
                                                                                          _restartService(server, &_securitySettingsService),
                                                                                          _factoryResetService(server, &ESPFS, &_securitySettingsService),
                                                                                          _systemStatus(server, &_securitySettingsService),
                                                                                          _starService(server, &_socket, &_securitySettingsService),
                                                                                          _filesService(server, &_socket, &_securitySettingsService),
                                                                                          _fixtureService(server, &_socket, &_securitySettingsService, &ESPFS),
                                                                                          _effectsService(server, &_socket, &_securitySettingsService, &ESPFS, &_fixtureService)
{
}

void ESP32SvelteKit::begin()
{
    ESP_LOGV("ESP32SvelteKit", "Loading settings from files system");
    ESPFS.begin(true);

    _wifiSettingsService.initWiFi();

    // SvelteKit uses a lot of handlers, so we need to increase the max_uri_handlers
    // WWWData has 77 Endpoints, Framework has 27, and Lighstate Demo has 4
    _server->config.max_uri_handlers = _numberEndpoints;
    _server->listen(80);

#ifdef EMBED_WWW
    // Serve static resources from PROGMEM
    ESP_LOGV("ESP32SvelteKit", "Registering routes from PROGMEM static resources");
    WWWData::registerRoutes(
        [&](const String &uri, const String &contentType, const uint8_t *content, size_t len)
        {
            PsychicHttpRequestCallback requestHandler = [contentType, content, len](PsychicRequest *request)
            {
                PsychicResponse response(request);
                response.setCode(200);
                response.setContentType(contentType.c_str());
                response.addHeader("Content-Encoding", "gzip");
                // commented cache control to avoid caching (temp should be develop mode)
                // response.addHeader("Cache-Control", "public, immutable, max-age=31536000");
                // response.addHeader("Cache-Control", "public, immutable, max-age=300");
                response.setContent(content, len);
                return response.send();
            };
            PsychicWebHandler *handler = new PsychicWebHandler();
            handler->onRequest(requestHandler);
            _server->on(uri.c_str(), HTTP_GET, handler);

            // Set default end-point for all non matching requests
            // this is easier than using webServer.onNotFound()
            if (uri.equals("/index.html"))
            {
                _server->defaultEndpoint->setHandler(handler);
            }
        });
#else
    // Serve static resources from /www/
    ESP_LOGV("ESP32SvelteKit", "Registering routes from FS /www/ static resources");
    _server->serveStatic("/_app/", ESPFS, "/www/_app/");
    _server->serveStatic("/favicon.png", ESPFS, "/www/favicon.png");
    //  Serving all other get requests with "/www/index.htm"
    _server->onNotFound([](PsychicRequest *request)
                        {
        if (request->method() == HTTP_GET) {
            PsychicFileResponse response(request, ESPFS, "/www/index.html", "text/html");
            return response.send();
            // String url = "http://" + request->host() + "/index.html";
            // request->redirect(url.c_str());
        } });
#endif

    // Serve static resources from /config/ if set by platformio.ini
#if SERVE_CONFIG_FILES
    _server->serveStatic("/config/", ESPFS, "/config/");
#endif

#if defined(ENABLE_CORS)
    ESP_LOGV("ESP32SvelteKit", "Enabling CORS headers");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", CORS_ORIGIN);
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Accept, Content-Type, Authorization");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Credentials", "true");
#endif

    ESP_LOGV("ESP32SvelteKit", "Starting MDNS");
    MDNS.begin(_wifiSettingsService.getHostname().c_str());
    MDNS.setInstanceName(_appName);
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ws", "tcp", 80);
    MDNS.addServiceTxt("http", "tcp", "Firmware Version", APP_VERSION);

#ifdef SERIAL_INFO
    Serial.printf("Running Firmware Version: %s\n", APP_VERSION);
#endif

    // Start the services
    _apStatus.begin();
    _socket.begin();
    _notificationService.begin();
    _apSettingsService.begin();
    _factoryResetService.begin();
    _featureService.begin();
    _restartService.begin();
    _systemStatus.begin();
    _wifiSettingsService.begin();
    _wifiScanner.begin();
    _wifiStatus.begin();

#if FT_ENABLED(FT_UPLOAD_FIRMWARE)
    _uploadFirmwareService.begin();
#endif
#if FT_ENABLED(FT_DOWNLOAD_FIRMWARE)
    _downloadFirmwareService.begin();
#endif
#if FT_ENABLED(FT_NTP)
    _ntpSettingsService.begin();
    _ntpStatus.begin();
#endif
#if FT_ENABLED(FT_MQTT)
    _mqttSettingsService.begin();
    _mqttStatus.begin();
#endif
#if FT_ENABLED(FT_SECURITY)
    _authenticationService.begin();
    _securitySettingsService.begin();
#endif
#if FT_ENABLED(FT_ANALYTICS)
    _analyticsService.begin();
#endif
#if FT_ENABLED(FT_SLEEP)
    _sleepService.begin();
#endif
#if FT_ENABLED(FT_BATTERY)
    _batteryService.begin();
#endif

    _starService.begin();
    _filesService.begin();
    _fixtureService.begin(); //before effectservice!
    _effectsService.begin();

    // Start the loop task
    ESP_LOGV("ESP32SvelteKit", "Starting loop task");
    xTaskCreatePinnedToCore(
        this->_loopImpl,            // Function that should be called
        "ESP32 SvelteKit Loop",     // Name of the task (for debugging)
        4096,                       // Stack size (bytes)
        this,                       // Pass reference to this class instance
        (tskIDLE_PRIORITY + 1),     // task priority
        NULL,                       // Task handle
        ESP32SVELTEKIT_RUNNING_CORE // Pin to application core
    );
}

void ESP32SvelteKit::_loop()
{
    bool wifi = false;
    bool ap = false;
    bool event = false;
    bool mqtt = false;

    while (1)
    {
        uint32_t cycles = ESP.getCycleCount();
        loopsPerSecond++;

        //every 20ms
        if (millis() - twentyMsMillis >= 20) {
            twentyMsMillis = millis();

            _wifiSettingsService.loop(); // 30 seconds
            _apSettingsService.loop();   // 10 seconds
    #if FT_ENABLED(FT_MQTT)
            _mqttSettingsService.loop(); // 5 seconds
    #endif
            // Query the connectivity status
            wifi = _wifiStatus.isConnected();
            ap = _apStatus.isActive();
            event = _socket.getConnectedClients() > 0;
    #if FT_ENABLED(FT_MQTT)
            mqtt = _mqttStatus.isConnected();
    #endif

            // Update the system status
            if (wifi && mqtt)
            {
                _connectionStatus = ConnectionStatus::STA_MQTT;
            }
            else if (wifi)
            {
                _connectionStatus = event ? ConnectionStatus::STA_CONNECTED : ConnectionStatus::STA;
            }
            else if (ap)
            {
                _connectionStatus = event ? ConnectionStatus::AP_CONNECTED : ConnectionStatus::AP;
            }
            else
            {
                _connectionStatus = ConnectionStatus::OFFLINE;
            }

            // iterate over all loop functions
            for (auto &function : _loopFunctions)
            {
                function();
            }

        }

        //every 50ms, 20 lps
        if (millis() - fiftyMsMillis >= 50) {
            fiftyMsMillis = millis();

            _fixtureService.loop50ms();
        }

        cycles =  (ESP.getCycleCount() - cycles); //add the new cycles (converted to ms) to the total cpu time
        cyclesPerSecond += cycles;

        //every second (add analytics here instead of a seperate task with its own stack ?)
        if (millis() - oneSecondMillis >= 1000) {
            oneSecondMillis = millis();

            _analyticsService.cpuPerc = cyclesPerSecond / (ESP.getCpuFreqMHz() * 10000); //1 sec
            _analyticsService.loopsPerSecond = loopsPerSecond;
            // _systemStatus.cpuPerc = _analyticsService.cpuPerc;
            // _systemStatus.loopsPerSecond = _analyticsService.loopsPerSecond;
            Serial.printf("Cycles: %d - %d / %d -> %d lps:%d\n", cycles/1000000, cyclesPerSecond/1000000, ESP.getCpuFreqMHz(), _analyticsService.cpuPerc, _analyticsService.loopsPerSecond);

            cyclesPerSecond = 0;
            loopsPerSecond = 0;

        }

        vTaskDelay(1 / portTICK_PERIOD_MS); // feed the watchdog (this sets the loop to a max of 1000 loops per second!), otherwise 10000 fps measured (but watchdog crash)
        //can be removed if there is always enough to do in this loop

    } //while 1
}
