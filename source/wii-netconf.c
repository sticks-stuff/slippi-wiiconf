#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include <unistd.h>
#include <gccore.h>
#include <ogcsys.h>
#include <network.h>
#include <fat.h>

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

#define NAND_CUR_PATH	"/shared2/sys/net/02/config.dat"
#define USB_CUR_PATH	"fat:/current_config.dat"
#define USB_NEW_PATH	"fat:/new_config.dat"

/* Typical libogc video/console initialization */
void xfb_init(void)
{
	VIDEO_Init();
	PAD_Init();
	rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight,
			rmode->fbWidth*VI_DISPLAY_PIX_SZ);

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE)
		VIDEO_WaitVSync();
}

/* Check for USB storage and mount - no SD card right now */
int storage_config(void)
{
	__io_usbstorage.startup();
	if (!__io_usbstorage.isInserted())
	{
		printf("[!] USB storage startup failed\n");
		return -1;
	}

	if (!fatMountSimple("fat", &__io_usbstorage))
	{
		printf("[!] Couldn't mount USB storage\n");
		return -1;
	}
	return 0;
}

/* Carve out data region to store the current config.dat contents before we
 * write them to a FAT storage device */
static u8 config_data[0x1b60] ATTRIBUTE_ALIGN(32) = {0};

/* Read the current network configuration from /shared2/sys/net/02/config.dat
 * and write to current_config.dat on the root of USB storage media */
int get_config(void)
{
	s32 res;

	/* We need to read out config.dat in order to see what the network
	 * configuration is like (perhaps there are other steps we need to
	 * take in order to deal with this?
	 *
	 * I don't understand the distinction between IOS_Open() and something
	 * like ISFS_Open()? What's the difference?
	 */

	printf("[*] Calling IOS_Open() on %s ...\n", NAND_CUR_PATH);
	s32 netconf_fd = IOS_Open(NAND_CUR_PATH, IPC_OPEN_READ);
	if (netconf_fd < 0) {
		printf("[!] IOS_Open() returned %ld\n", netconf_fd);
		return -1;
	}

	printf("[*] Issuing IOS_Read() ...\n");
	res = IOS_Read(netconf_fd, config_data, 0x1b5c);
	IOS_Close(netconf_fd);

	if (res != 0x1b5c) {
		printf("[!] IOS_Read() returned 0x%x (should be 0x1b5c?)\n", res);
		return -1;
	}

	printf("[*] Writing config.dat to %s\n", USB_CUR_PATH);
	FILE *fp = fopen(USB_CUR_PATH, "wb");
	if (!fp) {
		printf("[!] Couldn't open %s\n", USB_CUR_PATH);
		return -1;
	}
	res = fwrite(config_data, 1, 0x1b5c, fp);
	fclose(fp);

	return 0;
}

/* Read some new_config.dat from the root of USB storage media, then write it
 * back to NAND flash. Mostly based on suloku's code and documentation, see:
 *	http://wiibrew.org/wiki/Offline_Network_Enabler
 */
#define NAND_PAGE_SIZE	2048
int set_config(void)
{
	s32 res, config_fd;
	u32 input_len, cnt, sz, off;

	printf("[*] Calling ISFS_Initialize() ..\n");
	res = ISFS_Initialize();
	if (res < 0)
	{
		printf("[!] ISFS_Initialize() returned %d\n", res);
		sleep(5);
		return -1;
	}

	// For the data to-be-written
	u8 *new_config = (u8*)memalign(32, 0x1b5c);

	// Buffer for writing - you can only do 2048-byte writes?
	u8 *buf = (u8*)memalign(32, NAND_PAGE_SIZE);

	// IOS fstat - I guess this needs to be 32-byte aligned too?
	fstats *stats = memalign(32, sizeof(fstats));

	printf("[*] Reading from %s\n", USB_NEW_PATH);
	FILE *fp = fopen(USB_NEW_PATH, "rb");
	if (!fp) {
		printf("[!] Couldn't find %s\n", USB_NEW_PATH);
		free(stats);
		free(new_config);
		free(buf);
		return -1;
	}
	res = fread(new_config, 1,  0x1b5c, fp);
	input_len = res;
	cnt = input_len;
	fclose(fp);
	if (res != 0x1b5c) {
		printf("[!] Expected 0x1b5c bytes, got 0x%x\n", res);
		free(stats);
		free(new_config);
		free(buf);
		return -1;
	}

	printf("[*] Deleting %s\n", NAND_CUR_PATH);
	res = ISFS_Delete(NAND_CUR_PATH);
	if (res != 0) {
		printf("[!] ISFS_Delete() returned %d\n", res);
		free(stats);
		free(new_config);
		free(buf);
		return -1;
	}

	printf("[*] Creating new %s\n", NAND_CUR_PATH);
	res = ISFS_CreateFile(NAND_CUR_PATH, 0, 3, 3, 3);
	printf("[!] ISFS_CreateFile() returned %d\n", res);

	printf("[!] Opening %s\n", NAND_CUR_PATH);
	config_fd = ISFS_Open(NAND_CUR_PATH, ISFS_OPEN_RW);
	if (!config_fd)
	{
		printf("[!] ISFS_Open() returned %d\n", config_fd);
		return -1;
	}

	printf("[!] Writing new data on %s\n", NAND_CUR_PATH);
	off = 0;
	while(cnt > 0)
	{
		if (cnt >= NAND_PAGE_SIZE)
			sz = NAND_PAGE_SIZE;
		else
			sz = cnt;

		memcpy(buf, new_config + off, sz);
		res = ISFS_Write(config_fd, buf, sz);
		if (!res) {
			printf("[!] ISFS_Write() returned %d\n", res);
			printf("[!] cnt=0x%x, sz=0x%x\n", cnt, sz);
			ISFS_Close(config_fd);
			free(stats);
			free(new_config);
			free(buf);
			sleep(5);
			return -1;
		}

		off += sz;
		cnt -= sz;
	}

	ISFS_Close(config_fd);
	s32 new_fd = ISFS_Open(NAND_CUR_PATH, ISFS_OPEN_RW);
	res = ISFS_GetFileStats(new_fd, stats);
	printf("[*] New file is 0x%x bytes\n", stats->file_length);
	ISFS_Close(new_fd);

	free(stats);
	free(new_config);
	free(buf);
	return 0;
}

static char localip[16] = {0};
static char gateway[16] = {0};
static char netmask[16] = {0};
int main(int argc, char **argv)
{
	u32 res;

	xfb_init();
	printf("\x1b[2;0H");

	/* Make sure a USB device is attached */

	res = storage_config();
	if (res != 0) {
		printf("[!] Couldn't initialize USB device\n");
		printf("[!] Rebooting in 5s ...");
		sleep(5);
		exit(0);
	}
	else {
		printf("[*] Found USB device\n");
	}

	/* Try initializing the network first */

	res = if_config(localip, netmask, gateway, TRUE, 20);

	/* Wait for user to select some option */

	while(1) {
		printf("\x1b[40m\x1b[2J\x1b[2;0H");
		printf("[*] wii-netconf [https://github.com/hosaka-corp/wii-netconf]\n");
		printf("[*] Current configuration:\n");
		printf("    IP Address:\t %s\n", localip);
		printf("\n");
		printf("[*] Press A to read config to usb:/current_config.dat\n");
		printf("[*] Press B to write config from usb:/new_config.dat\n");
		printf("[*] Press START to reboot\n");

		// Read controller inputs
		PAD_ScanPads();
		u32 pressed = PAD_ButtonsDown(0);

		if (pressed & PAD_BUTTON_A) {
			res = get_config();
			if (res != 0) {
				printf("[!] get_config() failed\n");
			} else {
				printf("[*] get_config() completed\n");
			}
			sleep(5);
		}
		else if (pressed & PAD_BUTTON_B) {
			res = set_config();
			if (res != 0) {
				printf("[!] set_config() failed\n");
			} else {
				printf("[*] set_config() completed\n");
			}
			sleep(5);
		}
		else if (pressed & PAD_BUTTON_START)
			exit(0);

		VIDEO_WaitVSync();
	}
	return 0;
}
