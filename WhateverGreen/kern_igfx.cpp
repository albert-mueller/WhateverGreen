//
//  kern_igfx.cpp
//  WhateverGreen
//
//  Copyright © 2018 vit9696. All rights reserved.
//

#include "kern_igfx.hpp"
#include "kern_igfx_kexts.hpp"
#include "kern_fb.hpp"
#include <Headers/kern_api.hpp>
#include <Headers/kern_cpu.hpp>
#include <Headers/kern_devinfo.hpp>

IGFX *IGFX::callbackIGFX;

void IGFX::init() {
	callbackIGFX = this;
	
	// Initialisierung der Submodule
	for (auto submodule : this->submodules) submodule->init();
	
	auto &bdi = BaseDeviceInfo::get();
	
	switch (bdi.cpuGeneration) {
		case CPUInfo::CpuGeneration::SandyBridge:
			currentGraphics = &kextIntelHD3000;
			currentFramebuffer = &kextIntelSNBFb;
			break;
		case CPUInfo::CpuGeneration::IvyBridge:
			currentGraphics = &kextIntelHD4000;
			currentFramebuffer = &kextIntelCapriFb;
			break;
		case CPUInfo::CpuGeneration::Haswell:
			currentGraphics = &kextIntelHD5000;
			currentFramebuffer = &kextIntelAzulFb;
			break;
		case CPUInfo::CpuGeneration::Skylake:
			forceSKLAsKBL = getKernelVersion() >= KernelVersion::Ventura || checkKernelArgument("-igfxsklaskbl");
			supportsGuCFirmware = true;
			currentGraphics = forceSKLAsKBL ? &kextIntelKBL : &kextIntelSKL;
			currentFramebuffer = forceSKLAsKBL ? &kextIntelKBLFb : &kextIntelSKLFb;
			modBlackScreenFix.available = true;
			break;
		case CPUInfo::CpuGeneration::CoffeeLake:
		case CPUInfo::CpuGeneration::CometLake:
			supportsGuCFirmware = true;
			currentGraphics = &kextIntelKBL;
			currentFramebuffer = &kextIntelCFLFb;
			modBlackScreenFix.available = true;
			break;
		case CPUInfo::CpuGeneration::IceLake:
			supportsGuCFirmware = true;
			currentGraphics = &kextIntelICL;
			currentFramebuffer = &kextIntelICLLPFb;
			modDVMTCalcFix.available = true;
			break;
		default:
			break;
	}

	if (currentGraphics) lilu.onKextLoadForce(currentGraphics);
	if (currentFramebuffer) lilu.onKextLoadForce(currentFramebuffer);
}

void IGFX::processKernel(KernelPatcher &patcher, DeviceInfo *info) {
	if (!info->videoBuiltin) return;
	
	auto &bdi = BaseDeviceInfo::get();

	// GuC-Logik: Aktivieren für moderne Hardware, deaktivieren für Legacy
	if (supportsGuCFirmware && getKernelVersion() >= KernelVersion::HighSierra) {
		if (fwLoadMode == FW_AUTO)
			fwLoadMode = (info->firmwareVendor == DeviceInfo::FirmwareVendor::Apple) ? FW_APPLE :
						 (bdi.cpuGeneration >= CPUInfo::CpuGeneration::Skylake ? FW_ENABLE : FW_DISABLE);
	}

	// T2-Sicherheitsschutz
	if (info->firmwareVendor == DeviceInfo::FirmwareVendor::Apple) {
		modForceCompleteModeset.enabled = false;
		modBlackScreenFix.enabled = false;
	} else if (bdi.cpuGeneration < CPUInfo::CpuGeneration::Skylake) {
		modBlackScreenFix.enabled = true;
	}

	for (auto submodule : this->submodules) submodule->processKernel(patcher, info);
}

// Implementierung der Global Page Table Read (Sicherheits-Fix)
bool IGFX::ReadDescriptorPatch::globalPageTableRead(void *hardwareGlobalPageTable, uint64_t address, uint64_t &physAddress, uint64_t &flags) {
	uint64_t pageNumber = address >> PAGE_SHIFT;
	if (pageNumber >= 0x1000) return false;

	uint64_t pageEntry = getMember<uint64_t *>(hardwareGlobalPageTable, 0x28)[pageNumber];
	physAddress = pageEntry & 0x7FFFFFF000ULL;
	flags = pageEntry & PAGE_MASK;
	return (flags & 3U) != 0;
}

// Korrekte Wrapper-Funktion mittels Cast
OSObject *IGFX::wrapCopyExistingServices(OSDictionary *matching, IOOptionBits inState, IOOptionBits options) {
	typedef OSObject *(*t_copyExistingServices)(OSDictionary *matching, IOOptionBits inState, IOOptionBits options);
	return reinterpret_cast<t_copyExistingServices>(callbackIGFX->orgCopyExistingServices)(matching, inState, options);
}
