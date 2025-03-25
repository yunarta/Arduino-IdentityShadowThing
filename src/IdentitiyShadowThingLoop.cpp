//
// Created by yunarta on 3/25/25.
//

#include "IdentitiyShadowThingLoop.h"

long shadowLoop(IdentityShadowThing* shadowThing) {
    // vTaskDelay(pdMS_TO_TICKS(1000));
    shadowThing->loop();
    switch (shadowThing->getConnectionState()) {
        case CONNECTED:
          return 1000L;
        break;
        case CONNECTING:
            return 10000L;
        break;
        case TIMEOUT:
            return -1;
    }

    return 1000L;
}