//
//  kern_igfx.hpp
//  WhateverGreen
//
//  Copyright © 2018 vit9696. All rights reserved.
//

#ifndef kern_igfx_hpp
#define kern_igfx_hpp

#include "kern_fb.hpp"
#include "kern_igfx_lspcon.hpp"
#include "kern_igfx_backlight.hpp"
#include <Headers/kern_patcher.hpp>
#include <Headers/kern_devinfo.hpp>
#include <Headers/kern_cpu.hpp>
#include <Headers/kern_disasm.hpp>
#include <IOKit/IOService.h>
#include <IOKit/IOLocks.h>

// FIX: Vorwärtsdeklarationen VOR der Klasse IGFX einfügen,
// um die "Unknown type name" Fehler im Editor zu beheben [1, 4, 5].
class AppleIntelFramebufferController;
class AppleIntelPort;

class IGFX {
public:
	/**
	 *  Initialisierungs- und Deinitialisierungsroutinen [6]
	 */
	void init();
	void deinit();

	/**
	 *  Property-Patching und Kext-Verarbeitung [7]
	 */
	void processKernel(KernelPatcher &patcher, DeviceInfo *info);
	bool processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

	/**
	 *  FIX: Statische Instanz für den Zugriff aus Untermodulen (wie GUC).
	 *  Muss 'public' sein, um Zugriffsfehler zu vermeiden [2, 3].
	 */
	static IGFX *callbackIGFX;

	/**
	 *  FIX: GuC Firmware Lade-Modi.
	 *  Verschoben nach 'public', damit GUC den Status prüfen kann [2, 4, 8].
	 */
	enum FirmwareLoad {
		FW_AUTO    = -1 /* Nutze Apple-Standard, deaktiviere für andere */,
		FW_DISABLE = 0  /* Host-Scheduler ohne GuC */,
		FW_LILU    = 1  /* Referenz-Scheduler mit GuC (Injektion) */,
		FW_APPLE   = 2  /* Apple GuC Scheduler */
	};
	
	/**
	 *  Aktueller Firmware-Lademodus [4].
	 */
	FirmwareLoad fwLoadMode {FW_AUTO};

	/**
	 *  Hilfsfunktion zum Abrufen des aktiven Framebuffers [5].
	 */
	KernelPatcher::KextInfo *getRealFramebuffer(size_t index) {
		return (currentFramebuffer && currentFramebuffer->loadIndex == index) ? currentFramebuffer : currentFramebufferOpt;
	}

	// MARK: - Untermodule Schnittstelle [9, 10]

	class PatchSubmodule {
	public:
		virtual ~PatchSubmodule() = default;
		bool enabled {false};
		bool requiresPatchingFramebuffer {false};
		bool requiresPatchingGraphics {false};
		bool requiresGlobalFramebufferControllersAccess {false};
		bool requiresMMIORegistersReadAccess {false};
		bool requiresMMIORegistersWriteAccess {false};

		virtual void init() {}
		virtual void deinit() {}
		virtual void processKernel(KernelPatcher &patcher, DeviceInfo *info) {}
		virtual void processFramebufferKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {}
		virtual void processGraphicsKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {}
		virtual void disableDependentSubmodules() {}
	};

	// MARK: - Gemeinsame Untermodule (Auszug) [11-13]

	class FramebufferControllerAccessSupport : public PatchSubmodule {
		AppleIntelFramebufferController **controllers {};
	public:
		AppleIntelFramebufferController *getController(size_t index) { return controllers[index]; }
		void init() override;
		void processKernel(KernelPatcher &patcher, DeviceInfo *info) override;
		void processFramebufferKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) override;
		void disableDependentSubmodules() override;
	} modFramebufferControllerAccessSupport;

	// (Weitere Submodule wie modMMIORegistersReadSupport, modMMIORegistersWriteSupport folgen hier...)

private:
	/**
	 *  Framebuffer-Patch-Konfiguration [7, 14]
	 */
	union FramebufferPatchFlags {
		struct {
			uint32_t FPFFramebufferId : 1;
			uint32_t FPFFlags : 1;
			uint32_t FPFMobile : 1;
			uint32_t FPFPipeCount : 1;
			uint32_t FPFPortCount : 1;
			uint32_t FPFStolenMemorySize : 1;
			uint32_t FPFFramebufferMemorySize : 1;
			uint32_t FPFUnifiedMemorySize : 1;
		} bits;
		uint32_t value;
	} framebufferPatchFlags {};

	/**
	 *  Kext-Informationen [3]
	 */
	KernelPatcher::KextInfo *currentGraphics {nullptr};
	KernelPatcher::KextInfo *currentFramebuffer {nullptr};
	KernelPatcher::KextInfo *currentFramebufferOpt {nullptr};

	/**
	 *  Originale Funktionszeiger für den Patcher [15, 16]
	 */
	mach_vm_address_t orgCopyExistingServices {};
	mach_vm_address_t orgAcceleratorStart {};
	mach_vm_address_t orgGetOSInformation {};
	mach_vm_address_t orgLoadGuCBinary {};
	mach_vm_address_t orgLoadFirmware {};
	mach_vm_address_t orgInitSchedControl {};
	mach_vm_address_t orgIgBufferWithOptions {};
	mach_vm_address_t orgIgBufferGetGpuVirtualAddress {};

	// (Restliche private Member und Hilfsmethoden [1])
};

#endif /* kern_igfx_hpp */
