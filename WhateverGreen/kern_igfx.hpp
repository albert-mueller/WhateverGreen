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

class IGFX {
public:
	void init();
	void deinit();
	
	/**
	 *  Property patching routine
	 */
	void processKernel(KernelPatcher &patcher, DeviceInfo *info);
	
	/**
	 *  Patch kext if needed and prepare other patches
	 */
	bool processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

	/**
	 *  FIX: Statische Instanz für den Zugriff aus Submodulen (wie GUC) [5].
	 */
	static IGFX *callbackIGFX;

	/**
	 *  FIX: GuC Firmware Lade-Schema [8, 9].
	 *  Wurde von private nach public verschoben, um den "private member" Fehler zu beheben.
	 */
	enum FirmwareLoad {
		FW_AUTO    = -1 /* Nutze Apple-Standard, deaktiviere für andere */,
		FW_DISABLE = 0  /* Host-Scheduler ohne GuC */,
		FW_LILU    = 1  /* Referenz-Scheduler mit GuC (Lilu-Injektion) */,
		FW_APPLE   = 2  /* Apple GuC Scheduler */
	};
	
	/**
	 *  Aktueller Firmware-Lademodus.
	 */
	FirmwareLoad fwLoadMode {FW_AUTO};

	/**
	 *  Gibt den aktuell verwendeten Framebuffer zurück [10].
	 */
	KernelPatcher::KextInfo *getRealFramebuffer(size_t index) {
		return (currentFramebuffer && currentFramebuffer->loadIndex == index) ? currentFramebuffer : currentFramebufferOpt;
	}

	// MARK: - Patch Submodule & Injection Kits [10-14]

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

	// MARK: - Shared Submodules [14-19]

	class FramebufferControllerAccessSupport : public PatchSubmodule {
		AppleIntelFramebufferController **controllers {};
	public:
		AppleIntelFramebufferController *getController(size_t index) { return controllers[index]; }
		void init() override;
		void processKernel(KernelPatcher &patcher, DeviceInfo *info) override;
		void processFramebufferKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) override;
		void disableDependentSubmodules() override;
	} modFramebufferControllerAccessSupport;

	// (Hier folgen weitere Submodule wie MMIORegistersReadSupport, MMIORegistersWriteSupport, etc.) [20-61]

private:
	/**
	 *  Framebuffer patch flags [2, 3]
	 */
	union FramebufferPatchFlags {
		struct {
			uint32_t FPFFramebufferId : 1;
			uint32_t FPFFlags : 1;
			uint32_t FPFCamelliaVersion : 1;
			uint32_t FPFMobile : 1;
			uint32_t FPFPipeCount : 1;
			uint32_t FPFPortCount : 1;
			uint32_t FPFFBMemoryCount : 1;
			uint32_t FPFStolenMemorySize : 1;
			uint32_t FPFFramebufferMemorySize : 1;
			uint32_t FPFUnifiedMemorySize : 1;
			uint32_t FPFFramebufferCursorSize : 1;
		} bits;
		uint32_t value;
	} framebufferPatchFlags {};

	/**
	 *  Interne Kext-Infos [5]
	 */
	KernelPatcher::KextInfo *currentGraphics {nullptr};
	KernelPatcher::KextInfo *currentFramebuffer {nullptr};
	KernelPatcher::KextInfo *currentFramebufferOpt {nullptr};

	/**
	 *  Originale Funktionszeiger für den Patcher [6, 7]
	 */
	mach_vm_address_t orgCopyExistingServices {};
	mach_vm_address_t orgAcceleratorStart {};
	mach_vm_address_t orgGetOSInformation {};
	mach_vm_address_t orgLoadGuCBinary {};
	mach_vm_address_t orgLoadFirmware {};
	mach_vm_address_t orgInitSchedControl {};
	mach_vm_address_t orgIgBufferWithOptions {};
	mach_vm_address_t orgIgBufferGetGpuVirtualAddress {};

	// (Restliche private Member und Helper-Methoden) [7-75]
};

#endif /* kern_igfx_hpp */
