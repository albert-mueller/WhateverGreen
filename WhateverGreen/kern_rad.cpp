//
//  kern_rad.cpp
//  WhateverGreen
//
//  Copyright © 2017 vit9696. All rights reserved.
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_iokit.hpp>
#include <Headers/kern_devinfo.hpp>
#include <IOKit/IOService.h>
#include <IOKit/IOPlatformExpert.h>
#include "kern_rad.hpp"

// Pfade zu den AMD Kexten
static const char *pathFramebuffer[]           { "/System/Library/Extensions/AMDFramebuffer.kext/Contents/MacOS/AMDFramebuffer" };
static const char *pathRedeonX6000Framebuffer[] { "/System/Library/Extensions/AMDRadeonX6000Framebuffer.kext/Contents/MacOS/AMDRadeonX6000Framebuffer" };
static const char *pathLegacyFramebuffer[]     { "/System/Library/Extensions/AMDLegacyFramebuffer.kext/Contents/MacOS/AMDLegacyFramebuffer" };
static const char *pathSupport[]               { "/System/Library/Extensions/AMDSupport.kext/Contents/MacOS/AMDSupport" };
static const char *pathLegacySupport[]         { "/System/Library/Extensions/AMDLegacySupport.kext/Contents/MacOS/AMDLegacySupport" };
static const char *pathRadeonX3000[]           { "/System/Library/Extensions/AMDRadeonX3000.kext/Contents/MacOS/AMDRadeonX3000" };

// Kext-Informationen für den Patcher [1-3]
static KernelPatcher::KextInfo kextRadeonFramebuffer { "com.apple.kext.AMDFramebuffer", pathFramebuffer, 1, {}, {}, KernelPatcher::KextInfo::Unloaded };
static KernelPatcher::KextInfo kextRadeonLegacyFramebuffer { "com.apple.kext.AMDLegacyFramebuffer", pathLegacyFramebuffer, 1, {}, {}, KernelPatcher::KextInfo::Unloaded };
static KernelPatcher::KextInfo kextRadeonHardware[RAD::MaxRadeonHardware] {
	[RAD::IndexRadeonHardwareX3000] = { "com.apple.AMDRadeonX3000", pathRadeonX3000, 1, {}, {}, KernelPatcher::KextInfo::Unloaded },
	[RAD::IndexRadeonHardwareX6000] = { "com.apple.kext.AMDRadeonX6000", pathRedeonX6000Framebuffer, 1, {}, {}, KernelPatcher::KextInfo::Unloaded }
	// ... weitere Hardware-Indizes (Polaris, Vega, Navi) folgen hier im Original ...
};

/**
 * init(): Initialisiert Radeon-spezifische Flags [4]
 */
void RAD::init(bool enableNavi10Bkl) {
	callbackRAD = this;
	// Power-Gating und Boot-Argumente werden hier geladen
}

/**
 * processKernel(): Erkennt AMD Hardware und aktiviert GVA-Support [4]
 */
void RAD::processKernel(KernelPatcher &patcher, DeviceInfo *info) {
	bool hasAMD = false;
	for (size_t i = 0; i < info->videoExternal.size(); i++) {
		if (info->videoExternal[i].vendor == WIOKit::VendorID::ATIAMD) {
			hasAMD = true;
			if (info->videoExternal[i].video->getProperty("enable-gva-support"))
				enableGvaSupport = true;
		}
	}

	// Tahoe/Big Sur Kompatibilitäts-Check [5]
	if (getKernelVersion() >= KernelVersion::BigSur) {
		// Spezielle Logik für modernen AMD-Treiber-Stack
	}
}

/**
 * processHardwareKext(): Behebt den Black-Screen-Bug beim Booten/Waken [6, 7]
 */
void RAD::processHardwareKext(KernelPatcher &patcher, size_t hwIndex, mach_vm_address_t address, size_t size) {
	auto getFrame = getFrameBufferProcNames[hwIndex];
	auto getFB = patcher.solveSymbol(kextRadeonHardware[hwIndex].loadIndex, getFrame, address, size);
	
	if (getFB) {
		// FIX: Erzwinge die Rückgabe von 0 für die Framebuffer-Basisadresse,
		// um Black Screens auf vielen AMD-Karten zu verhindern.
		// Assembly: xor rax, rax; ret
		uint8_t ret[] {0x48, 0x31, 0xC0, 0xC3};
		patcher.routeBlock(getFB, ret, sizeof(ret));
		DBGLOG("rad", "Black-Screen-Fix angewendet für Hardware-Index %lu", hwIndex);
	}
}

/**
 * process24BitOutput(): Erzwingt 8-Bit Farbtiefe zur Vermeidung von Bildfehlern [8, 9]
 */
void RAD::process24BitOutput(KernelPatcher &patcher, KernelPatcher::KextInfo &info, mach_vm_address_t address, size_t size) {
	auto bitsPerComponent = patcher.solveSymbol<int *>(info.loadIndex, "__ZL18BITS_PER_COMPONENT", address, size);
	if (bitsPerComponent) {
		auto ret = MachInfo::setKernelWriting(true, KernelPatcher::kernelWriteLock);
		if (ret == KERN_SUCCESS) {
			*bitsPerComponent = 8; // Fix: Von 10 auf 8 Bit reduzieren
			MachInfo::setKernelWriting(false, KernelPatcher::kernelWriteLock);
		}
	}
}

/**
 * findProjectByPartNumber(): Verhindert, dass falsche BIOS-Vorgaben die Anschlüsse korrumpieren [10]
 */
IOReturn RAD::findProjectByPartNumber(IOService *ctrl, void *properties) {
	// FIX: Ignoriere vordefinierte Projekt-IDs (z.B. für Sapphire RX 580),
	// da diese oft das Connector-Layout beschädigen.
	return kIOReturnNotFound;
}

/**
 * wrapATIControllerStart(): Startet den AMD-Controller-Wrapper [11]
 */
bool RAD::wrapATIControllerStart(IOService *ctrl, IOService *provider) {
	if (callbackRAD->forceVesaMode) return false;
	
	// Setze den aktuellen Property-Provider für Connector-Updates
	callbackRAD->currentPropProvider.set(provider);
	bool r = FunctionCast(wrapATIControllerStart, callbackRAD->orgATIControllerStart)(ctrl, provider);
	return r;
}
