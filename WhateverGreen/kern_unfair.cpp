#include "kern_unfair.hpp"
#include "kern_weg.hpp"
#include <Headers/kern_api.hpp>
#include <Headers/kern_file.hpp>    // WICHTIG: Behebt vnode & vn_getpath Fehler
#include <Headers/kern_user.hpp>    // WICHTIG: Behebt matchSharedCachePath Fehler
#include <Headers/kern_devinfo.hpp>
#include <IOKit/IODeviceTreeSupport.h>

UNFAIR *UNFAIR::callbackUNFAIR;

void UNFAIR::init() {
	callbackUNFAIR = this;
	disableUnfair = checkKernelArgument("-unfairoff");
}

void UNFAIR::csValidatePage(vnode *vp, memory_object_t pager, memory_object_offset_t page_offset, const void *data, int *validated_p, int *tainted_p, int *nx_p) {
	// Zuerst die Originalfunktion aufrufen
	FunctionCast(csValidatePage, callbackUNFAIR->orgCsValidatePage)(vp, pager, page_offset, data, validated_p, tainted_p, nx_p);

	char path[PATH_MAX];
	int pathlen = PATH_MAX;
	if (vn_getpath(vp, path, &pathlen) == 0) {
		// DRM Patches für Tahoe
		if ((callbackUNFAIR->unfairGva & UnfairDyldSharedCache) != 0 && UserPatcher::matchSharedCachePath(path)) {
			if ((callbackUNFAIR->unfairGva & UnfairRelaxHdcpRequirements) != 0) {
				// FIX: Größe auf [] geändert, damit Xcode die 29 Bytes automatisch zählt
				static const uint8_t find[] = {
					0x4D, 0x61, 0x63, 0x50, 0x72, 0x6F, 0x35, 0x2C, 0x31, 0x00, 0x4D, 0x61, 0x63, 0x50, 0x72, 0x6F,
					0x36, 0x2C, 0x31, 0x00, 0x49, 0x4F, 0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65
				};
				if (UNLIKELY(KernelPatcher::findAndReplace(const_cast<void *>(data), PAGE_SIZE, find, sizeof(find), BaseDeviceInfo::get().modelIdentifier, 20))) {
					DBGLOG("unfair", "DRM-Modell erfolgreich gepatcht.");
				}
			}
		}
	}
}

void UNFAIR::processKernel(KernelPatcher &patcher, DeviceInfo *info) {
	if (disableUnfair) return;

	WEG::getVideoArgument(info, "unfairgva", &unfairGva, sizeof(unfairGva));
	if (unfairGva == 0) return;

	if ((unfairGva & UnfairCustomAppleGvaBoardId) != 0) {
		auto entry = IORegistryEntry::fromPath("/", gIODTPlane);
		if (entry) {
			// FIX: Korrekter Cast für IOKit-Längenangabe
			entry->setProperty("hwgva-id", const_cast<char *>("Mac-7BA5B2D9E42DDD94"), static_cast<uint32_t>(sizeof("Mac-7BA5B2D9E42DDD94")));
			entry->release();
		}
	}

	// Routing der Tahoe Validierungs-Routine
	KernelPatcher::RouteRequest csRoute("_cs_validate_page", csValidatePage, orgCsValidatePage);
	if (!patcher.routeMultipleLong(KernelPatcher::KernelID, &csRoute, 1)) {
		SYSLOG("unfair", "Fehler beim Routing der CS-Validierung.");
	}
}
