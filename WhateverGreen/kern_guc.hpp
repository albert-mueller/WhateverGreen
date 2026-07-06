//
//  kern_guc.hpp
//  WhateverGreen
//
//  Copyright © 2018 vit9696. All rights reserved.
//

#ifndef kern_guc_hpp
#define kern_guc_hpp

#include <Headers/kern_patcher.hpp>
#include <libkern/libkern.h>

/**
 * Deklaration der GUC-Klasse.
 * Ohne diesen Block meldet Xcode den Fehler "undeclared identifier".
 */
class GUC {
public:
	/**
	 * Wendet die Firmware-Patches auf den Intel-Treiber an.
	 */
	void processGraphicsKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

private:
	/**
	 * Hilfsfunktion für die dynamische Tahoe-Suche.
	 */
	void applyTahoeGuCFix(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);
};

// MARK: - GuC Firmware Konstanten (Bleiben wie in [1])
static constexpr size_t GuCFirmwareSignatureSize = 256;
extern const uint8_t *GuCFirmwareSKL;
extern const uint8_t *GuCFirmwareSKLSignature;
extern const size_t GuCFirmwareSKLSize;
extern const uint8_t *GuCFirmwareKBL;
extern const uint8_t *GuCFirmwareKBLSignature;
extern const size_t GuCFirmwareKBLSize;

#endif /* kern_guc_hpp */
