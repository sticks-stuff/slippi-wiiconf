/* Definitions of these structures are pulled directly from:
 *
 *	https://wiibrew.org/wiki/shared2/sys/net/02/config.dat
 */

#ifndef _NETCONF_H
#define _NETCONF_H

struct proxy
{
	u8 use_proxy;
	u8 use_proxy_auth;
	u8 pad_0[2];
	u8 name[255];
	u8 pad_1;
	u16 port;
	u8 username[32];
	u8 pad_2;
	u8 password[32];
} __attribute__((packed));

struct wifi
{
	u8 ssid[32];		// Wi-Fi SSID
	u8 pad_5;
	u8 ssid_len;		// SSID length
	u8 pad_6[2];
	u8 pad_7;
	u8 enc;			// Wi-Fi encryption type
	u8 pad_8[2];
	u8 pad_9;
	u8 key_len;		// Wi-Fi key length
	u8 unk_0;
	u8 pad_11;
	u8 key[64];		// Wi-Fi key
} __attribute__((packed));

struct manual
{
	u8 ip[4];		// IP address
	u8 mask[4];		// Network mask
	u8 gw[4];		// Default gateway	
	u8 dns1[4];		// DNS Server 1
	u8 dns2[4];		// DNS Server 2

	u8 pad_1[2];
	u16 mtu;		// MTU
	u8 pad_2[8];
}__attribute__((packed));

// Bits for the flags field in `struct conn`
#define USE_WIRED	(1 << 0)
#define USE_DHCP_DNS	(1 << 1)
#define USE_DHCP_ADDR	(1 << 2)
#define UNK_FLAG_3	(1 << 3)
#define USE_PROXY	(1 << 4)
#define HAS_INTERNET	(1 << 5)
#define UNK_FLAG_6	(1 << 6)
#define IS_ACTIVE	(1 << 7)

struct conn
{
	u8 flags;		// bitfield
	u8 pad_0[3];
	struct manual manual;	// 0x20 bytes
	struct proxy proxy;	// 0x147 bytes
	u8 pad_3;
	struct proxy proxy_dup;	// 0x147 bytes
	u8 pad_4[1297];
	struct wifi wifi;	// 0x6c bytes
	u8 pad_5[236];
}__attribute__((packed));

/* This is the structure of config.dat - there are 3 profiles.
 */
struct network_config
{
	/* It's not entirely clear what these header bytes are for:
	 *	- Header byte 0x4 seems to switch between Wi-Fi and Ethernet?
	 *	  (observed wifi=0x1, ethernet=0x2)
	 *	- Header byte 0x6 is always set to 0x7?
	 *	- Header bytes 0x{0,1,2,3,5,7} always seem to be set to 0x00?
	 */

	u8 header[8];
	struct conn profile[3];	// (0x91c * 3) bytes
} __attribute__((packed));

#endif // _NETCONF_H
