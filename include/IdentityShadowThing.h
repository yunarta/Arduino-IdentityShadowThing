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

#define IdentityShadowThingCallback std::function<bool(const String &event)>
#define IdentityShadowThingSignalCallback std::function<void(void)>

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

    FleetProvisioningClient* provisioningClient;
    ThingClient* thingClient;

    Preferences preferences;

    void mqttCallback(const char *topic, uint8_t *payload, unsigned int length);

    bool provisioningCallback(const String &topic, JsonDocument &payload);

    bool thingCallback(const String &topic, JsonDocument &payload);

    bool thingShadowCallback(const String &shadowName, JsonObject &payload);

    IdentityShadowThingCallback callback;
    IdentityShadowThingSignalCallback signalCallback;

    int connectionState;
    unsigned long startAttemptTime;
    bool provisioned;
public:
    IdentityShadowThing(const char *awsEndPoint, const char *provisioningName);

    void begin();
    void connect();
    void loop();

    void setIdentityCallback(IdentityShadowThingCallback callback);
    void setSignalCallback(IdentityShadowThingSignalCallback callback);

    PubSubClient* getClient();
    int getConnectionState();

};


#endif //IDENTITYSHADOWTHING_H
