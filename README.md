# IdentityShadowThing

## Overview

The `IdentityShadowThing` class is an implementation for managing AWS IoT shadows and MQTT connections on devices built
with the Arduino framework (e.g., ESP32). It uses AWS Fleet Provisioning to provision and manage devices dynamically.

This implementation leverages:

- **AWS IoT**: To handle device provisioning and shadow updates.
- **LittleFS**: For storing certificates and keys securely.
- **ArduinoJson**: For handling JSON data related to MQTT messages and shadow updates.
- **PubSubClient**: For managing MQTT communication.

### Features

- AWS IoT device provisioning using Fleet Provisioning.
- Management of IoT shadows, including updates and callbacks (with example application on "Identity" shadow).
- Secure communication with AWS IoT using TLS/SSL.
- Automatic reconnect and loop handling for MQTT communication.

---

## Class: `IdentityShadowThing`

### Constructor

```cpp
IdentityShadowThing(const char *awsEndPoint, const char *provisioningName);
```

- **Parameters**:
    - `awsEndPoint`: AWS IoT endpoint for MQTT communication.
    - `provisioningName`: The provisioning template name defined in AWS IoT.
- **Description**:
    - Initializes the class with the required endpoint and provisioning name.
    - Configures MQTT client and sets initial connection and provisioning states.

---

### Public Methods

#### `void begin()`

- **Description**:
    - Performs device initialization including preferences storage, secure client set-up, and MQTT configuration.
- **Key Points**:
    - Loads AWS certificates and private keys stored in LittleFS.
    - Sets MQTT server and callbacks for provisioning and shadow updates.

#### `void connect()`

- **Description**:
    - Attempts to establish an MQTT connection.
    - If device is not provisioned, begins the provisioning process using the Fleet Provisioning template.
    - If device is already provisioned, initializes shadow client and registers the "Identity" shadow.

#### `void loop()`

- **Description**:
    - Continuously manages the MQTT connection and handles shadow updates.
    - Automatically reconnects to MQTT if disconnected.

#### `void setIdentityCallback(IdentityShadowThingCallback callback)`

- **Description**:
    - Sets a callback for handling updates to the "Identity" shadow.

#### `void setSignalCallback(std::function<void()> callback)`

- **Description**:
    - Sets a signal callback, triggered when an MQTT message is received.

#### `PubSubClient *getClient()`

- **Description**:
    - Returns a pointer to the internal MQTT client.
    - Useful for custom handling of MQTT messages.

#### `int getConnectionState()`

- **Description**:
    - Returns the current MQTT connection state.

---

### Private Methods

#### `void mqttCallback(const char *topic, uint8_t *payload, unsigned int length)`

- **Description**:
    - Handles incoming MQTT messages by deserializing the payload and dispatching it to the provisioning or thing client
      based on the state.

#### `bool provisioningCallback(const String &topic, JsonDocument &payload)`

- **Description**:
    - Handles provisioning messages (e.g., storing certificate and private key upon success).

#### `bool thingCallback(const String &shadowName, JsonDocument &payload)`

- **Description**:
    - Callback for specific shadow updates.

#### `bool thingShadowCallback(const String &shadowName, JsonObject &payload)`

- **Description**:
    - Handles updates to the "Identity" shadow and synchronizes state if versions are mismatched.

---

## Dependencies

- **ArduinoJson**: Used for parsing JSON messages.
- **LittleFS**: A lightweight filesystem for storing credentials securely.
- **PubSubClient**: MQTT client library for handling communication with AWS.
- **AWS IoT libraries**: For secure provisioning and shadow management.

---

## How To Use

1. **Include Necessary Libraries and Files**:
   Ensure you have the following libraries added to your project and configured properly:
    - AWS IoT dependencies.
    - LittleFS and PubSubClient.

2. **Prepare AWS IoT Configuration**:
    - Set up a Fleet Provisioning template in the AWS IoT console.
    - Generate an AWS IoT endpoint.

3. **Write Initialization Code**:

   ```cpp
   #include "IdentityShadowThing.h"

   IdentityShadowThing identityShadowThing("your-aws-endpoint.amazonaws.com", "ProvisioningTemplateName");

   void setup() {
       Serial.begin(115200);
       identityShadowThing.begin();
   }

   void loop() {
       identityShadowThing.loop();
   }
   ```

4. **Customize Behavior with Callbacks**:
    - Use `setIdentityCallback` or `setSignalCallback` to respond to shadow updates or messages.

5. **Upload and Run on ESP32/Arduino**:
   Ensure the proper certificates and provisioning data are stored in the `/aws-iot/` directory on LittleFS.

---

## Logging and Debugging

- The library uses macros `LOG_INFO` and `LOG_DEBUG` for serial debugging.
- Set these macros in your project to enable or disable debugging output.

---

## Future Enhancements

- Dynamic handling of multiple shadows.
- Enhanced error handling and recovery mechanisms.
- Additional support for OTA updates using AWS IoT.

---

## License

This project adheres to the terms mentioned in the originating repository/framework/library files.