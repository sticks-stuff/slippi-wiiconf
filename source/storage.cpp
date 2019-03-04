#include <stdio.h>

#include <ogc/usbstorage.h>
#include <ogc/usb.h>
#include <ogc/disc_io.h>
#include <sdcard/wiisd_io.h>
#include <fat.h>

#include "storage.h"

// Global state
bool sd_card_initialized = false;
bool usb_initialized = false;

bool usb_config(void)
{
	__io_usbstorage.startup();
	if (!__io_usbstorage.isInserted())
	{
		printf("[!] USB storage startup failed\n");
		return false;
	}

	if (!fatMountSimple("usb", &__io_usbstorage))
	{
		printf("[!] Couldn't mount USB storage\n");
		return false;
	}
	usb_initialized = true;
	return true;
}
void usb_shutdown(void) { fatUnmount("usb"); __io_usbstorage.shutdown(); }

bool sd_init(void)
{
	__io_wiisd.startup();
	if (!__io_wiisd.isInserted())
	{
		printf("[!] SD card startup failed\n");
		return false;
	}
	if (!fatMountSimple("sd", &__io_wiisd))
	{
		printf("[!] Couldn't mount SD card storage\n");
		return false;
	}

	sd_card_initialized = true;
	return true;

}
void sd_shutdown(void) { fatUnmount("sd"); __io_wiisd.shutdown(); }

