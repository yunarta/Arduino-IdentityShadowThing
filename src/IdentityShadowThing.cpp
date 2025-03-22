//
// Created by yunarta on 3/15/25.
//

#include "IdentityShadowThing.h"

#include <aws_utils.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "../../ESP32-QualityOfLife/include/ESP32QoL.h"

namespace {
    const char *AWS_IOT_NAMESPACE = "aws-iot";
    const char *AWS_IOT_CERTIFICATE = "/aws-iot/certificate.pem.crt";
    const char *AWS_IOT_PRIVATE_KEY = "/aws-iot/private.pem.key";
    const char *AWS_IOT_ROOT_CA = "/aws-iot/aws-root-ca.pem";
    const char *SHADOW_IDENTITY_KEY = "shadowIdentity";
    const char *IDENTITY_SHADOW = "Identity";

    const char *EVENT_IDENTITY = "Identity";
    const char *EVENT_JOBS = "Jobs";
    const char *EVENT_COMMAND = "Command";
}

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
    Serial.println(F("[INFO] IdentityShadowThing initialized"));
    Serial.printf("[INFO] awsEndPoint: %s\n", awsEndPoint);
    Serial.printf("[INFO] provisioningName: %s\n", provisioningName);
#endif
}

void IdentityShadowThing::begin() {
    preferences.begin(AWS_IOT_NAMESPACE, false);

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
        thingClient->setCommandCallback([this](const String &jobId, JsonDocument &payload) -> bool {
            return thingCommandCallback(jobId, payload);
        });
        thingClient->setJobsCallback([this](const String &jobId, JsonDocument &payload) -> bool {
            return thingJobsCallback(jobId, payload);
        });
        thingClient->setShadowCallback([this](const String &shadowName, JsonObject &payload) -> bool {
            return thingShadowCallback(shadowName, payload);
        });

        String payload = preferences.getString(SHADOW_IDENTITY_KEY, "{}");

        JsonDocument doc;
        deserializeJson(doc, payload);
        auto object = doc.as<JsonObject>();

        thingClient->preloadShadow(IDENTITY_SHADOW, object);
    }

    LittleFS.open(AWS_IOT_CERTIFICATE, "r");
    auto rootca = LittleFS.open(AWS_IOT_ROOT_CA, "r");
    securedClient.loadCACert(rootca, rootca.size());

    auto certificate = LittleFS.open(AWS_IOT_CERTIFICATE, "r");
    securedClient.loadCertificate(certificate, certificate.size());

    auto privateKey = LittleFS.open(AWS_IOT_PRIVATE_KEY, "r");
    securedClient.loadPrivateKey(privateKey, privateKey.size());

    mqttClient.setServer(this->awsEndPoint.c_str(), 8883);

    startAttemptTime = millis();
    connectionState = CONNECTING;
#ifdef LOG_INFO
    Serial.println(F("[INFO] MQTT and Shadow callbacks initialized"));
#endif
}

void IdentityShadowThing::connect() {
#ifdef LOG_INFO
    Serial.println(F("[INFO] Starting MQTT connection"));
#endif

    if (mqttClient.connect(WiFi.macAddress().c_str())) {
        connectionState = CONNECTED;

#ifdef LOG_INFO
        Serial.println(F("[INFO] MQTT connected"));
#endif
        if (!provisioned) {
            provisioningClient->begin();
#ifdef LOG_INFO
            Serial.println(F("[INFO] Starting provisioning"));
#endif
        } else {
            thingClient->begin();
            thingClient->registerShadow(IDENTITY_SHADOW);
#ifdef LOG_INFO
            Serial.println(F("[INFO] Shadow client started and 'Identity' shadow registered"));
#endif
        }
    } else {
#ifdef LOG_DEBUG
        Serial.println(F("[DEBUG] MQTT connection failed, retrying..."));
#endif
        if (millis() - startAttemptTime >= 120000) {
            connectionState = TIMEOUT;
#ifdef LOG_INFO
            Serial.println(F("[INFO] MQTT connection timeout reached"));
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
        Serial.println(F("[DEBUG] MQTT disconnected, attempting reconnect"));
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
        Serial.println(F("[DEBUG] Failed to deserialize incoming MQTT message"));
#endif
        return;
    }

#ifdef LOG_DEBUG
    Serial.printf("[DEBUG] Processing message on topic: %s\n", topic);
    Serial.printf("[DEBUG] Payload: %s\n", payload);
#endif

    if (!provisioned) {
        if (provisioningClient->onMessage(topic, doc)) {
#ifdef LOG_DEBUG
            Serial.println(F("[DEBUG] Message handled by provisioning client"));
#endif
        }
    } else {
        if (thingClient->onMessage(topic, doc)) {
#ifdef LOG_DEBUG
            Serial.println(F("[DEBUG] Message handled by thing client"));
#endif
        }
    }
}

bool IdentityShadowThing::provisioningCallback(const String &topic, JsonDocument &payload) {
    if (topic.equals("provisioning/success")) {
#ifdef LOG_INFO
        Serial.println(F("[INFO] Provisioning successful, saving credentials"));
#endif
        auto certificate = LittleFS.open(AWS_IOT_CERTIFICATE, "w", true);
        certificate.print(payload["certificate"].as<const char *>());
        certificate.flush();
        certificate.close();

        auto privateKey = LittleFS.open(AWS_IOT_PRIVATE_KEY, "w", true);
        privateKey.print(payload["privateKey"].as<const char *>());
        privateKey.flush();
        privateKey.close();

        preferences.putBool("provisioned", true);
        esp_restart();
    }

    return true;
}

bool IdentityShadowThing::thingCommandCallback(const String &executionId, JsonDocument &payload) {
#ifdef LOG_DEBUG
    Serial.printf("[DEBUG] Received callback for executionId: %s\n", executionId.c_str());
#endif

    if (this->callback != nullptr) {
        callback(EVENT_COMMAND);
    }

    if (this->commandCallback != nullptr) {
        commandCallback(executionId, payload);
    }
    return false;
}

bool IdentityShadowThing::thingJobsCallback(const String &jobId, JsonDocument &payload) {
#ifdef LOG_DEBUG
    Serial.printf("[DEBUG] Received callback for job: %s\n", jobId.c_str());
#endif

    this->jobs = payload;
    if (this->callback != nullptr) {
        callback(EVENT_JOBS);
    }

    return false;
}

bool IdentityShadowThing::thingCallback(const String &shadowName, JsonDocument &payload) {
#ifdef LOG_DEBUG
    Serial.printf("[DEBUG] Received callback for shadow: %s\n", shadowName.c_str());
#endif
    return false;
}

bool IdentityShadowThing::thingShadowCallback(const String &shadowName, JsonObject &payload) {
    if (shadowName.equals(IDENTITY_SHADOW)) {
        JsonObject shadow = thingClient->getShadow(IDENTITY_SHADOW);
        long currentVersion = shadow["version"].as<long>();
        long desiredVersion = payload["version"].as<long>();

#ifdef LOG_DEBUG
        Serial.printf("[DEBUG] Shadow update received for 'Identity' - Current Version: %ld, Desired Version: %ld\n",
                      currentVersion, desiredVersion);
#endif

        if (desiredVersion != 0) {
            if (thingClient->isValidated(IDENTITY_SHADOW)) {
                thingClient->listPendingJobs();
            }

            if (currentVersion != desiredVersion) {
                thingClient->updateShadow(IDENTITY_SHADOW, payload);

                String serialized;
                serializeJson(shadow, serialized);
                preferences.putString(SHADOW_IDENTITY_KEY, serialized);

                if (!payload["firmware"].isNull()) {
                    auto firmware = payload["firmware"].as<String>();
                    if (firmware != "") {
#ifdef LOG_INFO
                        Serial.printf("[INFO] Starting firmware update from URL: %s\n", firmware.c_str());
#endif
                        // performOTAUpdate(firmware);
#ifdef LOG_INFO
                        Serial.println(F("[INFO] Firmware update process completed"));
#endif
                    }
                } else {
#ifdef LOG_INFO
                    Serial.println(F("[INFO] No firmware update requested"));
#endif
                }

                if (this->callback != nullptr) {
                    callback(EVENT_IDENTITY);
                }
#ifdef LOG_INFO
                Serial.println(F("[INFO] Shadow updated for 'Identity'"));
#endif
            } else {
                thingClient->preloadedShadowValidated(shadowName);
            }
        }
        return true;
    }

    return false;
}

void IdentityShadowThing::setEventCallback(IdentityEventCallback callback) {
    this->callback = callback;
}

void IdentityShadowThing::setSignalCallback(IdentityShadowThingSignalCallback callback) {
    this->signalCallback = callback;
}

void IdentityShadowThing::setCommandCallback(IdentityCommandCallback callback) {
    this->commandCallback = callback;
}

PubSubClient *IdentityShadowThing::getClient() {
    return &mqttClient;
}

int IdentityShadowThing::getConnectionState() {
    return connectionState;
}

JsonDocument IdentityShadowThing::getPendingJobs() {
    return this->jobs;
}

void IdentityShadowThing::commandReply(const String &executionId, const CommandReply &payload) {
    // {
    //     'status': 'SUCCEEDED',
    //     'statusReason': {
    //         'reasonCode': '200',
    //         'reasonDescription': 'Execution_in_progress'
    //     },
    //     'result': {
    //         'status': {
    //             's': 'OK'
    //         }
    //     },
    // }
    thingClient->commandReply(executionId, payload);
}
