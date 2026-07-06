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

// MARK: - Kext-Informationen und Pfade [1-3]

static const char *pathFramebuffer[]            { "/System/Library/Extensions/AMDFramebuffer.kext/Contents/MacOS/AMDFramebuffer" };
static const char *pathRedeonX6000Framebuffer[]  { "/System/Library/Extensions/AMDRadeonX6000Framebuffer.kext/Contents/MacOS/AMDRadeonX6000Framebuffer" };
static const char *pathLegacyFramebuffer[]      { "/System/Library/Extensions/AMDLegacyFramebuffer.kext/Contents/MacOS/AMDLegacyFramebuffer" };
static const char *pathSupport[]                { "/System/Library/Extensions/AMDSupport.kext/Contents/MacOS/AMDSupport" };
static const char *pathLegacySupport[]          { "/System/Library/Extensions/AMDLegacySupport.kext/Contents/MacOS/AMDLegacySupport" };
static const char *pathRadeonX3000[]            { "/System/Library/Extensions/AMDRadeonX3000.kext/Contents/MacOS/AMDRadeonX3000" };
static const char *pathRadeonX4000[]            { "/System/Library/Extensions/AMDRadeonX4000.kext/Contents/MacOS/AMDRadeonX4000" };
static const char *pathRadeonX5000[]            { "/System/Library/Extensions/AMDRadeonX5000.kext/Contents/MacOS/AMDRadeonX5000" };
static const char *pathRadeonX6000[]            { "/System/Library/Extensions/AMDRadeonX6000.kext/Contents/MacOS/AMDRadeonX6000" };

static KernelPatcher::KextInfo kextRadeonFramebuffer       { "com.apple.kext.AMDFramebuffer", pathFramebuffer, 1, {}, {}, KernelPatcher::KextInfo::Unloaded };
static KernelPatcher::KextInfo kextRadeonSupport           { "com.apple.kext.AMDSupport", pathSupport, 1, {}, {}, KernelPatcher::KextInfo::Unloaded };
static KernelPatcher::KextInfo kextRadeonLegacySupport     { "com.apple.kext.AMDLegacySupport", pathLegacySupport, 1, {}, {}, KernelPatcher::KextInfo::Unloaded };
static KernelPatcher::KextInfo kextRadeonX6000Framebuffer  { "com.apple.kext.AMDRadeonX6000Framebuffer", pathRedeonX6000Framebuffer, 1, {}, {}, KernelPatcher::KextInfo::Unloaded };

KernelPatcher::KextInfo kextRadeonHardware[RAD::MaxRadeonHardware] {
	[RAD::IndexRadeonHardwareX3000] = { "com.apple.AMDRadeonX3000", pathRadeonX3000, 1, {}, {}, KernelPatcher::KextInfo::Unloaded },
	[RAD::IndexRadeonHardwareX4000] = { "com.apple.AMDRadeonX4000", pathRadeonX4000, 1, {}, {}, KernelPatcher::KextInfo::Unloaded },
	[RAD::IndexRadeonHardwareX5000] = { "com.apple.AMDRadeonX5000", pathRadeonX5000, 1, {}, {}, KernelPatcher::KextInfo::Unloaded },
	[RAD::IndexRadeonHardwareX6000] = { "com.apple.kext.AMDRadeonX6000", pathRadeonX6000, 1, {}, {}, KernelPatcher::KextInfo::Unloaded }
};

RAD *RAD::callbackRAD;

// MARK: - Kern-Logik [5, 6]

void RAD::init(bool enableNavi10Bkl) {
	callbackRAD = this;
	forceOpenGL = checkKernelArgument("-radgl");
	forceVesaMode = checkKernelArgument("-radvesa");
	force24BppMode = checkKernelArgument("-rad24");
}

void RAD::processKernel(KernelPatcher &patcher, DeviceInfo *info) {
	bool hasAMD = false;
	for (size_t i = 0; i < info->videoExternal.size(); i++) {
		if (info->videoExternal[i].vendor == WIOKit::VendorID::ATIAMD) {
			hasAMD = true;
			if (info->videoExternal[i].video->getProperty("enable-gva-support"))
				enableGvaSupport = true;
		}
	}

	if (hasAMD) {
		// Spezial-Routing für Catalina und neuer (inkl. Tahoe) [6]
		KernelPatcher::RouteRequest requests[] {
			{"__ZNK15IORegistryEntry11getPropertyEPKc", wrapGetProperty, orgGetProperty}
		};
		if (getKernelVersion() >= KernelVersion::Catalina)
			patcher.routeMultipleLong(KernelPatcher::KernelID, requests, arrsize(requests));
		else
			patcher.routeMultiple(KernelPatcher::KernelID, requests);
	} else {
		kextRadeonFramebuffer.switchOff();
	}
}

// MARK: - Kext-Verarbeitung [7-10]

bool RAD::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
	// Radeon X6000 (Navi/Tahoe) Support [7]
	if (kextRadeonX6000Framebuffer.loadIndex == index) {
		KernelPatcher::RouteRequest requests[] = {
			{"_dce_panel_cntl_hw_init", wrapDcePanelCntlHwInit, orgDcePanelCntlHwInit},
			{"__ZN35AMDRadeonX6000_AmdRadeonFramebuffer25setAttributeForConnectionEijm", wrapAMDRadeonX6000AmdRadeonFramebufferSetAttribute, orgAMDRadeonX6000AmdRadeonFramebufferSetAttribute}
		};
		patcher.routeMultiple(index, requests, address, size, true, true);
		return true;
	}

	// Legacy Support [10]
	if (kextRadeonLegacySupport.loadIndex == index) {
		processConnectorOverrides(patcher, address, size, false);
		return true;
	}

	// Verarbeite Hardware-Kexts (Black-Screen-Fix Logik) [10, 18]
	for (size_t i = 0; i < maxHardwareKexts; i++) {
		if (kextRadeonHardware[i].loadIndex == index) {
			processHardwareKext(patcher, i, address, size);
			return true;
		}
	}
	return false;
}

/**
 * FIXED: Behebt den Xcode-Fehler durch Iteration über das Symbol-Array [19, 27].
 * Wendet den Black-Screen-Fix durch Assembly-Patch an.
 */
void RAD::processHardwareKext(KernelPatcher &patcher, size_t hwIndex, mach_vm_address_t address, size_t size) {
	auto getFrame = getFrameBufferProcNames[hwIndex];
	auto &hardware = kextRadeonHardware[hwIndex];

	// Wir müssen durch das Array iterieren, da es mehrere mögliche Symbolnamen geben kann
	for (size_t j = 0; j < MaxGetFrameBufferProcs && getFrame[j] != nullptr; j++) {
		// FIX: Übergeben des einzelnen Symbols (getFrame[j]) mit explizitem Template
		auto getFB = patcher.solveSymbol<mach_vm_address_t>(hardware.loadIndex, getFrame[j], address, size);
		
		if (getFB) {
			// Black-Screen-Fix: Erzwinge Rückgabe von 0 (xor rax, rax; ret) [20]
			uint8_t ret[] {0x48, 0x31, 0xC0, 0xC3};
			patcher.routeBlock(getFB, ret, sizeof(ret));
			DBGLOG("rad", "Black-Screen-Fix angewendet für: %s", getFrame[j]);
			break;
		}
	}

	// Metal-Deaktivierung bei erzwungenem OpenGL [22]
	if (forceOpenGL) {
		uint8_t find[] {0x4D, 0x65, 0x74, 0x61, 0x6C, 0x53, 0x74, 0x61}; // "MetalSta"
		uint8_t repl[] {0x50, 0x65, 0x74, 0x61, 0x6C, 0x53, 0x74, 0x61}; // "PetalSta"
		KernelPatcher::LookupPatch p {&hardware, find, repl, sizeof(find), 2};
		patcher.applyLookupPatch(&p);
		patcher.clearError();
	}
}

// MARK: - Hilfsfunktionen [11, 26]

void RAD::process24BitOutput(KernelPatcher &patcher, KernelPatcher::KextInfo &info, mach_vm_address_t address, size_t size) {
	auto bitsPerComponent = patcher.solveSymbol<int *>(info.loadIndex, "__ZL18BITS_PER_COMPONENT", address, size);
	if (bitsPerComponent && MachInfo::setKernelWriting(true, KernelPatcher::kernelWriteLock) == KERN_SUCCESS) {
		*bitsPerComponent = 8; // Fix von 10-Bit auf 8-Bit [12]
		MachInfo::setKernelWriting(false, KernelPatcher::kernelWriteLock);
	}
}

IOReturn RAD::findProjectByPartNumber(IOService *ctrl, void *properties) {
	// Ignoriere vordefinierte Projektdaten, die oft Connectoren beschädigen (z.B. RX 580) [26]
	return kIOReturnNotFound;
}
