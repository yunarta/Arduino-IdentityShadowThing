//
// Created by yunarta on 3/15/25.
//

#include "IdentityShadowThing.h"

#include <aws_utils.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "../../ESP32-QualityOfLife/include/ESP32QoL.h"

const char *AWS_IOT_NAMESPACE = "aws-iot";
const char *AWS_IOT_CERTIFICATE = "/aws-iot/certificate.pem.crt";
const char *AWS_IOT_PRIVATE_KEY = "/aws-iot/private.pem.key";
const char *AWS_IOT_ROOT_CA = "/aws-iot/aws-root-ca.pem";
const char *SHADOW_IDENTITY_KEY = "shadowIdentity";
const char *IDENTITY_SHADOW = "Identity";

const char *IDENTITY_THING_EVENT_IDENTITY = "Identity";
const char *IDENTITY_THING_EVENT_JOBS = "Jobs";
const char *IDENTITY_THING_EVENT_COMMAND = "Command";

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
        thingClient->setShadowCallback([this](const String &shadowName, JsonObject &payload, bool shouldMutate) -> bool {
            return thingShadowCallback(shadowName, payload, shouldMutate);
        });

        String payload = preferences.getString(SHADOW_IDENTITY_KEY, "{}");

        // JsonDocument doc;
        // deserializeJson(doc, payload);
        // auto object = doc.as<JsonObject>();
        // thingClient->preloadShadow(IDENTITY_SHADOW, object);
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
        identified = false;
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
        mqttClient.loop();
        if (provisioned) {
            thingClient->loop();
        }
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
        callback(IDENTITY_THING_EVENT_COMMAND);
    }

    if (this->commandCallback != nullptr) {
        commandCallback(executionId, payload);
    }
    return false;
}

void IdentityShadowThing::updateFirmware(const String &jobId, JsonDocument &payload) {
    JsonObject execution = payload["execution"];
    JsonObject document = execution["jobDocument"].as<JsonObject>();

    Preferences otaPreferences;
    otaPreferences.begin("OTAUpdate", false);

    String firmwareUrl = document["job"]["params"]["url"];
    String firmwareVersion = document["job"]["params"]["version"];

    String installedVersion = otaPreferences.getString("appVersion", "");
    if (installedVersion.equals(firmwareVersion)) {
        long expectedVersion = otaPreferences.getLong("expectedVersion", -1);
        if (expectedVersion != -1) {
            String expect = otaPreferences.getString("expect", "{}");

            JsonDocument expectJsonDoc;
            deserializeJson(expectJsonDoc, expect);

            JobReply reply{
                .status = "SUCCEEDED",
                .expectedVersion = expectedVersion,
                .statusDetails = expectJsonDoc,
            };

            Serial.printf("Replying to job ID: %s with status: %s\n", jobId.c_str(), reply.status.c_str());
            jobReply(jobId, reply);

            otaPreferences.remove("expect");
        }
    } else {
        JsonObject expect = document["expect"];

        String expectJson;
        serializeJson(expect, expectJson);

        otaPreferences.putString("executionId", jobId);
        otaPreferences.putLong("expectedVersion", execution["versionNumber"].as<long>());
        otaPreferences.putString("expect", expectJson);
        otaPreferences.end();

        OTAUpdate.begin(firmwareVersion, firmwareUrl);
    }

    otaPreferences.end();
}

bool IdentityShadowThing::thingJobsCallback(const String &jobId, JsonDocument &payload) {
#ifdef LOG_DEBUG
    Serial.printf("[DEBUG] Received callback for job: %s\n", jobId.c_str());
#endif

    String serialized;
    serializeJson(payload, serialized);
    deserializeJson(this->jobs, serialized);

    if (jobId.length() == 0) {
        JsonDocument jobs = getPendingJobs();
        if (jobs.size() > 0) {
            Serial.println("Pending jobs found.");
            for (const JsonObject &job: jobs["queuedJobs"].as<JsonArray>()) {
                requestJobDetail(job["jobId"]);
            }
            return true;
        }
   
        if (this->callback != nullptr) {
            callback(IDENTITY_THING_EVENT_JOBS);
            return true;
        }
    } else {
        auto execution = payload["execution"].as<JsonObject>();
        if (!execution.isNull()) {
            auto document = execution["jobDocument"].as<JsonObject>();
            if (!document.isNull()) {
                String jobType = document["type"];
                if (jobType.equals("UpdateFirmware")) {
                    updateFirmware(jobId, payload);
                    return true;
                }
            }
        }

        if (this->jobCallback != nullptr) {
            jobCallback(jobId, payload);
            return true;
        }
    }


    return false;
}

bool IdentityShadowThing::thingCallback(const String &shadowName, JsonDocument &payload) {
#ifdef LOG_DEBUG
    Serial.printf("[DEBUG] Received callback for shadow: %s\n", shadowName.c_str());
#endif
    return false;
}

bool IdentityShadowThing::thingShadowCallback(const String &shadowName, JsonObject &payload, bool shouldMutate) {
    if (shadowName.equals(IDENTITY_SHADOW)) {
        identified = true;
        JsonObject shadow = thingClient->getShadow(IDENTITY_SHADOW);

        Serial.printf("[DEBUG] Received callback for shadow: %s\n", shadowName.c_str());
        Serial.printf("[DEBUG] Should mutate shadow: %s\n", shouldMutate ? "true" : "false");

        if (shouldMutate) {
            Preferences otaPreferences;
            otaPreferences.begin("OTAUpdate", false);
            String installedVersion = otaPreferences.getString("appVersion", "");
            otaPreferences.end();

            payload["appVersion"] = installedVersion;

            // merge this->identity into payload
            for (JsonPair kv: this->identity.as<JsonObject>()) {
                payload[kv.key()] = kv.value();
            }

            thingClient->updateShadow(IDENTITY_SHADOW, payload);

            String serialized;
            serializeJson(shadow, serialized);
            preferences.putString(SHADOW_IDENTITY_KEY, serialized);

            if (this->callback != nullptr) {
                callback(IDENTITY_THING_EVENT_IDENTITY);
            }
#ifdef LOG_INFO
            Serial.println(F("[INFO] Shadow updated for 'Identity'"));
#endif
        } else {
#ifdef LOG_INFO
            Serial.println(F("[INFO] Shadow preload validated for 'Identity'"));
#endif
            thingClient->preloadedShadowValidated(shadowName);
            thingClient->listPendingJobs();
        }

        if (thingClient->isValidated(IDENTITY_SHADOW)) {
            thingClient->listPendingJobs();
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

void IdentityShadowThing::setJobCallback(IdentityJobCallback callback) {
    this->jobCallback = callback;
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

JsonObject IdentityShadowThing::getIdentity() {
    return this->thingClient->getShadow(IDENTITY_SHADOW);
}

void IdentityShadowThing::mergeIdentity(JsonDocument identity) {
    this->identity = identity;
    if (identified) {
        thingClient->requestShadow(IDENTITY_SHADOW);
    }
}

void IdentityShadowThing::requestJobDetail(const String &jobId) {
    return thingClient->requestJobDetail(jobId);;
}

void IdentityShadowThing::commandReply(const String &executionId, const CommandReply &payload) {
    thingClient->commandReply(executionId, payload);
}

void IdentityShadowThing::jobReply(const String &jobId, const JobReply &payload) {
    thingClient->jobReply(jobId, payload);
}
