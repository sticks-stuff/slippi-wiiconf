/****************************************************************************
 * libwiigui Template
 * Tantric 2009
 *
 * demo.h
 ***************************************************************************/

#ifndef _MAIN_H
#define _MAIN_H

#include "FreeTypeGX.h"

enum {
	METHOD_AUTO,
	METHOD_SD,
	METHOD_USB,
	METHOD_DVD,
	METHOD_SMB,
	METHOD_MC_SLOTA,
	METHOD_MC_SLOTB,
	METHOD_SD_SLOTA,
	METHOD_SD_SLOTB
};

struct SSettings {
    int		AutoLoad;
    int		AutoSave;
    int		LoadMethod;
	int		SaveMethod;
	char	Folder1[256]; // Path to files
	char	Folder2[256]; // Path to files
	char	Folder3[256]; // Path to files
};
extern struct SSettings Settings;


/* Slippi-specific settings.
 *
 * rtc_bias - 32-bits, "difference between RTC and local time" in seconds.
 *	      Specifically, the number of seconds since 1/1/2000 00:00:00.
 * nickname - 32 byte, user-configurable nickname for this console.
 *	      
 */
struct slippi_settings {
	u32	rtc_bias;
	char	nickname[32];
};

void ExitApp();
extern int ExitRequested;
extern FreeTypeGX *fontSystem[];

#endif // _MAIN_H
