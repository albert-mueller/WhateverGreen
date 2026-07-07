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

class AppleIntelFramebufferController;
class AppleIntelPort;

class IGFX {
public:
	void init();
	void deinit();
	void processKernel(KernelPatcher &patcher, DeviceInfo *info);
	bool processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

	static IGFX *callbackIGFX;

	// Fix: Enums vollständig definiert
	enum FirmwareLoad {
		FW_AUTO    = -1,
		FW_DISABLE = 0,
		FW_ENABLE  = 1,
		FW_LILU    = 2,
		FW_APPLE   = 3
	};

	FirmwareLoad fwLoadMode {FW_AUTO};
	bool supportsGuCFirmware {false};
	bool forceSKLAsKBL {false};

	// Fix: Submodule-Interface
	class PatchSubmodule {
	public:
		virtual ~PatchSubmodule() = default;
		bool enabled {false};
		virtual void init() {}
		virtual void processKernel(KernelPatcher &patcher, DeviceInfo *info) {}
	};

	// Fix: Submodul-Instanzen
	class FramebufferControllerAccessSupport : public PatchSubmodule {
	public:
		void init() override;
		void processKernel(KernelPatcher &patcher, DeviceInfo *info) override;
	} modFramebufferControllerAccessSupport;

	// Fix: Arrays für die Initialisierungsschleifen
	PatchSubmodule *submodules[1] { &modFramebufferControllerAccessSupport };
	PatchSubmodule *sharedSubmodules[0] {};

	// Fix: Modul-Optionen
	struct ModSettings {
		bool available {false};
		bool enabled {false};
		bool legacy {false};
	} modForceCompleteModeset {}, modBlackScreenFix {}, modDVMTCalcFix {};

	KernelPatcher::KextInfo *getRealFramebuffer(size_t index) {
		return (currentFramebuffer && currentFramebuffer->loadIndex == index) ? currentFramebuffer : currentFramebufferOpt;
	}

private:
	KernelPatcher::KextInfo *currentGraphics {nullptr};
	KernelPatcher::KextInfo *currentFramebuffer {nullptr};
	KernelPatcher::KextInfo *currentFramebufferOpt {nullptr};

	mach_vm_address_t orgCopyExistingServices {};
};

#endif /* kern_igfx_hpp */
