#ifndef PROVISIONING_H
#define PROVISIONING_H

#include <Arduino.h>

// WiFi onboarding for device32 examples.
//
// Credentials live in NVS, never in the image. When none are stored (or the
// saved network cannot be reached) the device raises a SoftAP with a captive
// portal that serves a small configuration page, so a user can provision it
// from a phone without a toolchain.

// True when an SSID has been stored in NVS.
bool wifiHasCredentials();

// Erase stored credentials (used by the hold-to-reset gesture).
void wifiClearCredentials();

// Connect using stored credentials. Falls back to the setup portal when
// nothing is stored or the association fails. Returns true once online; on
// success WiFi is connected in station mode.
bool wifiConnectOrProvision(uint32_t connectTimeoutMs);

// Blocking captive-portal flow. Brings up the SoftAP, answers DNS (so the OS
// captive-portal sheet opens) and HTTP, and only returns once the device has
// joined the network the user submitted.
void wifiRunSetupPortal();

#endif // PROVISIONING_H
