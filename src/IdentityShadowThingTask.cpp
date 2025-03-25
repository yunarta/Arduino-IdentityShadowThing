//
// Created by yunarta on 3/25/25.
//

#include "IdentityShadowThingTask.h"


static void IdentityShadowThingTask::taskEntryPoint(void *p) {
    auto *task = static_cast<IdentityShadowThingTask *>(p);
    task->task();
}

void IdentityShadowThingTask::task() {
    shadowThing->setSignalCallback([this] {
        xTaskNotifyGive(thingLifecycleHandle);
    });
    shadowThing->begin();

    while (true) {
        // vTaskDelay(pdMS_TO_TICKS(1000));
        shadowThing->loop();
        switch (shadowThing->getConnectionState()) {
            case CONNECTED:
                // ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
                    vTaskDelay(pdMS_TO_TICKS(1000));
            break;
            case CONNECTING:
                vTaskDelay(pdMS_TO_TICKS(10000));
            break;
            case TIMEOUT:
                esp_deep_sleep_start();
        }
    }
}

IdentityShadowThingTask::IdentityShadowThingTask(IdentityShadowThing *shadowThing) {
    this->shadowThing = shadowThing;
}

void IdentityShadowThingTask::begin() {
    xTaskCreate(taskEntryPoint,
                "thingLifecycle", 10 * 1024, this, 1,
                &thingLifecycleHandle);
}
