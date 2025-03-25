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

extern const char *IDENTITY_THING_EVENT_IDENTITY;
extern const char *IDENTITY_THING_EVENT_COMMAND;
extern const char *IDENTITY_THING_EVENT_JOBS;

#define IdentityEventCallback std::function<bool(const String &event)>
#define IdentityShadowThingSignalCallback std::function<void(void)>
#define IdentityJobCallback std::function<bool(const String &jobId, JsonDocument &payload)>
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

    JsonDocument identity;

    void mqttCallback(const char *topic, uint8_t *payload, unsigned int length);

    bool provisioningCallback(const String &topic, JsonDocument &payload);

    bool thingCommandCallback(const String &executionId, JsonDocument &payload);

    void updateFirmware(const String &jobId, JsonDocument &payload);

    bool thingJobsCallback(const String &jobId, JsonDocument &payload);

    bool thingCallback(const String &topic, JsonDocument &payload);

    bool thingShadowCallback(const String &shadowName, JsonObject &payload, bool shouldMutate);

    IdentityEventCallback callback;
    IdentityShadowThingSignalCallback signalCallback;
    IdentityJobCallback jobCallback;
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

    void setJobCallback(IdentityJobCallback callback);

    PubSubClient *getClient();

    int getConnectionState();

    JsonDocument getPendingJobs();

    JsonObject getIdentity();

    void mergeIdentity(JsonDocument identity);

    void commandReply(const String &executionId, const CommandReply &payload);

    void jobReply(const String &jobId, const JobReply &payload);

    void requestJobDetail(const String &jobId);
};


#endif //IDENTITYSHADOWTHING_H
