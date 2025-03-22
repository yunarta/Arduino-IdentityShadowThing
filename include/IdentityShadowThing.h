//
// Created by yunarta on 3/15/25.
//

#ifndef IDENTITYSHADOWTHING_H
#define IDENTITYSHADOWTHING_H

#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <AwsIoTCore.h>
#include <Preferences.h>
#include <ArduinoJson.h>

namespace {
    extern const char *EVENT_IDENTITY;
    extern const char *EVENT_JOBS;
}

#define IdentityEventCallback std::function<bool(const String &event)>
#define IdentityShadowThingSignalCallback std::function<void(void)>
#define IdentityCommandCallback std::function<bool(const String &executionId, JsonDocument &payload)>

enum IdentityShadowThingConnectionState {
    CONNECTED = 1,
    CONNECTING = 2,
    TIMEOUT = 3
};

class IdentityShadowThing {
    String thingName;
    String provisioningName;
    String awsEndPoint;

    WiFiClientSecure securedClient;
    PubSubClient mqttClient;

    FleetProvisioningClient *provisioningClient;
    ThingClient *thingClient;

    Preferences preferences;
    JsonDocument jobs;

    void mqttCallback(const char *topic, uint8_t *payload, unsigned int length);

    bool provisioningCallback(const String &topic, JsonDocument &payload);

    bool thingCommandCallback(const String &executionId, JsonDocument &payload);

    bool thingJobsCallback(const String &jobId, JsonDocument &payload);

    bool thingCallback(const String &topic, JsonDocument &payload);

    bool thingShadowCallback(const String &shadowName, JsonObject &payload);

    IdentityEventCallback callback;
    IdentityShadowThingSignalCallback signalCallback;
    IdentityCommandCallback commandCallback;

    int connectionState;
    unsigned long startAttemptTime;
    bool provisioned;

public:
    IdentityShadowThing(const char *awsEndPoint, const char *provisioningName);

    void begin();

    void connect();

    void loop();

    void setEventCallback(IdentityEventCallback callback);

    void setSignalCallback(IdentityShadowThingSignalCallback callback);

    void setCommandCallback(IdentityCommandCallback callback);

    PubSubClient *getClient();

    int getConnectionState();

    JsonDocument getPendingJobs();

    void commandReply(const String &executionId, const CommandReply &payload);
};


#endif //IDENTITYSHADOWTHING_H
