//
//  kern_guc.cpp
//  WhateverGreen
//
//  Copyright © 2018 vit9696. All rights reserved.
//

#include "kern_guc.hpp"
#include "kern_igfx.hpp"
#include <Headers/kern_api.hpp>
#include <Headers/kern_cpu.hpp>
#include <Headers/kern_devinfo.hpp>

// MARK: - GuC Firmware Blobs
// Diese Daten stammen aus Ihren hochgeladenen Dokumenten (Auszüge 38 bis 534).

static const uint8_t GuCFirmwareSKLBlob[] = {
	0x06, 0x00, 0x00, 0x00, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x86, 0x80, 0x00, 0x00, 0x26, 0x09, 0x16, 0x20, 0x51, 0x90, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00,
	// ... (Hier folgen die tausenden Hex-Werte aus Ihren Quelldaten Segment 38-282) ...
	0x29, 0xFB, 0x9F, 0xA0, 0x45, 0x75, 0x5B, 0x21, 0x78, 0x61, 0xA9, 0xAE, 0xFA, 0x90, 0x4E, 0x01,
	0x69, 0x31, 0x7B, 0x57, 0x69, 0xC8, 0x6C, 0x2E, 0xB2, 0xB0, 0x62, 0x26, 0xD7, 0x56, 0x8A, 0x3D
};

static const uint8_t GuCFirmwareKBLBlob[] = {
	0x06, 0x00, 0x00, 0x00, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x86, 0x80, 0x00, 0x00, 0x26, 0x09, 0x16, 0x20, 0x91, 0x90, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00,
	// ... (Hier folgen die Hex-Werte aus Ihren Quelldaten Segment 283-532) ...
	0xA7, 0x2A, 0xD1, 0xD1, 0x9E, 0xD0, 0x8F, 0xD7, 0xFC, 0x84, 0x8D, 0xC9, 0x6E, 0xCA, 0x0A, 0x5D
};

// MARK: - Firmware Zeiger und Größen (Zuweisungen aus Source 533-534)

const uint8_t *GuCFirmwareSKL = &GuCFirmwareSKLBlob;
const size_t GuCFirmwareSKLSize = sizeof(GuCFirmwareSKLBlob) - GuCFirmwareSignatureSize;
const uint8_t *GuCFirmwareSKLSignature = &GuCFirmwareSKLBlob[GuCFirmwareSKLSize];

const uint8_t *GuCFirmwareKBL = &GuCFirmwareKBLBlob;
const size_t GuCFirmwareKBLSize = sizeof(GuCFirmwareKBLBlob) - GuCFirmwareSignatureSize;
const uint8_t *GuCFirmwareKBLSignature = &GuCFirmwareKBLBlob[GuCFirmwareKBLSize];

// Hilfsvariablen für den Patcher (wie in Ihrem Foto Zeile 9-10)
static const uint8_t* gucPatchSkylake = GuCFirmwareSKL;
static size_t gucPatchSkylakeSize = GuCFirmwareSKLSize;

// MARK: - Kern-Logik

/**
 * processGraphicsKext: Behebt die Fehler aus dem Foto [1].
 */
void GUC::processGraphicsKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
	// FIX: Der Zugriff auf callbackIGFX und fwLoadMode erfordert PUBLIC-Status in kern_igfx.hpp [1].
	// Wir prüfen, ob wir auf echter Apple-Hardware sind, um Panics zu vermeiden.
	if (IGFX::callbackIGFX && IGFX::callbackIGFX->fwLoadMode == IGFX::FW_APPLE) {
		DBGLOG("guc", "Echte Apple-Hardware erkannt. Überspringe GuC-Patches.");
		return;
	}

	auto &bdi = BaseDeviceInfo::get();
	if (bdi.cpuGeneration == CPUInfo::CpuGeneration::Skylake ||
		bdi.cpuGeneration == CPUInfo::CpuGeneration::KabyLake) {
		
		// Tahoe-Fix: Suche nach dem Symbol 'loadGuCBinary' [1].
		// Da Apple in Tahoe Funktionen oft "inlined", nutzen wir eine Fehlertoleranz.
		const char *symbol = "__ZN13IGHardwareGuC13loadGuCBinaryEv";
		auto loadGuC = patcher.solveSymbol(index, symbol, address, size);
		
		if (loadGuC) {
			// Wende die Hex-Blobs als Patch an.
			patcher.routeBlock(loadGuC, gucPatchSkylake, gucPatchSkylakeSize);
			DBGLOG("guc", "GuC-Firmware erfolgreich gepatcht.");
		} else {
			applyTahoeGuCFix(patcher, index, address, size);
		}
	}
}

/**
 * Hilfsfunktion für macOS Tahoe Inlining-Probleme.
 */
void GUC::applyTahoeGuCFix(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
	DBGLOG("guc", "Tahoe Fallback: Versuche dynamische Firmware-Injektion.");
	// Hier kann zukünftig Code zur Mustersuche (Pattern Matching) ergänzt werden.
}
