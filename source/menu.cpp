/****************************************************************************
 * libwiigui Template
 * Tantric 2009
 *
 * menu.cpp
 * Menu flow routines - handles all menu logic
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>

#include "libwiigui/gui.h"
#include "menu.h"
#include "main.h"
#include "input.h"
#include "filelist.h"
#include "filebrowser.h"

#include "util.h"
#include "netconf.h"
#include "Slippi.h"

#define THREAD_SLEEP 100

// Global state
static GuiImageData * pointer[4];
static GuiImage * bgImg = NULL;
static GuiWindow * mainWindow = NULL;
static lwp_t guithread = LWP_THREAD_NULL;
static bool guiHalt = true;

// App-specific global state
int user_accepted_disclaimer = 0;
extern bool sd_card_initialized;

const char *sd_title = "WARNING:";
const char *sd_msg = "Could not initialize the SD card. Exiting the application.";
const char *sd_button1 = "OK";

const char *disclaimer_title = "WARNING:";
const char *disclaimer_button1 = "I Understand";
const char *disclaimer_button2 = "Exit";
const char *disclaimer_msg = 
	"This is a homebrew application for changing system settings."
	"If something goes wrong, this software has the potential to "
	"damage your Wii. If you aren't willing to accept this risk, "
	"please terminate the application by selecting 'Exit' now.";

extern struct slippi_settings settings;


char month_string[12][32] = { 
	"January",
	"February",
	"March",
	"April",
	"May",
	"June",
	"July",
	"August",
	"September",
	"October",
	"November",
	"December",
};

/* Resume GUI thread execution
 */
static void
ResumeGui()
{
	guiHalt = false;
	LWP_ResumeThread (guithread);
}

/* Wait for the GUI thread to stop. Call this before changing GUI elements
 */
static void HaltGui()
{
	guiHalt = true;

	// wait for thread to finish
	while(!LWP_ThreadIsSuspended(guithread))
		usleep(THREAD_SLEEP);
}


/* Display some window to the user
 */
int WindowPrompt(const char *title, const char *msg, 
	const char *btn1Label, const char *btn2Label)
{
	int choice = -1;

	GuiWindow promptWindow(448,288);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	//GuiText titleTxt(title, 26, (GXColor){0, 0, 0, 255});
	//titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	//titleTxt.SetPosition(0,40);

	GuiText msgTxt(msg, 18, (GXColor){0, 0, 0, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	msgTxt.SetPosition(0,-20);
	msgTxt.SetWrap(true, 400);

	GuiText btn1Txt(btn1Label, 20, (GXColor){0, 0, 0, 255});
	GuiImage btn1Img(&btnOutline);
	GuiImage btn1ImgOver(&btnOutlineOver);
	GuiButton btn1(btnOutline.GetWidth(), btnOutline.GetHeight());

	if(btn2Label)
	{
		btn1.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
		btn1.SetPosition(20, -25);
	}
	else
	{
		btn1.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
		btn1.SetPosition(0, -25);
	}

	btn1.SetLabel(&btn1Txt);
	btn1.SetImage(&btn1Img);
	btn1.SetImageOver(&btn1ImgOver);
	btn1.SetTrigger(&trigA);
	btn1.SetState(STATE_SELECTED);
	btn1.SetEffectGrow();

	GuiText btn2Txt(btn2Label, 20, (GXColor){0, 0, 0, 255});
	GuiImage btn2Img(&btnOutline);
	GuiImage btn2ImgOver(&btnOutlineOver);
	GuiButton btn2(btnOutline.GetWidth(), btnOutline.GetHeight());
	btn2.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	btn2.SetPosition(-20, -25);
	btn2.SetLabel(&btn2Txt);
	btn2.SetImage(&btn2Img);
	btn2.SetImageOver(&btn2ImgOver);
	btn2.SetTrigger(&trigA);
	btn2.SetEffectGrow();

	promptWindow.Append(&dialogBoxImg);
	//promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);
	promptWindow.Append(&btn1);

	if(btn2Label)
		promptWindow.Append(&btn2);

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 50);
	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);
	ResumeGui();

	while(choice == -1)
	{
		usleep(THREAD_SLEEP);

		if(btn1.GetState() == STATE_CLICKED)
			choice = 1;
		else if(btn2.GetState() == STATE_CLICKED)
			choice = 0;
	}

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
	while(promptWindow.GetEffect() > 0) usleep(THREAD_SLEEP);
	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
	return choice;
}


/* Primary thread for handling GUI state change, application exit, etc 
 */
static void *UpdateGUI(void *arg)
{
	int i;

	while(1)
	{
		if(guiHalt)
		{
			LWP_SuspendThread(guithread);
		}
		else
		{
			UpdatePads();
			mainWindow->Draw();

			#ifdef HW_RVL
			for(i=3; i >= 0; i--) // so that player 1's cursor appears on top!
			{
				if(userInput[i].wpad->ir.valid)
					Menu_DrawImg(userInput[i].wpad->ir.x-48, userInput[i].wpad->ir.y-48,
						96, 96, pointer[i]->GetImage(), userInput[i].wpad->ir.angle, 1, 1, 255);
				DoRumble(i);
			}
			#endif

			Menu_Render();

			for(i=0; i < 4; i++)
				mainWindow->Update(&userInput[i]);

			if(ExitRequested)
			{
				for(i = 0; i <= 255; i += 15)
				{
					mainWindow->Draw();
					Menu_DrawRectangle(0,0,screenwidth,screenheight,(GXColor){0, 0, 0, i},1);
					Menu_Render();
				}
				ExitApp();
			}
		}
	}
	return NULL;
}

/****************************************************************************
 * InitGUIThread
 *
 * Startup GUI threads
 ***************************************************************************/
void
InitGUIThreads()
{
	LWP_CreateThread (&guithread, UpdateGUI, NULL, NULL, 0, 70);
}

/****************************************************************************
 * OnScreenKeyboard
 *
 * Opens an on-screen keyboard window, with the data entered being stored
 * into the specified variable.
 ***************************************************************************/
static void OnScreenKeyboard(char * var, u16 maxlen)
{
	int save = -1;

	GuiKeyboard keyboard(var, maxlen);

	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText okBtnTxt("OK", 22, (GXColor){0, 0, 0, 255});
	GuiImage okBtnImg(&btnOutline);
	GuiImage okBtnImgOver(&btnOutlineOver);
	GuiButton okBtn(btnOutline.GetWidth(), btnOutline.GetHeight());

	okBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	okBtn.SetPosition(25, -25);

	okBtn.SetLabel(&okBtnTxt);
	okBtn.SetImage(&okBtnImg);
	okBtn.SetImageOver(&okBtnImgOver);
	okBtn.SetTrigger(&trigA);
	okBtn.SetEffectGrow();

	GuiText cancelBtnTxt("Cancel", 22, (GXColor){0, 0, 0, 255});
	GuiImage cancelBtnImg(&btnOutline);
	GuiImage cancelBtnImgOver(&btnOutlineOver);
	GuiButton cancelBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	cancelBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	cancelBtn.SetPosition(-25, -25);
	cancelBtn.SetLabel(&cancelBtnTxt);
	cancelBtn.SetImage(&cancelBtnImg);
	cancelBtn.SetImageOver(&cancelBtnImgOver);
	cancelBtn.SetTrigger(&trigA);
	cancelBtn.SetEffectGrow();

	keyboard.Append(&okBtn);
	keyboard.Append(&cancelBtn);

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&keyboard);
	mainWindow->ChangeFocus(&keyboard);
	ResumeGui();

	while(save == -1)
	{
		usleep(THREAD_SLEEP);

		if(okBtn.GetState() == STATE_CLICKED)
			save = 1;
		else if(cancelBtn.GetState() == STATE_CLICKED)
			save = 0;
	}

	if(save)
	{
		snprintf(var, maxlen, "%s", keyboard.kbtextstr);
	}

	HaltGui();
	mainWindow->Remove(&keyboard);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
}


/* The main menu 
 */
static int MainMenu()
{
	int menu = MENU_NONE;

	// Main menu title
	GuiText titleTxt("wii-netconf", 28, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	// Init button objects
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);

	// Init trigger handlers
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	GuiTrigger trigHome;
	trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0);

	// "Network settings" button properties
	GuiText netBtnTxt("Network Settings", 22, (GXColor){0, 0, 0, 255});
	netBtnTxt.SetWrap(true, btnLargeOutline.GetWidth()-30);
	GuiImage netBtnImg(&btnLargeOutline);
	GuiImage netBtnImgOver(&btnLargeOutlineOver);
	GuiButton netBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	netBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	netBtn.SetPosition(0, 120);
	netBtn.SetLabel(&netBtnTxt);
	netBtn.SetImage(&netBtnImg);
	netBtn.SetImageOver(&netBtnImgOver);
	netBtn.SetTrigger(&trigA);
	netBtn.SetEffectGrow();

	// "Slippi Console Nickname" button properties
	GuiText nickBtnTxt("Slippi Settings", 22, (GXColor){0, 0, 0, 255});
	nickBtnTxt.SetWrap(true, btnLargeOutline.GetWidth()-30);
	GuiImage nickBtnImg(&btnLargeOutline);
	GuiImage nickBtnImgOver(&btnLargeOutlineOver);
	GuiButton nickBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	nickBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	nickBtn.SetPosition(0, 250);
	nickBtn.SetLabel(&nickBtnTxt);
	nickBtn.SetImage(&nickBtnImg);
	nickBtn.SetImageOver(&nickBtnImgOver);
	nickBtn.SetTrigger(&trigA);
	nickBtn.SetEffectGrow();

	// "Exit" button properties
	GuiText exitBtnTxt("Exit", 22, (GXColor){0, 0, 0, 255});
	GuiImage exitBtnImg(&btnOutline);
	GuiImage exitBtnImgOver(&btnOutlineOver);
	GuiButton exitBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	exitBtn.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
	exitBtn.SetPosition(0, -35);
	exitBtn.SetLabel(&exitBtnTxt);
	exitBtn.SetImage(&exitBtnImg);
	exitBtn.SetImageOver(&exitBtnImgOver);
	exitBtn.SetTrigger(&trigA);
	exitBtn.SetTrigger(&trigHome);
	exitBtn.SetEffectGrow();

	// Suspend GUI thread, add buttons to window, then resume GUI thread
	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&titleTxt);
	w.Append(&netBtn);
	w.Append(&nickBtn);
	w.Append(&exitBtn);
	mainWindow->Append(&w);
	ResumeGui();


	if (sd_card_initialized == false) {
		WindowPrompt(sd_title, sd_msg, sd_button1, NULL);
		menu = MENU_EXIT;
	}
	else {
		if (user_accepted_disclaimer == 0)
			user_accepted_disclaimer = WindowPrompt(disclaimer_title, disclaimer_msg,
				disclaimer_button1, disclaimer_button2);
		if (user_accepted_disclaimer == 0)
			menu = MENU_EXIT;
	}
			
	/* Loop for handling button presses: when we handle a button, fall out
	 * of this loop and return the proper menu to-be-rendered next. 
	 */

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		// Disable network config menu for now
		//if(netBtn.GetState() == STATE_CLICKED)
		//{
		//	menu = MENU_NETWORK;
		//}

		if(nickBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_SLIPPI;
		}
		if(exitBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_EXIT;
		}
	}
	HaltGui();
	mainWindow->Remove(&w);
	return menu;
}


/* ----------------------------------------------------------------------------
 * MenuSlippi() 
 * Slippi settings menu - options for RTC bias, console nickname, etc.
 *
 */

#define UNIX_BASE 946684800	// this is 1/1/2000 in UNIX time

u32 current_rtc = 0;		// updated every second for option display
time_t current_unixtime = 0;	// updated every second for option display
struct tm *current_time;	// updated every second for option display
char temp_nickname[32] = { 0 };	// temporary buffer for user input
u32  temp_bias = 0;		// temporary value for user input

// FTP temporary settings
u32  temp_ftp_enabled = 0;
char temp_ftp_server[64] = { 0 };
u16  temp_ftp_port = 21;
char temp_ftp_username[32] = { 0 };
char temp_ftp_password[32] = { 0 };
char temp_ftp_directory[64] = { 0 };

static int MenuSlippi()
{
	bool settings_changed = false;

	int menu = MENU_NONE;
	int ret;
	int i = 0;

	OptionList options;
	sprintf(options.name[i++], "Nickname");
	sprintf(options.name[i++], "Year");
	sprintf(options.name[i++], "Month");
	sprintf(options.name[i++], "Date");
	sprintf(options.name[i++], "Hour");
	sprintf(options.name[i++], "Minute");
	sprintf(options.name[i++], "Second");
	sprintf(options.name[i++], "FTP Enabled");
	sprintf(options.name[i++], "FTP Server");
	sprintf(options.name[i++], "FTP Port");
	sprintf(options.name[i++], "FTP Username");
	sprintf(options.name[i++], "FTP Password");
	sprintf(options.name[i++], "FTP Directory");
	options.length = i;

	// Initialize these with the value from settings
	temp_bias = settings.rtc_bias;
	strncpy(temp_nickname, settings.nickname, 32);
	
	// Initialize FTP temp variables
	temp_ftp_enabled = settings.ftp_enabled;
	strncpy(temp_ftp_server, settings.ftp_server, 64);
	temp_ftp_port = settings.ftp_port;
	strncpy(temp_ftp_username, settings.ftp_username, 32);
	strncpy(temp_ftp_password, settings.ftp_password, 32);
	strncpy(temp_ftp_directory, settings.ftp_directory, 64);

	GuiText titleTxt("Slippi Settings", 28, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);


	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();

	//GuiOptionBrowser optionBrowser(552, 248, &options);
	//optionBrowser.SetPosition(0, 108);
	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.SetPosition(0, 108);
	optionBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	optionBrowser.SetCol2Position(185);

	// Enable use of STATE_INCREMENT and STATE_DECREMENT
	optionBrowser.useIncDec = 1;

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	u32 ts_update_timer = 0;
	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);
		ts_update_timer += THREAD_SLEEP;

		// Recompute current_time for calendar/clock display (~once a second)
		if (ts_update_timer % 1000000)
		{
			__SYS_GetRTC(&current_rtc);
			current_unixtime = UNIX_BASE + current_rtc + temp_bias;
			current_time = gmtime(&current_unixtime);
			ts_update_timer = 0;
		}

		// Handle 'A' button presses for certain options
		ret = optionBrowser.GetClickedOption();
		switch (ret)
		{
			// Nickname
			case 0:
				OnScreenKeyboard(temp_nickname, 31);
				break;
			// FTP Enabled (toggle)
			case 7:
				temp_ftp_enabled = !temp_ftp_enabled;
				break;
			// FTP Server
			case 8:
				OnScreenKeyboard(temp_ftp_server, 63);
				break;
			// FTP Username  
			case 10:
				OnScreenKeyboard(temp_ftp_username, 31);
				break;
			// FTP Password
			case 11:
				OnScreenKeyboard(temp_ftp_password, 31);
				break;
			// FTP Directory
			case 12:
				OnScreenKeyboard(temp_ftp_directory, 63);
				break;
		}

		// Handle Left/Right pad presses for certain options
		ret = optionBrowser.GetIncDecOption(INCREMENT);
		switch (ret)
		{
			// Year
			case 1:
				current_time->tm_year += 1;
				break;
			// Month
			case 2:
				current_time->tm_mon += 1;
				break;
			// Date
			case 3:
				current_time->tm_mday += 1;
				break;
			// Hour
			case 4:
				current_time->tm_hour += 1;
				break;
			// Minute
			case 5:
				current_time->tm_min += 1;
				break;
			// Second
			case 6:
				current_time->tm_sec += 1;
				break;
			// FTP Port
			case 9:
				if (temp_ftp_port < 65535)
					temp_ftp_port += 1;
				break;
		}

		// Update the bias according to user input
		current_unixtime = mktime(current_time);
		if ((current_unixtime - UNIX_BASE - current_rtc) != temp_bias)
			temp_bias = current_unixtime - UNIX_BASE - current_rtc;

		ret = optionBrowser.GetIncDecOption(DECREMENT);
		switch (ret)
		{
			// Year
			case 1:
				current_time->tm_year -= 1;
				break;
			// Month
			case 2:
				current_time->tm_mon -= 1;
				break;
			// Date
			case 3:
				current_time->tm_mday -= 1;
				break;
			// Hour
			case 4:
				current_time->tm_hour -= 1;
				break;
			// Minute
			case 5:
				current_time->tm_min -= 1;
				break;
			// Second
			case 6:
				current_time->tm_sec -= 1;
				break;
			// FTP Port
			case 9:
				if (temp_ftp_port > 1)
					temp_ftp_port -= 1;
				break;
		}

		// Update the bias according to user input
		current_unixtime = mktime(current_time);
		if ((current_unixtime - UNIX_BASE - current_rtc) != temp_bias)
			temp_bias = current_unixtime - UNIX_BASE - current_rtc;

		// Recompute current_time so we display the right values
		current_time = gmtime(&current_unixtime);

		// Render all of the calendar/clock options
		snprintf(options.value[0], 32, "%s",	temp_nickname);
		snprintf(options.value[1], 32, "%04d",	(current_time->tm_year + 1900));
		snprintf(options.value[2], 32, "%s",	month_string[current_time->tm_mon]);
		snprintf(options.value[3], 32, "%02d",	current_time->tm_mday);
		snprintf(options.value[4], 32, "%02d",	current_time->tm_hour);
		snprintf(options.value[5], 32, "%02d",	current_time->tm_min);
		snprintf(options.value[6], 32, "%02d",	current_time->tm_sec);
		
		// Render FTP options
		snprintf(options.value[7], 32, "%s",	temp_ftp_enabled ? "Yes" : "No");
		snprintf(options.value[8], 32, "%s",	temp_ftp_server);
		snprintf(options.value[9], 32, "%d",	temp_ftp_port);
		snprintf(options.value[10], 32, "%s",	temp_ftp_username);
		snprintf(options.value[11], 32, "%s",	strlen(temp_ftp_password) > 0 ? "***" : "");
		snprintf(options.value[12], 32, "%s",	temp_ftp_directory);

		optionBrowser.TriggerUpdate();

		if(backBtn.GetState() == STATE_CLICKED)
		{
			// If any options have changed, move them back to `struct slippi_settings`
			if (strncmp(temp_nickname, settings.nickname, 32) != 0) 
			{
				strncpy(settings.nickname, temp_nickname, 32);
				settings_changed = true;

			}
			if (temp_bias != settings.rtc_bias)
			{
				settings.rtc_bias = temp_bias;
				settings_changed = true;
			}
			
			// Check FTP settings changes
			if (temp_ftp_enabled != settings.ftp_enabled)
			{
				settings.ftp_enabled = temp_ftp_enabled;
				settings_changed = true;
			}
			if (strncmp(temp_ftp_server, settings.ftp_server, 64) != 0)
			{
				strncpy(settings.ftp_server, temp_ftp_server, 64);
				settings_changed = true;
			}
			if (temp_ftp_port != settings.ftp_port)
			{
				settings.ftp_port = temp_ftp_port;
				settings_changed = true;
			}
			if (strncmp(temp_ftp_username, settings.ftp_username, 32) != 0)
			{
				strncpy(settings.ftp_username, temp_ftp_username, 32);
				settings_changed = true;
			}
			if (strncmp(temp_ftp_password, settings.ftp_password, 32) != 0)
			{
				strncpy(settings.ftp_password, temp_ftp_password, 32);
				settings_changed = true;
			}
			if (strncmp(temp_ftp_directory, settings.ftp_directory, 64) != 0)
			{
				strncpy(settings.ftp_directory, temp_ftp_directory, 64);
				settings_changed = true;
			}

			// Flush settings back to disk before exiting
			if (settings_changed == true)
			{
				FILE *slippi_fp = fopen(SD_SLIPPI_DAT_FILE, "wb");
				if (slippi_fp) 
				{
					fwrite(&settings, 1, sizeof(struct slippi_settings), slippi_fp);
					fclose(slippi_fp);
					usleep(200);
				}
				// TODO: Do something here if we can't get a FILE*
			}
			menu = MENU_MAIN;
		}
	}

	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	return menu;
}



static 

/* ----------------------------------------------------------------------------
 * MenuNetwork() 
 * Network settings menu.
 *
 * NOTE: For now, to minimize the chances of disaster, let's limit ourselves
 * to supporting Connection Profile #1. Additionally, let's not support any
 * proxy configuration until we have this working correctly.
 *
 */

#define OPT_ENABLE		0
const char OPT_ENABLE_STRING[] = "Enabled?";
#define OPT_WIRED		1
const char OPT_WIRED_STRING[] = "Use Wired?";
#define OPT_DHCP		2
const char OPT_DHCP_STRING[] =	"DHCP (IP)";
#define OPT_DHCPDNS		3
const char OPT_DHCPDNS_STRING[] = "DHCP (DNS)";
#define OPT_SSID		4
const char OPT_SSID_STRING[] =	"SSID";
#define OPT_ENC			5
const char OPT_ENC_STRING[] =	"Encryption";
#define OPT_KEY			6
const char OPT_KEY_STRING[] =	"Key";
#define OPT_ADDR		7
const char OPT_ADDR_STRING[] =	"IP Addr";
#define OPT_MASK		8
const char OPT_MASK_STRING[] =	"Netmask";
#define OPT_GW			9
const char OPT_GW_STRING[] =	"Gateway";
#define OPT_DNS1		10
const char OPT_DNS1_STRING[] =	"Primary DNS";
#define OPT_DNS2		11
const char OPT_DNS2_STRING[] =	"Secondary DNS";
#define OPT_LEN			12

const char tf_string[2][16] = { "False", "True", };
const char of_string[2][16] = { "Off", "On", };
const char ed_string[2][16] = { "Disabled", "Enabled", };

char ip_string[32]	= { 0 };
char mask_string[32]	= { 0 };
char gw_string[32]	= { 0 };
char dns1_string[32]	= { 0 };
char dns2_string[32]	= { 0 };

const char safe_string[32] = "************************";

const char enc_string[7][32] = {
	"Open",
	"WEP64",
	"WEP128",
	"WARNING: Unknown 0x3?",
	"WPA-PSK (TKIP)",
	"WPA2-PSK (AES)",
	"WPA-PSK (AES)",
};


#define CONFIG_LEN	0x1b5c
#define NAND_PATH	"/shared2/sys/net/02/config.dat"
#define SD_PATH		"sd:/config.dat"

//static u8 netcfg[0x1b60] ATTRIBUTE_ALIGN(32) = { 0 };
static struct network_config netcfg ATTRIBUTE_ALIGN(32);

static int MenuNetwork()
{
	bool settings_changed = false;

	int menu = MENU_NONE;
	int ret;
	int i = 0;

	/* Read network configuration from /shared2/sys/net/02/config.dat and
	 * write it to netcfg. This buffer will be flushed to NAND if 
	 * the user has made any changes during this session.
	 * If we fail to read the file for some reason, just exit this menu.
	 */

	s32 res, netconf_fd;
	bool do_exit = false;

	// Open and read the network config file into memory
	netconf_fd = IOS_Open(NAND_PATH, IPC_OPEN_READ);
	if (netconf_fd < 0)
	{
		printf("IOS_Open returned %d\n", netconf_fd);
		do_exit = true;
	}
	res = IOS_Read(netconf_fd, &netcfg, CONFIG_LEN);
	if (res != CONFIG_LEN)
	{
		printf("IOS_Read returned %08x (expected %08x)\n", res, CONFIG_LEN);
		do_exit = true;
	}
	IOS_Close(netconf_fd);

	/* This program relies on some assumptions about config files being
	 * "well-formed" (according to our limited knowledge). Because this is
	 * the case, we *must* refuse to handle some of these cases.
	 */

	if (do_exit)
	{
		// Tell the user something went wrong, sleep, then exit
		HaltGui();
		menu = MENU_MAIN;
		return menu;
	}

	/* Set up the initial list of options according to the current
	 * network configuration. This might be changed later in the loop
	 * if the user changes some options. These control the way the menu 
	 * appears to users at any given moment.
	 */

	bool connectionEnabled	= false;
	bool useWired		= false;
	bool useProxy		= false;
	bool useDHCP		= false;
	bool useDHCPDNS		= false;

	if (netcfg.profile[0].flags & IS_ACTIVE)
	{
		connectionEnabled = true;

		netcfg.profile[0].flags |= IS_ACTIVE;
		netcfg.profile[0].flags |= HAS_INTERNET;

		// For now, let's assume we *don't* need to do this
		//netcfg.profile[1].flags &= ~(IS_ACTIVE);
		//netcfg.profile[2].flags &= ~(IS_ACTIVE);
	}

	/* Each profile has a bit which determines if the connection is a
	 * wired connection; however, byte 0x4 in the network configuration's
	 * header also seems to determine (perhaps more globally) whether or
	 * not some interface is used. 
	 *
	 * Byte 0x4 in the header must be set to 0x2 for wired connections. 
	 * In case we come across a configuration where the flags dont match 
	 * this header byte, correct it for the user.
	 *
	 * It's not clear if this has any implications if say, a user were to
	 * switch to a wireless configuration configured on a different profile
	 * using the System Menu options [or something else like that].
	 */

	if ((netcfg.profile[0].flags & USE_WIRED) == 1)
	{
		if (netcfg.header[4] != 0x02)
			netcfg.header[4] = 0x02;

		useWired = true;
	}
	else
	{
		if (netcfg.header[4] != 0x01)
			netcfg.header[4] = 0x01;
	}

	// For now, explicitly disable proxy settings
	if ((netcfg.profile[0].flags & USE_PROXY) == 1)
	{
		//useProxy = true;
		netcfg.profile[0].flags &= ~(USE_PROXY);
	}

	if ((netcfg.profile[0].flags & USE_DHCP_ADDR) == 1)
	{
		useDHCP = true;
	}

	if ((netcfg.profile[0].flags & USE_DHCP_DNS) == 1) 
		useDHCPDNS = true;


	// Set names for all of the options
	OptionList options;
	sprintf(options.name[OPT_ENABLE], OPT_ENABLE_STRING);
	sprintf(options.name[OPT_WIRED], OPT_WIRED_STRING);
	sprintf(options.name[OPT_SSID],	OPT_SSID_STRING	);
	sprintf(options.name[OPT_ENC], OPT_ENC_STRING);
	sprintf(options.name[OPT_KEY], OPT_KEY_STRING);
	sprintf(options.name[OPT_DHCP],	OPT_DHCP_STRING);
	sprintf(options.name[OPT_DHCPDNS], OPT_DHCPDNS_STRING);
	sprintf(options.name[OPT_ADDR],	OPT_ADDR_STRING);
	sprintf(options.name[OPT_MASK],	OPT_MASK_STRING);
	sprintf(options.name[OPT_GW], OPT_GW_STRING);
	sprintf(options.name[OPT_DNS1],	OPT_DNS1_STRING);
	sprintf(options.name[OPT_DNS2],	OPT_DNS2_STRING);
	options.length = OPT_LEN;

	/* Initally draw all of the options when the menu is instantiated.
	 */

	snprintf(options.value[OPT_ENABLE], 32, "%s", tf_string[connectionEnabled]);
	snprintf(options.value[OPT_WIRED], 32, "%s", tf_string[useWired]);
	snprintf(options.value[OPT_DHCP],  32, "%s", tf_string[useDHCP]);

	if (useDHCP == true)
	{
		snprintf(options.value[OPT_ADDR], 32, "%s", "DHCP");
		snprintf(options.value[OPT_MASK], 32, "%s", "DHCP");
		snprintf(options.value[OPT_GW], 32, "%s", "DHCP");
		snprintf(options.value[OPT_DHCPDNS],  32, "%s", tf_string[useDHCPDNS]);

	}
	else
	{
		snprintf(options.value[OPT_DHCPDNS],  32, "%s", "<disabled>");
		snprintf(options.value[OPT_ADDR],  32, "%u.%u.%u.%u", 
			netcfg.profile[0].manual.ip[0], 
			netcfg.profile[0].manual.ip[1], 
			netcfg.profile[0].manual.ip[2], 
			netcfg.profile[0].manual.ip[3]);
		snprintf(options.value[OPT_MASK],  32, "%u.%u.%u.%u",
			netcfg.profile[0].manual.mask[0], 
			netcfg.profile[0].manual.mask[1],
			netcfg.profile[0].manual.mask[2], 
			netcfg.profile[0].manual.mask[3]);
		snprintf(options.value[OPT_GW],    32, "%u.%u.%u.%u",
			netcfg.profile[0].manual.gw[0], 
			netcfg.profile[0].manual.gw[1],
			netcfg.profile[0].manual.gw[2], 
			netcfg.profile[0].manual.gw[3]);
	}

	if (useDHCPDNS == true)
	{
		snprintf(options.value[OPT_DNS1], 32, "%s", "<disabled>");
		snprintf(options.value[OPT_DNS2], 32, "%s", "<disabled>");
	}
	else
	{
		snprintf(options.value[OPT_DNS1],  32, "%u.%u.%u.%u",
			netcfg.profile[0].manual.dns1[0], 
			netcfg.profile[0].manual.dns1[1],
			netcfg.profile[0].manual.dns1[2], 
			netcfg.profile[0].manual.dns1[3]);
		snprintf(options.value[OPT_DNS2],  32, "%u.%u.%u.%u",
			netcfg.profile[0].manual.dns2[0], 
			netcfg.profile[0].manual.dns2[1],
			netcfg.profile[0].manual.dns2[2], 
			netcfg.profile[0].manual.dns2[3]);
	}

	if (useWired)
	{
		snprintf(options.value[OPT_SSID], 32, "%s", 
				"(Disabled for Wired Connection)");
		snprintf(options.value[OPT_ENC], 32, "%s", 
				"(Disabled for Wired Connection)");
		snprintf(options.value[OPT_KEY], 32, "%s", 
				"(Disabled for Wired Connection)");
	}
	else
	{
		snprintf(options.value[OPT_SSID], 32, "%.32s", 
				netcfg.profile[0].wifi.ssid);
		snprintf(options.value[OPT_ENC], 32, "%s", 
				enc_string[netcfg.profile[0].wifi.enc]);
		snprintf(options.value[OPT_KEY], 32, "%s", safe_string);
	}


	// Title
	GuiText titleTxt("Network Settings", 28, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	// "Go back" button
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();

	// Option-browser object
	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.SetPosition(0, 108);
	optionBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	optionBrowser.SetCol2Position(185);
	optionBrowser.useIncDec = 1;

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		// Deal with 'A' presses for "global" options
		ret = optionBrowser.GetClickedOption();
		switch (ret)
		{
			case OPT_ENABLE:
				connectionEnabled = !connectionEnabled;
				if (connectionEnabled == true)
				{
					netcfg.profile[0].flags |= IS_ACTIVE;
					netcfg.profile[0].flags |= HAS_INTERNET;
				}
				else
				{
					netcfg.profile[0].flags &= ~(IS_ACTIVE);
					netcfg.profile[0].flags &= ~(HAS_INTERNET);
				}
				break;
			case OPT_WIRED:
				useWired = !useWired;
				break;
			case OPT_DHCP:
				useDHCP = !useDHCP;
				break;
			case OPT_DHCPDNS:
				if (useDHCP == true)
					useDHCPDNS = !useDHCPDNS;
				break;
		}

		// Deal with 'A' presses for manual config options
		if (useDHCP == false)
		{
			ret = optionBrowser.GetClickedOption();
			switch (ret)
			{
				case OPT_ADDR:
					break;
				case OPT_MASK:
					break;
				case OPT_GW:
					break;
			}
		}

		// Deal with 'A' presses for manual DNS options
		if (useDHCPDNS == false)
		{
			ret = optionBrowser.GetClickedOption();
			switch (ret)
			{
				case OPT_DNS1:
					break;
				case OPT_DNS2:
					break;
			}
		}

		// Deal with 'A' presses for Wi-Fi options
		if (useWired == false)
		{
			ret = optionBrowser.GetClickedOption();
			switch (ret)
			{
				// Note that SSID strings are right-padded 
				// with 0x00 bytes. Remember to verify this. 
				case OPT_SSID:
					break;

				// Value for 0x03 is unknown; just skip it.
				// Wrap around the list after 0x06. 
				case OPT_ENC:
					netcfg.profile[0].wifi.enc++;
					if (netcfg.profile[0].wifi.enc == 0x03)
						netcfg.profile[0].wifi.enc = 0x04;
					// Wrap around the list
					if (netcfg.profile[0].wifi.enc > 0x06)
						netcfg.profile[0].wifi.enc = 0x00;
					break;

				case OPT_KEY:
					break;
			}
		}


		/* Conditionally re-draw values for certain options depending
		 * on the current configuration (ie. for wired connections,
		 * indicate that Wi-Fi options are disabled; for some config
		 * with DHCP, indicate that manual settings are disabled).
		 */

		snprintf(options.value[OPT_ENABLE], 32, "%s", tf_string[connectionEnabled]);
		snprintf(options.value[OPT_WIRED], 32, "%s", tf_string[useWired]);
		snprintf(options.value[OPT_DHCP],  32, "%s", tf_string[useDHCP]);

		// Render static addressing options
		if (useDHCP == true)
		{
			snprintf(options.value[OPT_ADDR], 32, "%s", "<disabled>");
			snprintf(options.value[OPT_MASK], 32, "%s", "<disabled>");
			snprintf(options.value[OPT_GW], 32, "%s", "<disabled>");
			snprintf(options.value[OPT_DHCPDNS],  32, "%s", tf_string[useDHCPDNS]);
		}
		else
		{
			snprintf(options.value[OPT_DHCPDNS],  32, "%s", "<disabled>");
			snprintf(options.value[OPT_ADDR], 32, "%u.%u.%u.%u", 
				netcfg.profile[0].manual.ip[0], 
				netcfg.profile[0].manual.ip[1], 
				netcfg.profile[0].manual.ip[2], 
				netcfg.profile[0].manual.ip[3]);
			snprintf(options.value[OPT_MASK], 32, "%u.%u.%u.%u",
				netcfg.profile[0].manual.mask[0], 
				netcfg.profile[0].manual.mask[1],
				netcfg.profile[0].manual.mask[2], 
				netcfg.profile[0].manual.mask[3]);
			snprintf(options.value[OPT_GW], 32, "%u.%u.%u.%u",
				netcfg.profile[0].manual.gw[0], 
				netcfg.profile[0].manual.gw[1],
				netcfg.profile[0].manual.gw[2], 
				netcfg.profile[0].manual.gw[3]);
		}

		if (useDHCPDNS == true)
		{
			snprintf(options.value[OPT_DNS1], 32, "%s", "<disabled>");
			snprintf(options.value[OPT_DNS2], 32, "%s", "<disabled>");
		}
		else
		{
			snprintf(options.value[OPT_DNS1], 32, "%u.%u.%u.%u",
				netcfg.profile[0].manual.dns1[0], 
				netcfg.profile[0].manual.dns1[1],
				netcfg.profile[0].manual.dns1[2], 
				netcfg.profile[0].manual.dns1[3]);
			snprintf(options.value[OPT_DNS2], 32, "%u.%u.%u.%u",
				netcfg.profile[0].manual.dns2[0], 
				netcfg.profile[0].manual.dns2[1],
				netcfg.profile[0].manual.dns2[2], 
				netcfg.profile[0].manual.dns2[3]);
		}

		// Render Wi-Fi options
		if (useWired)
		{
			snprintf(options.value[OPT_SSID], 32, "%s", "<disabled>");
			snprintf(options.value[OPT_ENC], 32, "%s", "<disabled>");
			snprintf(options.value[OPT_KEY], 32, "%s", "<disabled>");
		}
		else
		{
			snprintf(options.value[OPT_SSID], 32, "%.32s", 
					netcfg.profile[0].wifi.ssid);
			snprintf(options.value[OPT_ENC], 32, "%s", 
					enc_string[netcfg.profile[0].wifi.enc]);
			snprintf(options.value[OPT_KEY], 32, "%s", safe_string);
		}


		optionBrowser.TriggerUpdate();

		if(backBtn.GetState() == STATE_CLICKED)
		{
			// Flush settings back to disk before exiting
			menu = MENU_MAIN;
		}
	}

	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	return menu;
}




/****************************************************************************
 * MenuSettingsFile
 ***************************************************************************/

static int MenuSettingsFile()
{
	int menu = MENU_NONE;
	int ret;
	int i = 0;
	bool firstRun = true;
	OptionList options;
	sprintf(options.name[i++], "Load Device");
	sprintf(options.name[i++], "Save Device");
	sprintf(options.name[i++], "Folder 1");
	sprintf(options.name[i++], "Folder 2");
	sprintf(options.name[i++], "Folder 3");
	sprintf(options.name[i++], "Auto Load");
	sprintf(options.name[i++], "Auto Save");
	options.length = i;

	GuiText titleTxt("Settings - Saving & Loading", 28, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,50);

	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText backBtnTxt("Go Back", 22, (GXColor){0, 0, 0, 255});
	GuiImage backBtnImg(&btnOutline);
	GuiImage backBtnImgOver(&btnOutlineOver);
	GuiButton backBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	backBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	backBtn.SetPosition(100, -35);
	backBtn.SetLabel(&backBtnTxt);
	backBtn.SetImage(&backBtnImg);
	backBtn.SetImageOver(&backBtnImgOver);
	backBtn.SetTrigger(&trigA);
	backBtn.SetEffectGrow();

	GuiOptionBrowser optionBrowser(552, 248, &options);
	optionBrowser.SetPosition(0, 108);
	optionBrowser.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	optionBrowser.SetCol2Position(185);

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&backBtn);
	mainWindow->Append(&optionBrowser);
	mainWindow->Append(&w);
	mainWindow->Append(&titleTxt);
	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		ret = optionBrowser.GetClickedOption();

		switch (ret)
		{
			case 0:
				Settings.LoadMethod++;
				break;

			case 1:
				Settings.SaveMethod++;
				break;

			case 2:
				OnScreenKeyboard(Settings.Folder1, 256);
				break;

			case 3:
				OnScreenKeyboard(Settings.Folder2, 256);
				break;

			case 4:
				OnScreenKeyboard(Settings.Folder3, 256);
				break;

			case 5:
				Settings.AutoLoad++;
				if (Settings.AutoLoad > 2)
					Settings.AutoLoad = 0;
				break;

			case 6:
				Settings.AutoSave++;
				if (Settings.AutoSave > 3)
					Settings.AutoSave = 0;
				break;
		}

		if(ret >= 0 || firstRun)
		{
			firstRun = false;

			// correct load/save methods out of bounds
			if(Settings.LoadMethod > 4)
				Settings.LoadMethod = 0;
			if(Settings.SaveMethod > 6)
				Settings.SaveMethod = 0;

			if (Settings.LoadMethod == METHOD_AUTO) sprintf (options.value[0],"Auto Detect");
			else if (Settings.LoadMethod == METHOD_SD) sprintf (options.value[0],"SD");
			else if (Settings.LoadMethod == METHOD_USB) sprintf (options.value[0],"USB");
			else if (Settings.LoadMethod == METHOD_DVD) sprintf (options.value[0],"DVD");
			else if (Settings.LoadMethod == METHOD_SMB) sprintf (options.value[0],"Network");

			if (Settings.SaveMethod == METHOD_AUTO) sprintf (options.value[1],"Auto Detect");
			else if (Settings.SaveMethod == METHOD_SD) sprintf (options.value[1],"SD");
			else if (Settings.SaveMethod == METHOD_USB) sprintf (options.value[1],"USB");
			else if (Settings.SaveMethod == METHOD_SMB) sprintf (options.value[1],"Network");
			else if (Settings.SaveMethod == METHOD_MC_SLOTA) sprintf (options.value[1],"MC Slot A");
			else if (Settings.SaveMethod == METHOD_MC_SLOTB) sprintf (options.value[1],"MC Slot B");

			snprintf (options.value[2], 256, "%s", Settings.Folder1);
			snprintf (options.value[3], 256, "%s", Settings.Folder2);
			snprintf (options.value[4], 256, "%s", Settings.Folder3);

			if (Settings.AutoLoad == 0) sprintf (options.value[5],"Off");
			else if (Settings.AutoLoad == 1) sprintf (options.value[5],"Some");
			else if (Settings.AutoLoad == 2) sprintf (options.value[5],"All");

			if (Settings.AutoSave == 0) sprintf (options.value[5],"Off");
			else if (Settings.AutoSave == 1) sprintf (options.value[6],"Some");
			else if (Settings.AutoSave == 2) sprintf (options.value[6],"All");

			optionBrowser.TriggerUpdate();
		}

		if(backBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_MAIN;
		}
	}
	HaltGui();
	mainWindow->Remove(&optionBrowser);
	mainWindow->Remove(&w);
	mainWindow->Remove(&titleTxt);
	return menu;
}


/* Main menu initialization and loop.
 */
void MenuInit(void)
{
	int currentMenu = MENU_MAIN;

	// Probably for handling Wiimotes?
	#ifdef HW_RVL
	pointer[0] = new GuiImageData(player1_point_png);
	pointer[1] = new GuiImageData(player2_point_png);
	pointer[2] = new GuiImageData(player3_point_png);
	pointer[3] = new GuiImageData(player4_point_png);
	#endif

	// Initialize main window with a background
	mainWindow = new GuiWindow(screenwidth, screenheight);
	bgImg = new GuiImage(screenwidth, screenheight, (GXColor){50, 50, 50, 255});
	bgImg->ColorStripe(30);
	mainWindow->Append(bgImg);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	ResumeGui();

	/* --------------------------------------------------------------------
	 * Main loop. When a sub-menu is done executing, it should return the
	 * ID of the next menu to-be-dispatched.
	 */

	while(currentMenu != MENU_EXIT)
	{
		switch (currentMenu)
		{
			case MENU_MAIN:
				currentMenu = MainMenu();
				break;
			case MENU_SETTINGS_FILE:
				currentMenu = MenuSettingsFile();
				break;
			case MENU_NETWORK:
				currentMenu = MenuNetwork();
				break;
			case MENU_SLIPPI:
				currentMenu = MenuSlippi();
				break;
			default: // unrecognized menu
				currentMenu = MainMenu();
				break;
		}
	}


	/* --------------------------------------------------------------------
	 * Cleanup before we exit
	 */
	
	ResumeGui();
	ExitRequested = 1;
	while(1) usleep(THREAD_SLEEP);

	HaltGui();

	delete bgImg;
	delete mainWindow;

	delete pointer[0];
	delete pointer[1];
	delete pointer[2];
	delete pointer[3];

	mainWindow = NULL;
}
