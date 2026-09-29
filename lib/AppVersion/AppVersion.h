#pragma once

// PlatformIO normally supplies these through build_flags/extra_scripts. Keep
// fallbacks here so editor indexers and simulator-like tools still parse files.
#ifndef CROSSPOINT_VERSION
#define CROSSPOINT_VERSION "dev"
#endif

#ifndef ECHO_VERSION
#define ECHO_VERSION "dev"
#endif

#ifndef ECHO_BUILD_ENV
#define ECHO_BUILD_ENV "unknown"
#endif

#ifndef ECHO_FIRMWARE_VARIANT
#ifdef CROSSPOINT_FIRMWARE_VARIANT
#define ECHO_FIRMWARE_VARIANT CROSSPOINT_FIRMWARE_VARIANT
#else
#define ECHO_FIRMWARE_VARIANT "unknown"
#endif
#endif
