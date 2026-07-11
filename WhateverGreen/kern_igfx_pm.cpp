//
//  kern_igfx_pm.cpp
//  WhateverGreen
//
//  Created by Pb on 22/06/2020.
//  Copyright © 2020 vit9696. All rights reserved.
//

#include <IOKit/IOService.h>
#include <Headers/kern_patcher.hpp>
#include <Headers/kern_devinfo.hpp>
#include <Headers/kern_cpu.hpp>
#include <Headers/kern_disasm.hpp>
#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <mach/clock.h>

#include "kern_igfx.hpp"

// Fix für den Fehler "Use of undeclared identifier 'PmNotifyWrapperFunc'"
typedef int (*PmNotifyWrapperFunc)(unsigned int, unsigned int, unsigned long long *, unsigned int *);

namespace {
constexpr const char* log = "igfx_pm";

constexpr uint32_t MCHBAR_MIRROR_BASE_SNB = 0x140000;
constexpr uint32_t GEN6_RP_STATE_CAP = MCHBAR_MIRROR_BASE_SNB + 0x5998;
constexpr uint32_t GEN9_FREQUENCY_SHIFT = 23;
constexpr uint32_t GEN9_FREQ_SCALER  = 3;
constexpr uint32_t FORCEWAKE_KERNEL_FALLBACK = 1 << 15;
constexpr uint32_t FORCEWAKE_ACK_TIMEOUT_MS = 50;

constexpr uint32_t FORCEWAKE_MEDIA_GEN9 = 0xa270;
constexpr uint32_t FORCEWAKE_RENDER_GEN9 = 0xa278;
constexpr uint32_t FORCEWAKE_BLITTER_GEN9 = 0xa188;

constexpr uint32_t FORCEWAKE_ACK_MEDIA_GEN9 = 0x0D88;
constexpr uint32_t FORCEWAKE_ACK_RENDER_GEN9 = 0x0D84;
constexpr uint32_t FORCEWAKE_ACK_BLITTER_GEN9 = 0x130044;

enum FORCEWAKE_DOM_BITS : unsigned {
	DOM_RENDER = 0b001,
	DOM_MEDIA = 0b010,
	DOM_BLITTER = 0b100,
	DOM_LAST = DOM_BLITTER,
	DOM_FIRST = DOM_RENDER
};

constexpr uint32_t regForDom(unsigned d) {
	if (d == DOM_RENDER) return FORCEWAKE_RENDER_GEN9;
	if (d == DOM_MEDIA) return FORCEWAKE_MEDIA_GEN9;
	if (d == DOM_BLITTER) return FORCEWAKE_BLITTER_GEN9;
	return 0;
}

constexpr uint32_t ackForDom(unsigned d) {
	if (d == DOM_RENDER) return FORCEWAKE_ACK_RENDER_GEN9;
	if (d == DOM_MEDIA) return FORCEWAKE_ACK_MEDIA_GEN9;
	if (d == DOM_BLITTER) return FORCEWAKE_ACK_BLITTER_GEN9;
	return 0;
}

constexpr uint32_t masked_field(uint32_t mask, uint32_t value) { return (mask << 16) | value; }
constexpr uint32_t fw_set(uint32_t v) { return masked_field(v, v); }
constexpr uint32_t fw_clear(uint32_t v) { return masked_field(v, 0); }
}

// MARK: - RPS Control Patch

void IGFX::RPSControlPatch::init() {
	requiresPatchingGraphics = true;
	requiresPatchingFramebuffer = true;
	requiresGlobalFramebufferControllersAccess = true;
	requiresMMIORegistersReadAccess = true;
}

void IGFX::RPSControlPatch::processKernel(KernelPatcher &patcher, DeviceInfo *info) {
	uint32_t rpsc = 0;
	if (PE_parse_boot_argn("igfxrpsc", &rpsc, sizeof(rpsc)) ||
		WIOKit::getOSDataValue(info->videoBuiltin, "rps-control", rpsc)) {
		enabled = rpsc > 0 && available;
	}
}

void IGFX::RPSControlPatch::processFramebufferKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
	KernelPatcher::RouteRequest routeRequest = {
		"__ZL15pmNotifyWrapperjjPyPj",
		wrapPmNotifyWrapper,
		orgPmNotifyWrapper
	};
	if (!patcher.routeMultiple(index, &routeRequest, 1, address, size))
		SYSLOG(log, "Failed to route pmNotifyWrapper.");
}

int IGFX::RPSControlPatch::wrapPmNotifyWrapper(unsigned int a0, unsigned int a1, unsigned long long *a2, unsigned int *freq) {
	uint32_t cfreq = 0;
	// Expliziter Cast auf den Typ-Alias
	((PmNotifyWrapperFunc)callbackIGFX->modRPSControlPatch.orgPmNotifyWrapper)(a0, a1, a2, &cfreq);
	
	if (!callbackIGFX->modRPSControlPatch.freq_max) {
		callbackIGFX->modRPSControlPatch.freq_max = callbackIGFX->readRegister32(callbackIGFX->defaultController(), GEN6_RP_STATE_CAP) & 0xFF;
	}
	
	*freq = (GEN9_FREQ_SCALER << GEN9_FREQUENCY_SHIFT) * callbackIGFX->modRPSControlPatch.freq_max;
	return 0;
}

// MARK: - Force Wake Workaround

bool IGFX::ForceWakeWorkaround::pollRegister(uint32_t reg, uint32_t val, uint32_t mask, uint32_t timeout) {
	AbsoluteTime now, deadline;
	clock_interval_to_deadline(timeout, kMillisecondScale, &deadline);
	for (clock_get_uptime(&now); now < deadline; clock_get_uptime(&now)) {
		auto rd = callbackIGFX->readRegister32(callbackIGFX->defaultController(), reg);
		if ((rd & mask) == val) return true;
	}
	return false;
}

bool IGFX::ForceWakeWorkaround::forceWakeWaitAckFallback(uint32_t d, uint32_t val, uint32_t mask) {
	unsigned pass = 1;
	bool ack = false;
	auto controller = callbackIGFX->defaultController();
	do {
		pollRegister(ackForDom(d), 0, FORCEWAKE_KERNEL_FALLBACK, FORCEWAKE_ACK_TIMEOUT_MS);
		callbackIGFX->writeRegister32(controller, regForDom(d), fw_set(FORCEWAKE_KERNEL_FALLBACK));
		IODelay(10 * pass);
		pollRegister(ackForDom(d), FORCEWAKE_KERNEL_FALLBACK, FORCEWAKE_KERNEL_FALLBACK, FORCEWAKE_ACK_TIMEOUT_MS);
		ack = (callbackIGFX->readRegister32(controller, ackForDom(d)) & mask) == val;
		callbackIGFX->writeRegister32(controller, regForDom(d), fw_clear(FORCEWAKE_KERNEL_FALLBACK));
	} while (!ack && pass++ < 10);
	return ack;
}

void IGFX::ForceWakeWorkaround::forceWake(void*, uint8_t set, uint32_t dom, uint32_t ctx) {
	uint32_t ack_exp = set << ctx;
	uint32_t mask = 1 << ctx;
	uint32_t wr = ack_exp | (1 << ctx << 16);
	
	for (unsigned d = DOM_FIRST; d <= DOM_LAST; d <<= 1)
		if (dom & d) {
			callbackIGFX->writeRegister32(callbackIGFX->defaultController(), regForDom(d), wr);
			IOPause(100);
			// KORREKTUR: Aufruf über das Instanz-Objekt
			if (!callbackIGFX->modForceWakeWorkaround.pollRegister(ackForDom(d), ack_exp, mask, FORCEWAKE_ACK_TIMEOUT_MS) &&
				!callbackIGFX->modForceWakeWorkaround.forceWakeWaitAckFallback(d, ack_exp, mask) &&
				!callbackIGFX->modForceWakeWorkaround.pollRegister(ackForDom(d), ack_exp, mask, FORCEWAKE_ACK_TIMEOUT_MS))
				PANIC(log, "ForceWake timeout");
		}
}
