//
//  kern_start.cpp
//  WhateverGreen
//
//  Copyright © 2017 vit9696. All rights reserved.
//

#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>

#include "kern_weg.hpp"

static WEG weg;

static const char *bootargOff[] {
	"-wegoff"
};

static const char *bootargDebug[] {
	"-wegdbg"
};

static const char *bootargBeta[] {
	"-wegbeta"
};

/**
 * Plugin configuration for Lilu
 * Fixed to include support for KernelVersion::Tahoe (macOS 26)
 */
PluginConfiguration ADDPR(config) {
	xStringify(PRODUCT_NAME),
	parseModuleVersion(xStringify(MODULE_VERSION)),
	LiluAPI::AllowNormal | LiluAPI::AllowInstallerRecovery | LiluAPI::AllowSafeMode,
	bootargOff,
	arrsize(bootargOff),
	bootargDebug,
	arrsize(bootargDebug),
	bootargBeta,
	arrsize(bootargBeta),
	KernelVersion::SnowLeopard, // Minimum kernel version (10.6)
	KernelVersion::Tahoe,       // Maximum kernel version (macOS 26 Tahoe)
	[]() {
		weg.init();
	}
};
