#ifndef RELAY_STATE_H
#define RELAY_STATE_H

#include <Arduino.h>
#include "relay_types.h"
#include "relay_curves.h"

/* Shared relay state structure for synchronizing both menu systems */
struct RelaySharedState {
    // Protection control
    bool* protection_enabled;
    bool* latched;
    
    // Settings (by reference)
    Standard*   std;
    IEC_Curve*  iec;
    IEEE_Curve* ieee;
    float*      tms;
    float*      pickup;
    float*      inst_m;
    
    RelaySharedState(bool& prot_en, bool& lat, Standard& s, IEC_Curve& i, 
                     IEEE_Curve& ie, float& t, float& p, float& im)
        : protection_enabled(&prot_en), latched(&lat), 
          std(&s), iec(&i), ieee(&ie), tms(&t), pickup(&p), inst_m(&im) {}
};

#endif
