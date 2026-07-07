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
#include <Headers/kern_devinfo.hpp>
#include <Headers/kern_file.hpp>
#include <Headers/kern_iokit.hpp>
#include <IOKit/pci/IOPCIDevice.h>

IGFX *IGFX::callbackIGFX;
static OSObject *(*orgCopyExistingServices)(OSDictionary *matching, IOOptionBits inState, IOOptionBits options) = nullptr;

void IGFX::init() {
	callbackIGFX = this;
	auto &bdi = BaseDeviceInfo::get();
	
	// Initialisiere Submodule erst nach Hardware-Erkennung
	for (auto submodule : submodules) submodule->init();
	for (auto submodule : sharedSubmodules) submodule->init();

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
			// SKL als KBL faken für neuere macOS Versionen
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

	applyFramebufferPatch = loadPatchesFromDevice(info->videoBuiltin, info->reportedFramebufferId);
	auto &bdi = BaseDeviceInfo::get();

	// GuC-Firmware Logik: Legacy (SNB-HSW) deaktivieren, Modern (SKL+) aktivieren
	if (supportsGuCFirmware && getKernelVersion() >= KernelVersion::HighSierra) {
		if (fwLoadMode == FW_AUTO) {
			fwLoadMode = (info->firmwareVendor == DeviceInfo::FirmwareVendor::Apple) ? FW_APPLE :
						 (bdi.cpuGeneration >= CPUInfo::CpuGeneration::Skylake ? FW_ENABLE : FW_DISABLE);
		}
	}

	// T2-Sicherheit: Deaktiviere problematische Fixes bei Apple-Hardware
	if (info->firmwareVendor == DeviceInfo::FirmwareVendor::Apple) {
		modForceCompleteModeset.enabled = false;
		modBlackScreenFix.enabled = false;
	} else if (bdi.cpuGeneration < CPUInfo::CpuGeneration::Skylake) {
		// Legacy-Fix für Sandy/Ivy/Haswell
		modBlackScreenFix.enabled = true;
	}

	for (auto submodule : submodules) submodule->processKernel(patcher, info);
}

// FIX: Sicherer Zugriff auf Global Page Table
bool IGFX::ReadDescriptorPatch::globalPageTableRead(void *hardwareGlobalPageTable, uint64_t address, uint64_t &physAddress, uint64_t &flags) {
	uint64_t pageNumber = address >> PAGE_SHIFT;
	if (pageNumber >= 0x1000) return false;

	uint64_t pageEntry = getMember<uint64_t *>(hardwareGlobalPageTable, 0x28)[pageNumber];
	physAddress = pageEntry & 0x7FFFFFF000ULL;
	flags = pageEntry & PAGE_MASK;
	return (flags & 3U) != 0;
}

// FIX: Keine Endlosschleife mehr durch expliziten Funktions-Pointer
OSObject *IGFX::wrapCopyExistingServices(OSDictionary *matching, IOOptionBits inState, IOOptionBits options) {
	return orgCopyExistingServices(matching, inState, options);
}
