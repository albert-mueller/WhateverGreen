#ifndef kern_igfx_hpp
#define kern_igfx_hpp

#include "kern_fb.hpp"
#include <Headers/kern_patcher.hpp>
#include <Headers/kern_devinfo.hpp>
typedef int (*PmNotifyWrapperFunc)(unsigned int, unsigned int, unsigned long long *, unsigned int *);
class IGFX {
public:
	void init();
	void deinit();
	void processKernel(KernelPatcher &patcher, DeviceInfo *info);
	bool processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

	static IGFX *callbackIGFX;

	// Basis-Klasse für alle Module
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
		virtual void processGraphicsKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {}
	};

	// Modul-Instanzen müssen hier definiert sein
	struct RPSControlPatch : public PatchSubmodule {
		void init() override;
		void processKernel(KernelPatcher &patcher, DeviceInfo *info) override;
		void processFramebufferKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);
		void processGraphicsKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) override;
		static int wrapPmNotifyWrapper(unsigned int a0, unsigned int a1, unsigned long long *a2, unsigned int *freq);
		bool patchRCSCheck(mach_vm_address_t& start);
		uint32_t freq_max {0};
		mach_vm_address_t orgPmNotifyWrapper {0};
	} modRPSControlPatch;

	struct ForceWakeWorkaround : public PatchSubmodule {
		void init() override;
		void processGraphicsKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) override;
		static void forceWake(void*, uint8_t set, uint32_t dom, uint32_t ctx);
		bool pollRegister(uint32_t reg, uint32_t val, uint32_t mask, uint32_t timeout);
		bool forceWakeWaitAckFallback(uint32_t d, uint32_t val, uint32_t mask);
	} modForceWakeWorkaround;

	// Hilfsmethoden für den Registerzugriff
	uint32_t readRegister32(void *controller, uint32_t reg);
	void writeRegister32(void *controller, uint32_t reg, uint32_t val);
	void *defaultController();
};

#endif
