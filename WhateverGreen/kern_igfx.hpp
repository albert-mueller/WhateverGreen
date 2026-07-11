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

	// --- Deine neuen Ergänzungen ---
	struct ReadDescriptorPatch {
		static bool globalPageTableRead(void *hardwareGlobalPageTable, uint64_t address, uint64_t &physAddress, uint64_t &flags);
	};
	static OSObject *wrapCopyExistingServices(OSDictionary *matching, IOOptionBits inState, IOOptionBits options);

	// --- Basis für Module ---
	class PatchSubmodule {
	public:
		virtual ~PatchSubmodule() = default;
		bool enabled {false};
		virtual void init() {}
		virtual void processKernel(KernelPatcher &patcher, DeviceInfo *info) {}
		virtual void processGraphicsKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {}
	};

	// --- Module (erforderlich für kern_igfx_pm.cpp) ---
	struct RPSControlPatch : public PatchSubmodule {
		void init() override;
		void processKernel(KernelPatcher &patcher, DeviceInfo *info) override;
		void processFramebufferKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);
		void processGraphicsKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) override;
		static int wrapPmNotifyWrapper(unsigned int a0, unsigned int a1, unsigned long long *a2, unsigned int *freq);
		bool patchRCSCheck(mach_vm_address_t& start);
		uint32_t freq_max {0};
		mach_vm_address_t orgPmNotifyWrapper {};
	} modRPSControlPatch;

	struct ForceWakeWorkaround : public PatchSubmodule {
		void init() override;
		void processGraphicsKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) override;
		static void forceWake(void*, uint8_t set, uint32_t dom, uint32_t ctx);
		bool pollRegister(uint32_t reg, uint32_t val, uint32_t mask, uint32_t timeout);
		bool forceWakeWaitAckFallback(uint32_t d, uint32_t val, uint32_t mask);
	} modForceWakeWorkaround;

	struct FramebufferControllerAccessSupport : public PatchSubmodule {
		void init() override;
		void processKernel(KernelPatcher &patcher, DeviceInfo *info) override;
	} modFramebufferControllerAccessSupport;

	// --- Konfiguration ---
	PatchSubmodule *submodules[3] { &modFramebufferControllerAccessSupport, &modRPSControlPatch, &modForceWakeWorkaround };

	// Register-Zugriffsmethoden (erforderlich für die Logik in kern_igfx_pm.cpp)
	uint32_t readRegister32(void *controller, uint32_t reg);
	void writeRegister32(void *controller, uint32_t reg, uint32_t val);
	void *defaultController();

	enum FirmwareLoad { FW_AUTO = -1, FW_DISABLE = 0, FW_ENABLE = 1, FW_LILU = 2, FW_APPLE = 3 };
	FirmwareLoad fwLoadMode {FW_AUTO};
	bool supportsGuCFirmware {false};
	bool forceSKLAsKBL {false};

	struct ModSettings { bool available {false}; bool enabled {false}; bool legacy {false}; }
	modForceCompleteModeset {}, modBlackScreenFix {}, modDVMTCalcFix {};

	KernelPatcher::KextInfo *getRealFramebuffer(size_t index) {
		return (currentFramebuffer && currentFramebuffer->loadIndex == index) ? currentFramebuffer : currentFramebufferOpt;
	}

private:
	KernelPatcher::KextInfo *currentGraphics {nullptr};
	KernelPatcher::KextInfo *currentFramebuffer {nullptr};
	KernelPatcher::KextInfo *currentFramebufferOpt {nullptr};
	mach_vm_address_t orgCopyExistingServices {};
};

#endif
