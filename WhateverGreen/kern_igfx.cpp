//
//  kern_igfx.cpp
//  WhateverGreen
//
//  Copyright © 2018 vit9696. All rights reserved.
//

#include "kern_igfx.hpp"
#include "kern_fb.hpp"
#include "kern_guc.hpp"
#include "kern_agdc.hpp"
#include "kern_igfx_kexts.hpp"
#include <Headers/kern_api.hpp>
#include <Headers/kern_cpu.hpp>
#include <Headers/kern_devinfo.hpp> // WICHTIG: Erforderlich für BaseDeviceInfo
#include <Headers/kern_file.hpp>
#include <Headers/kern_iokit.hpp>
#include <IOKit/pci/IOPCIDevice.h>

IGFX *IGFX::callbackIGFX;

/**
 * init(): Erkennt die Hardware-Generation und weist Treiber zu.
 * Unterstützt Ivy Bridge bis Ice Lake und bereitet Tahoe vor. [1, 2]
 */
void IGFX::init() {
	callbackIGFX = this;
	
	// Initialisiere Submodule
	for (auto submodule : submodules) submodule->init();
	for (auto submodule : sharedSubmodules) submodule->init();

	auto &bdi = BaseDeviceInfo::get();
	auto generation = bdi.cpuGeneration;

	switch (generation) {
		case CPUInfo::CpuGeneration::SandyBridge:
			currentGraphics = &kextIntelHD3000;
			currentFramebuffer = &kextIntelSNBFb;
			break;
		case CPUInfo::CpuGeneration::IvyBridge:
			// Erhält Legacy-Support für HD 4000 [3]
			currentGraphics = &kextIntelHD4000;
			currentFramebuffer = &kextIntelCapriFb;
			break;
		case CPUInfo::CpuGeneration::Haswell:
			currentGraphics = &kextIntelHD5000;
			currentFramebuffer = &kextIntelAzulFb;
			break;
		case CPUInfo::CpuGeneration::Skylake:
			// Tahoe Fix: Fake SKL als KBL, da Apple SKL-Treiber entfernt hat [1]
			forceSKLAsKBL = getKernelVersion() >= KernelVersion::Ventura || checkKernelArgument("-igfxsklaskbl");
			supportsGuCFirmware = true;
			if (forceSKLAsKBL) {
				currentGraphics = &kextIntelKBL;
				currentFramebuffer = &kextIntelKBLFb;
				modForceCompleteModeset.enabled = true;
			} else {
				currentGraphics = &kextIntelSKL;
				currentFramebuffer = &kextIntelSKLFb;
				modForceCompleteModeset.legacy = true;
			}
			modBlackScreenFix.available = true;
			break;
		case CPUInfo::CpuGeneration::CoffeeLake:
		case CPUInfo::CpuGeneration::CometLake:
			supportsGuCFirmware = true;
			currentGraphics = &kextIntelKBL;
			currentFramebuffer = &kextIntelCFLFb;
			modForceCompleteModeset.enabled = true;
			modBlackScreenFix.available = true;
			break;
		case CPUInfo::CpuGeneration::IceLake:
			supportsGuCFirmware = true;
			currentGraphics = &kextIntelICL;
			currentFramebuffer = &kextIntelICLLPFb;
			modDVMTCalcFix.available = true; // Fix für 60MB DVMT Panics [4]
			break;
		default:
			break;
	}

	if (currentGraphics) lilu.onKextLoadForce(currentGraphics);
	if (currentFramebuffer) lilu.onKextLoadForce(currentFramebuffer);
}

/**
 * processKernel(): Behandelt T2-Support und FileVault-Schutz. [5, 6]
 */
void IGFX::processKernel(KernelPatcher &patcher, DeviceInfo *info) {
	if (info->videoBuiltin) {
		applyFramebufferPatch = loadPatchesFromDevice(info->videoBuiltin, info->reportedFramebufferId);

		// T2-Support: Apple-Hardware erzwingt Apple-GuC-Modus [5]
		if (supportsGuCFirmware && getKernelVersion() >= KernelVersion::HighSierra) {
			if (fwLoadMode == FW_AUTO)
				fwLoadMode = info->firmwareVendor == DeviceInfo::FirmwareVendor::Apple ? FW_APPLE : FW_DISABLE;
		}

		// Schutz für T2-Macs: Deaktiviere FCM, um FileVault 2 nicht zu stören [6]
		if (info->firmwareVendor == DeviceInfo::FirmwareVendor::Apple) {
			modForceCompleteModeset.enabled = false;
			modBlackScreenFix.enabled = false;
			DBGLOG("igfx", "T2-Firmware erkannt: FCM/BSF zum Schutz von FileVault deaktiviert.");
		}

		for (auto submodule : submodules) submodule->processKernel(patcher, info);
	}
}

/**
 * Bug-Fix: Behebt den Global Page Table Crash ( ASUS Z170/Z270 10.14.4+). [7]
 */
bool IGFX::ReadDescriptorPatch::globalPageTableRead(void *hardwareGlobalPageTable, uint64_t address, uint64_t &physAddress, uint64_t &flags) {
	uint64_t pageNumber = address >> PAGE_SHIFT;
	uint64_t pageEntry = getMember<uint64_t *>(hardwareGlobalPageTable, 0x28)[pageNumber];
	physAddress = pageEntry & 0x7FFFFFF000ULL;
	flags = pageEntry & PAGE_MASK;
	// Fix: Prüfe sowohl Present (P) als auch Writeable (W) Bit [7]
	return (flags & 3U) != 0;
}

// MARK: - TODO: Implementierung von copyExistingServices [7]
OSObject *IGFX::wrapCopyExistingServices(OSDictionary *matching, IOOptionBits inState, IOOptionBits options) {
	return FunctionCast(wrapCopyExistingServices, callbackIGFX->orgCopyExistingServices)(matching, inState, options);
}
