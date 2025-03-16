//
// Created by yunarta on 3/15/25.
//

#include "IdentityShadowThing.h"

#include <aws_utils.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

IdentityShadowThing::IdentityShadowThing(const char *awsEndPoint,
                                         const char *provisioningName): thingName(thingNameWithMac(ESP.getChipModel())),
                                                                        provisioningName(provisioningName),
                                                                        awsEndPoint(awsEndPoint),
                                                                        mqttClient(securedClient),
                                                                        callback(
                                                                            nullptr
                                                                        ) {
    provisioned = false;
#ifdef LOG_INFO
    Serial.println("[INFO] IdentityShadowThing initialized");
    Serial.printf("[INFO] awsEndPoint: %s\n", awsEndPoint);
    Serial.printf("[INFO] provisioningName: %s\n", provisioningName);
#endif
}

void IdentityShadowThing::begin() {
    preferences.begin("aws-iot", false);

    mqttClient.setCallback([this](char *topic, uint8_t *payload, unsigned int length) {
        this->mqttCallback(topic, payload, length);
    });
    mqttClient.setBufferSize(1024 * 4);
    provisioned = preferences.getBool("provisioned", false);
    if (!provisioned) {
        provisioningClient = new FleetProvisioningClient(&mqttClient, provisioningName, thingName);
        provisioningClient->setCallback([this](const String &topic, JsonDocument &payload) -> bool {
            return provisioningCallback(topic, payload);
        });
    } else {
        thingClient = new ThingClient(&mqttClient, thingName);
        thingClient->setCallback([this](const String &topic, JsonDocument &payload) -> bool {
            return thingCallback(topic, payload);
        });
        thingClient->setShadowCallback([this](const String &shadowName, JsonObject &payload) -> bool {
            return thingShadowCallback(shadowName, payload);
        });
    }


    auto rootca = LittleFS.open("/aws-iot/aws-root-ca.pem", "r");
    securedClient.loadCACert(rootca, rootca.size());

    auto certificate = LittleFS.open("/aws-iot/certificate.pem.crt", "r");
    securedClient.loadCertificate(certificate, certificate.size());

    auto privateKey = LittleFS.open("/aws-iot/private.pem.key", "r");
    securedClient.loadPrivateKey(privateKey, privateKey.size());

    mqttClient.setServer(this->awsEndPoint.c_str(), 8883);

    startAttemptTime = millis();
    connectionState = CONNECTING;
#ifdef LOG_INFO
    Serial.println("[INFO] MQTT and Shadow callbacks initialized");
#endif
}

void IdentityShadowThing::connect() {
#ifdef LOG_INFO
    Serial.println("[INFO] Starting MQTT connection");
#endif

    if (mqttClient.connect(WiFi.macAddress().c_str())) {
        connectionState = CONNECTED;

#ifdef LOG_INFO
        Serial.println("[INFO] MQTT connected");
#endif
        if (!provisioned) {
            provisioningClient->begin();
#ifdef LOG_INFO
            Serial.println("[INFO] Starting provisioning");
#endif
        } else {
            thingClient->begin();
            thingClient->registerShadow("Identity");
#ifdef LOG_INFO
            Serial.println("[INFO] Shadow client started and 'Identity' shadow registered");
#endif
        }
    } else {
#ifdef LOG_DEBUG
        Serial.println("[DEBUG] MQTT connection failed, retrying...");
#endif
        if (millis() - startAttemptTime >= 120000) {
            connectionState = TIMEOUT;
#ifdef LOG_INFO
            Serial.println("[INFO] MQTT connection timeout reached");
#endif
        }
    }
}

void IdentityShadowThing::loop() {
    if (!mqttClient.connected()) {
        if (connectionState == CONNECTED) {
            connectionState = CONNECTING;
            startAttemptTime = millis();
        }

#ifdef LOG_DEBUG
        Serial.println("[DEBUG] MQTT disconnected, attempting reconnect");
#endif
        connect();
    } else {
        connectionState = CONNECTED;
        if (provisioned) {
            thingClient->loop();
        }
        mqttClient.loop();
    }
}

void IdentityShadowThing::mqttCallback(const char *topic, uint8_t *payload, unsigned int length) {
    if (signalCallback != nullptr) {
        signalCallback();
    }

    payload[length] = '\0';
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
#ifdef LOG_DEBUG
        Serial.println("[DEBUG] Failed to deserialize incoming MQTT message");
#endif
        return;
    }

#ifdef LOG_DEBUG
    Serial.printf("[DEBUG] Processing message on topic: %s\n", topic);
#endif
    if (!provisioned) {
        if (provisioningClient->onMessage(topic, doc)) {
#ifdef LOG_DEBUG
            Serial.println("[DEBUG] Message handled by provisioning client");
#endif
        }
    } else {
        if (thingClient->onMessage(topic, doc)) {
#ifdef LOG_DEBUG
            Serial.println("[DEBUG] Message handled by thing client");
#endif
        }
    }
}


bool IdentityShadowThing::provisioningCallback(const String &topic, JsonDocument &payload) {
    if (topic.equals("provisioning/success")) {
#ifdef LOG_INFO
        Serial.println("[INFO] Provisioning successful, saving credentials");
#endif
        auto certificate = LittleFS.open("/aws-iot/certificate.pem.crt", "w", true);
        certificate.print(payload["certificate"].as<const char *>());
        certificate.flush();
        certificate.close();

        auto privateKey = LittleFS.open("/aws-iot/private.pem.key", "w", true);
        privateKey.print(payload["privateKey"].as<const char *>());
        privateKey.flush();
        privateKey.close();

        preferences.putBool("provisioned", true);
        esp_restart();
    }

    return true;
}

bool IdentityShadowThing::thingCallback(const String &shadowName, JsonDocument &payload) {
#ifdef LOG_DEBUG
    Serial.printf("[DEBUG] Received callback for shadow: %s\n", shadowName.c_str());
#endif
    return false;
}

bool IdentityShadowThing::thingShadowCallback(const String &shadowName, JsonObject &payload) {
    if (shadowName.equals("Identity")) {
        JsonObject shadow = thingClient->getShadow(shadowName);
        long currentVersion = shadow["version"].as<long>();
        long desiredVersion = payload["version"].as<long>();

#ifdef LOG_DEBUG
        Serial.printf("[DEBUG] Shadow update received for 'Identity' - Current Version: %ld, Desired Version: %ld\n",
                      currentVersion, desiredVersion);
#endif

        if (desiredVersion != 0 && currentVersion != desiredVersion) {
            thingClient->updateShadow(shadowName, payload);
            if (this->callback != nullptr) {
                callback("Identity");
            }
#ifdef LOG_INFO
            Serial.println("[INFO] Shadow updated for 'Identity'");
#endif
        }
        return true;
    }

    return false;
}

void IdentityShadowThing::setIdentityCallback(IdentityShadowThingCallback callback) {
    this->callback = callback;
}

void IdentityShadowThing::setSignalCallback(std::function<void()> callback) {
    this->signalCallback = callback;
}

PubSubClient *IdentityShadowThing::getClient() {
    return &mqttClient;
}

int IdentityShadowThing::getConnectionState() {
    return connectionState;
}
