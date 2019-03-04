/****************************************************************************
 * libwiigui Template
 * Tantric 2009
 *
 * demo.cpp
 * Basic template/demonstration of libwiigui capabilities. For a
 * full-featured app using many more extensions, check out Snes9x GX.
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <unistd.h>
#include <wiiuse/wpad.h>
#include <fat.h>

#include <ogc/conf.h>

#include "FreeTypeGX.h"
#include "video.h"
#include "audio.h"
#include "menu.h"
#include "input.h"
#include "filelist.h"
#include "main.h"

#include "storage.h"
#include "util.h"

struct SSettings Settings;

extern bool sd_card_initialized;

// Current slippi settings
struct slippi_settings settings;

// Current network settings
u8 network_settings[0x1b60] ATTRIBUTE_ALIGN(32) = {0};

// Current RTC bias from SYSCONF
u32 current_bias = 0;

//u32 __getrtc(u32 *gctime)
//{
//	u32 ret;
//	u32 cmd;
//	u32 time;
//
//	if(EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_1,NULL)==0) return 0;
//	if(EXI_Select(EXI_CHANNEL_0,EXI_DEVICE_1,EXI_SPEED8MHZ)==0) {
//		EXI_Unlock(EXI_CHANNEL_0);
//		return 0;
//	}
//
//	ret = 0;
//	time = 0;
//	cmd = 0x20000000;
//	if(EXI_Imm(EXI_CHANNEL_0,&cmd,4,EXI_WRITE,NULL)==0) ret |= 0x01;
//	if(EXI_Sync(EXI_CHANNEL_0)==0) ret |= 0x02;
//	if(EXI_Imm(EXI_CHANNEL_0,&time,4,EXI_READ,NULL)==0) ret |= 0x04;
//	if(EXI_Sync(EXI_CHANNEL_0)==0) ret |= 0x08;
//	if(EXI_Deselect(EXI_CHANNEL_0)==0) ret |= 0x10;
//
//	EXI_Unlock(EXI_CHANNEL_0);
//	*gctime = time;
//	if(ret) return 0;
//
//	return 1;
//}
//
//u32 __SYS_GetRTC(u32 *gctime)
//{
//	u32 cnt,ret;
//	u32 time1,time2;
//
//	cnt = 0;
//	ret = 0;
//	while(cnt<16) {
//		if(__getrtc(&time1)==0) ret |= 0x01;
//		if(__getrtc(&time2)==0) ret |= 0x02;
//		if(ret) return 0;
//		if(time1==time2) {
//			*gctime = time1;
//			return 1;
//		}
//		cnt++;
//	}
//	return 0;
//}





int ExitRequested = 0;
void ExitApp()
{
	ShutoffRumble();
	StopGX();
	sd_shutdown();
	exit(0);
}

void DefaultSettings()
{
	Settings.LoadMethod = METHOD_AUTO;
	Settings.SaveMethod = METHOD_AUTO;
	sprintf (Settings.Folder1,"libwiigui/first folder");
	sprintf (Settings.Folder2,"libwiigui/second folder");
	sprintf (Settings.Folder3,"libwiigui/third folder");
	Settings.AutoLoad = 1;
	Settings.AutoSave = 1;
}


int main(int argc, char *argv[])
{
	s32 res;
	u32 current_rtc;
	FILE *slippi_fp;

	// Explicitly clear out our representation of Slippi settings
	memset(&settings, 0, sizeof(struct slippi_settings));

	InitVideo(); // Initialize video
	SetupPads(); // Initialize input
	InitAudio(); // Initialize audio

	// For now, just die here if we can't initialize the SD card
	if (!sd_init())
	{
		printf("    Couldn't initialize the SD card ... quitting in 5s.\n");
		sleep(5);
		ExitApp();
	}

	res = CONF_GetCounterBias(&current_bias);
	printf("    got %08x from GetCounterBias\n", current_bias);

	/* Try opening the Slippi configuration file on SD card. If we can't 
	 * open for reading, the file doesn't exist, and we need to create one. 
	 * Otherwise, just read the contents of the file into memory.
	 */

	slippi_fp = fopen("sd:/slippi_console.dat", "rb");
	if (!slippi_fp) {

		// Write a new, empty configuration file
		slippi_fp = fopen("sd:/slippi_console.dat", "wb");

		// Start with the current RTC bias from SYSCONF
		settings.rtc_bias = current_bias;
		printf("    settings.rtc_bias = %08x\n", settings.rtc_bias);

		res = fwrite(&settings, 1, sizeof(struct slippi_settings), slippi_fp);
		printf("    wrote %08x bytes to slippi_console.dat\n", res);
		fclose(slippi_fp);
	}
	else
	{
		res = fread(&settings, 1, sizeof(struct slippi_settings), slippi_fp);
		printf("    read %08x bytes from slippi_console.dat\n", res);
		fclose(slippi_fp);

		printf("    settings.rtc_bias = %08x\n", settings.rtc_bias);
		//sleep(5);
	}

	//fatInitDefault(); // Initialize file system
	InitFreeType((u8*)font_ttf, font_ttf_size); // Initialize font system
	InitGUIThreads(); // Initialize GUI

	// Set default settings
	DefaultSettings();

	// Dispatch the main menu
	MenuInit();
}
