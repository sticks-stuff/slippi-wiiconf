#!/usr/bin/python
''' wii-netconf.py
Hackily parse a config.dat file containing Nintendo Wii network configuration.
This is *experimental and not-well-tested*. Don't blame me [yet] if you break
something when you write back to NAND :^(

USAGE:
    1. Use the netconf DOL to read out your current config.dat
    2. Use the --to-json flag to convert into a readable JSON format
    3. Make some changes on it in a text editor
    4. Use the --to-binary flag on JSON to convert back into binary config.dat
    5. Write the new config.dat onto FAT32-formatted SD card and use the
       wii-netconf DOL to overwrite the existing configuration on your console

TODO/NOTES:
    - Written on Python 3.6
    - Probably doesn't work on Windows? - I don't use Windows.
    - Doesn't handle static network configuration or proxy configuration.
      Currently zeroing out these fields when writing back to binary.
    - JSON expects integer representation of Wi-Fi encryption schemes
      according to the ones defined in this file.
    - Currently just storing header bytes as a string of hex digits because
      I don't know what they do or how they work. See README.md for important
      information about this.
'''

import sys
import os
import json
import binascii
import ipaddress

from struct import pack, unpack
from enum import Enum

class flags(Enum):
    ACTIVE          = 0x80  # Is the connection selected?
    UNK_64          = 0x40  # Unknown
    INTERNET        = 0x20  # Passed internet connectivity check?
    USE_PROXY       = 0x10  # Use associated proxy settings?
    UNK_8           = 0x8   # Unknown
    USE_DHCP_ADDR   = 0x4   # Get an addr via DHCP? (otherwise, manual)
    USE_DHCP_DNS    = 0x2   # Get DNS info via DHCP? (otherwise, manual)
    USE_WIRED       = 0x1   # Use the LAN adapter? (otherwise, use wireless)

# Wifi Encryption Flags
class enc(Enum):
    OPEN            = 0x00
    WEP64           = 0x01
    WEP128          = 0x02
    WPA_PSK_TKIP    = 0x04
    WPA2_PSK_AES    = 0x05
    WPA_PSK_AES     = 0x06

# afaik binary input should always be this size
FILE_LEN = 0x1b5c

# -----------------------------------------------------------------------------
# Helper functions

def usage():
    print("Usage: wii-netconf.py [--to-json | --to-binary] <input file> <output file>")
    print('"Convert between representations of Wii network configuration files."')
    print("  --to-json:     Convert input binary file into JSON output file")
    print("  --to-binary:   Convert input JSON file into binary output file")
    exit(-1)

def flags_to_json(byte):
    """ Break out an int into a dict with the following fields.
    Binary flags are stored in a single uint8_t """

    flags = {   "active": False,
                "check-passed": False,
                "use-proxy": False,
                "use-dhcp-addr": False,
                "use-dhcp-dns": False,
                "use-wired": False,
            }

    if (byte & 0x80): flags['active'] = True
    if (byte & 0x20): flags['check-passed'] = True
    if (byte & 0x10): flags['use-proxy'] = True
    if (byte & 0x04): flags['use-dhcp-addr'] = True
    if (byte & 0x02): flags['use-dhcp-dns'] = True
    if (byte & 0x01): flags['use-wired'] = True
    return flags

def json_to_flags(flags):
    """ Convert some dict with the following fields into an int.
    Binary flags are stored in a single uint8_t """

    byte = 0x00
    if (flags['active']): byte |= 0x80
    if (flags['check-passed']): byte |= 0x20
    if (flags['use-proxy']): byte |= 0x10
    if (flags['use-dhcp-addr']): byte |= 0x04
    if (flags['use-dhcp-dns']): byte |= 0x02
    if (flags['use-wired']): byte |= 0x01
    return byte

def static_to_json(data):
    static = {}

    if (len(data) != 32):
        print("static_to_json() expected 32 bytes, got {}".format(len(data)))
        return None

    ip = unpack(">L", data[0:4])[0]
    mask = unpack(">L", data[4:8])[0]
    gw = unpack(">L", data[8:12])[0]
    dns1 = unpack(">L", data[12:16])[0]
    dns2 = unpack(">L", data[16:20])[0]
    # <2 bytes padding ...>
    mtu = unpack(">H", data[22:24])[0]
    # <8 bytes padding ...>

    static['ip'] = str(ipaddress.IPv4Address(ip))
    static['mask'] = str(ipaddress.IPv4Address(mask))
    static['gw'] = str(ipaddress.IPv4Address(gw))
    static['dns1'] = str(ipaddress.IPv4Address(dns1))
    static['dns2'] = str(ipaddress.IPv4Address(dns2))
    static['mtu'] = mtu

    print(static)
    return static


# -----------------------------------------------------------------------------
# netconf object definition

class netconf(object):
    """ Object for representing some Wii network configuration data.
    There are header bytes, then three connection profiles.
    Much of the binary format is not-well-understood, or just NUL padding.

    IMPORTANT: The header fields are currently just stored as a string of
    hex bytes. The structure of this is not-well-understood; *however*,
    it does appear to have some effect on whether or not a USB Ethernet
    adapter can be used. In my testing, the USB Ethernet adapter only
    seems to work when the header is set to "0000000002070000". All of the
    wireless profiles I've tested have had this set to "0000000001070000".
    I suspect that fifth byte may be necessary to enable a wired connection.

    Data is stored in a dictionary of the form:

        {   'header': <hex string representation of 8-byte header>,
            'conn': [ <array of three connection profiles> ], }

    A connection profile is a dictionary of the form:

        {   'flags': <dictionary representing profile flags>,
            'static': <dictionary representing static network config>,
            'ssid': <string up to 32 byte>,
            'enc': <1 byte integer>,
            'key': <64-byte string>, }

    A set of flags is a dictionary of the form:

        {   'active': bool,
            'check-passed': bool,
            'use-proxy': bool,
            'use-dhcp-addr': bool,
            'use-dhcp-dns': bool,
            'use-wired': bool, }

    A set of static network configuration is a dictionary of the form:

        {   'ip': string,
            'mask': string,
            'gw': string,
            'dns1': string,
            'dns2': string, }
    """

    def __init__(self):
        """ Constructor """
        self.data = None

    def load_binary(self, filename):
        """ Given a path to some binary config.dat, parse it into some JSON
        representation stored on this object """

        with open(filename, "rb") as f:

            ''' For now we need to assume an input config.dat of 0x1b5c bytes.
            Just error out if the user fails to provide this to us. '''

            f.seek(0, os.SEEK_END)
            sz = f.tell()
            f.seek(0, os.SEEK_SET)

            if (sz != FILE_LEN):
                print("[!] Expected 0x1b5c bytes (got 0x{:x})".format(sz))
                exit(-1)

            ''' Parse up each of the three structures describing the three
            connection profiles. We're just seeking over a bunch of fields for
            now (until I get around to handling them). '''

            self.data = { "header": None, "conn": [] }

            # Header bytes
            self.data['header'] = binascii.hexlify(f.read(8)).decode("utf-8")

            for i in range(0, 3):
                self.data['conn'].append({})

                # Connection flags
                self.data['conn'][i]['flags'] = flags_to_json(pack("<c", f.read(1))[0])
                f.seek(3, os.SEEK_CUR)

                # Static network configuration
                self.data['conn'][i]['static'] = static_to_json(f.read(32))
                #f.seek(32, os.SEEK_CUR)

                # Proxy configuration and padding
                f.seek(327, os.SEEK_CUR)
                f.seek(1, os.SEEK_CUR)

                # Proxy configuration and padding, again
                f.seek(327, os.SEEK_CUR)
                f.seek(1297, os.SEEK_CUR)

                # SSID and padding
                self.data['conn'][i]['ssid'] = f.read(32).decode("utf-8").rstrip('\0')
                f.seek(1, os.SEEK_CUR)

                # SSID length, then some padding
                f.seek(1, os.SEEK_CUR)
                f.seek(2, os.SEEK_CUR)
                f.seek(1, os.SEEK_CUR)

                # Wi-Fi encryption scheme, and padding
                self.data['conn'][i]['enc'] = pack("<c", f.read(1))[0]
                f.seek(2, os.SEEK_CUR)
                f.seek(1, os.SEEK_CUR)

                # Encryption key length, then padding bytes
                f.seek(1, os.SEEK_CUR)
                f.seek(1, os.SEEK_CUR)
                f.seek(1, os.SEEK_CUR)

                # Encryption key contents
                self.data['conn'][i]['key'] = f.read(64).decode("utf-8").rstrip('\0')
                f.seek(236, os.SEEK_CUR)

            print("[*] Loaded {}".format(filename))


    def save_binary(self, filename):
        """ Save binary representation of data to some output file """

        output = bytearray()

        # Header bytes
        header_bytes = bytearray(binascii.unhexlify(self.data['header']))
        output.extend(header_bytes)

        for i in range(0, 3):
            # Convert flags into a byte
            output.append(json_to_flags(self.data['conn'][i]['flags']))
            output.extend(b'\x00' * 3)

            # Static network configuration
            #output.extend(b'\x00' * 32)
            ip_bytes = pack(">L", int(ipaddress.IPv4Address(self.data['conn'][i]['static']['ip'])))
            mask_bytes = pack(">L", int(ipaddress.IPv4Address(self.data['conn'][i]['static']['mask'])))
            gw_bytes = pack(">L", int(ipaddress.IPv4Address(self.data['conn'][i]['static']['gw'])))
            dns1_bytes = pack(">L", int(ipaddress.IPv4Address(self.data['conn'][i]['static']['dns1'])))
            dns2_bytes = pack(">L", int(ipaddress.IPv4Address(self.data['conn'][i]['static']['dns2'])))
            mtu_bytes = pack(">H", self.data['conn'][i]['static']['mtu'])

            output.extend(ip_bytes)
            output.extend(mask_bytes)
            output.extend(gw_bytes)
            output.extend(dns1_bytes)
            output.extend(dns2_bytes)
            output.extend(b'\x00' * 2)
            output.extend(mtu_bytes)
            output.extend(b'\x00' * 8)

            # Proxy configuration
            output.extend(b'\x00' * 327)
            output.extend(b'\x00' * 1)
            output.extend(b'\x00' * 327)
            output.extend(b'\x00' * 1297)

            # SSID
            ssid_len = len(self.data['conn'][i]['ssid'])
            ssid_len_bytes = ssid_len.to_bytes(1, byteorder='little')
            if (ssid_len > 32):
                print("[!] Connection {} SSID name must be less than 32 bytes")
                print("[!] Exiting ...")
                exit(-1)
            ssid_bytes = bytearray(self.data['conn'][i]['ssid'].encode('utf-8'))
            ssid_bytes = ssid_bytes.ljust(32, b'\x00')

            output.extend(ssid_bytes)
            output.extend(b'\x00' * 1)
            output.extend(ssid_len_bytes)
            output.extend(b'\x00' * 2)
            output.extend(b'\x00' * 1)

            # Wi-fi encryption scheme
            output.append(self.data['conn'][i]['enc'])
            output.extend(b'\x00' * 2)
            output.extend(b'\x00' * 1)

            # Encryption key
            key_len = len(self.data['conn'][i]['key'])
            key_len_bytes = key_len.to_bytes(1, byteorder='little')
            if (key_len > 64):
                print("[!] Connection {} key length must be less than 64 bytes")
                print("[!] Exiting ...")
                exit(-1)
            key_bytes = bytearray(self.data['conn'][i]['key'].encode('utf-8'))
            key_bytes = key_bytes.ljust(64, b'\x00')

            output.extend(key_len_bytes)
            output.extend(b'\x00' * 1)
            output.extend(b'\x00' * 1)
            output.extend(key_bytes)
            output.extend(b'\x00' * 236)

        if (len(output) != FILE_LEN):
            print("[!] output bytearray() is only 0x{:x} bytes".format(
                len(output)))
            exit(-1)

        with open(filename, "wb") as f:
                f.write(output)
                print("[*] Wrote to {}".format(filename))


    def save_json(self, filename):
        """ Save JSON representation of data to some output file """
        with open(filename, "w") as f:
            json.dump(self.data, f)
            print("[*] Wrote to {}".format(filename))


    def load_json(self, filename):
        """ Load JSON representation of data into this out object """
        with open(filename, "r") as f:
            self.data = json.load(f)
            print("[*] Loaded {}".format(filename))


    def dump(self):
        """ Write human-readable data to stdout """
        print(json.dumps(self.data, indent=2))

# -----------------------------------------------------------------------------
# Deal with command-line arguments

if (len(sys.argv) < 4):
    usage()

conf = netconf()

if (sys.argv[1] == "--to-json"):
    conf.load_binary(sys.argv[2])
    conf.save_json(sys.argv[3])
    exit(0)
elif (sys.argv[1] == "--to-binary"):
    conf.load_json(sys.argv[2])
    conf.save_binary(sys.argv[3])
    exit(0)
else:
    usage()
