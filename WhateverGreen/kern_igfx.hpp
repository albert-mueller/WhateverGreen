#ifndef kern_igfx_hpp
#define kern_igfx_hpp

#include "kern_fb.hpp"
#include <Headers/kern_patcher.hpp>
#include <Headers/kern_devinfo.hpp>

// Konstanten für Firmware-Modi
enum { FW_AUTO, FW_APPLE, FW_ENABLE, FW_DISABLE };

class IGFX {
public:
	void init();
	void deinit();
	void processKernel(KernelPatcher &patcher, DeviceInfo *info);
	bool processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

	static IGFX *callbackIGFX;

	// Basis-Struktur für Submodule
	struct PatchSubmodule {
		virtual ~PatchSubmodule() = default;
		bool enabled {false};
		bool available {false};
		bool requiresPatchingGraphics {false};
		bool requiresPatchingFramebuffer {false};
		bool requiresGlobalFramebufferControllersAccess {false};
		bool requiresMMIORegistersReadAccess {false};
		bool requiresMMIORegistersWriteAccess {false};
		virtual void init() {}
		virtual void processKernel(KernelPatcher &patcher, DeviceInfo *info) {}
	};

	// Submodule definieren
	struct RPSControlPatch : public PatchSubmodule {
		void init() override;
		void processKernel(KernelPatcher &patcher, DeviceInfo *info) override;
		void processFramebufferKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);
		void processGraphicsKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);
		static int wrapPmNotifyWrapper(unsigned int a0, unsigned int a1, unsigned long long *a2, unsigned int *freq);
		uint32_t freq_max {0};
		mach_vm_address_t orgPmNotifyWrapper {0};
	} modRPSControlPatch;

	struct ForceWakeWorkaround : public PatchSubmodule {
		void init() override;
		void processGraphicsKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);
	} modForceWakeWorkaround;
	
	struct DVMTCalcFix : public PatchSubmodule { void init() override; } modDVMTCalcFix;
	struct ReadDescriptorPatch : public PatchSubmodule { void init() override; } modReadDescriptorPatch;
	struct ForceCompleteModeset : public PatchSubmodule { void init() override; } modForceCompleteModeset;
	struct BlackScreenFix : public PatchSubmodule { void init() override; } modBlackScreenFix;

	// Pointer-Array für die Initialisierung in kern_igfx.cpp
	PatchSubmodule *submodules[6] { &modDVMTCalcFix, &modReadDescriptorPatch, &modForceCompleteModeset, &modBlackScreenFix, &modRPSControlPatch, &modForceWakeWorkaround };
	
	// Status-Variablen
	KernelPatcher::KextInfo *currentGraphics {nullptr};
	KernelPatcher::KextInfo *currentFramebuffer {nullptr};
	bool supportsGuCFirmware {false};
	bool forceSKLAsKBL {false};
	int fwLoadMode {FW_AUTO};

	// Hilfsmethoden
	uint32_t readRegister32(void *controller, uint32_t reg);
	void writeRegister32(void *controller, uint32_t reg, uint32_t val);
	void *defaultController();
};

#endif
