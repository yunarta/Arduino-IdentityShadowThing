//
// Created by yunarta on 3/25/25.
//

#ifndef IDENTITYSHADOWTHINGTASK_H
#define IDENTITYSHADOWTHINGTASK_H

#include <IdentityShadowThing.h>

class IdentityShadowThingTask {
    static void taskEntryPoint(void *p);

    void task();

public:
    IdentityShadowThing *shadowThing;

    TaskHandle_t thingLifecycleHandle = nullptr;

    IdentityShadowThingTask(IdentityShadowThing *shadowThing) ;

    void begin();
};

#endif //IDENTITYSHADOWTHINGTASK_H
