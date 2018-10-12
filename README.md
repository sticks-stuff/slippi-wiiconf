# wii-netconf
Somewhat hacky tool for changing Wii network settings. Not designed to be
particularly user-friendly or safe [yet].

## Usage
Build this DOL (you'll devkitPPC and libogc) first, and then boot it
(If you're using the HBC, you can just copy the `apps/wii-netconf` folder
onto some storage media and boot it that way if you'd like).

You'll need to have an SD card attached in order for this to work.
The `wii-netconf.py` script in this directory will translate between binary
and JSON representations of network settings for you. Read the script if
you're interested in any of the technical details.

In the DOL:
* Pressing **A** will read your current network settings to `current_config.dat`
  on the root of your SD card
* Pressing **B** will read a `new_config.dat` from the root of your SD card and
  write it back to your console's NAND flash

## Output example
Here's an example of what a WPA2 Wi-Fi configuration with DHCP looks like:

```
{"header": "0000000001070000", "conn": [{"flags": {"active": true, "check-passed": true, "use-proxy": false, "use-dhcp-addr": true, "use-dhcp-dns": true, "use-wired": false}, "static": {"ip": "0.0.0.0", "mask": "0.0.0.0", "gw": "0.0.0.0", "dns1": "0.0.0.0", "dns2": "0.0.0.0", "mtu": 0}, "ssid": "MyWifiNetwork", "enc": 5, "key": "MyWirelessKey123"}, {"flags": {"active": false, "check-passed": false, "use-proxy": false, "use-dhcp-addr": false, "use-dhcp-dns": false, "use-wired": false}, "static": {"ip": "0.0.0.0", "mask": "0.0.0.0", "gw": "0.0.0.0", "dns1": "0.0.0.0", "dns2": "0.0.0.0", "mtu": 0}, "ssid": "", "enc": 0, "key": ""}, {"flags": {"active": false, "check-passed": false, "use-proxy": false, "use-dhcp-addr": false, "use-dhcp-dns": false, "use-wired": false}, "static": {"ip": "0.0.0.0", "mask": "0.0.0.0", "gw": "0.0.0.0", "dns1": "0.0.0.0", "dns2": "0.0.0.0", "mtu": 0}, "ssid": "", "enc": 0, "key": ""}]}
```

... and here's an example of a static configuration with a USB Ethernet adapter:

```
{"header": "0000000002070000", "conn": [{"flags": {"active": true, "check-passed": true, "use-proxy": false, "use-dhcp-addr": false, "use-dhcp-dns": false, "use-wired": true}, "static": {"ip": "10.200.200.5", "mask": "255.255.255.0", "gw": "10.200.200.1", "dns1": "10.0.0.1", "dns2": "0.0.0.0", "mtu": 0}, "ssid": "", "enc": 0, "key": ""}, {"flags": {"active": false, "check-passed": false, "use-proxy": false, "use-dhcp-addr": false, "use-dhcp-dns": false, "use-wired": false}, "static": {"ip": "0.0.0.0", "mask": "0.0.0.0", "gw": "0.0.0.0", "dns1": "0.0.0.0", "dns2": "0.0.0.0", "mtu": 0}, "ssid": "", "enc": 0, "key": ""}, {"flags": {"active": false, "check-passed": false, "use-proxy": false, "use-dhcp-addr": false, "use-dhcp-dns": false, "use-wired": false}, "static": {"ip": "0.0.0.0", "mask": "0.0.0.0", "gw": "0.0.0.0", "dns1": "0.0.0.0", "dns2": "0.0.0.0", "mtu": 0}, "ssid": "", "enc": 0, "key": ""}]}
```

**Important:** Note that the fifth byte in the header is set to `0x02` in the
wired connection example. In my testing, a USB Ethernet adapter will not work
when this byte is set to `0x01`. This should be sorted out automatically in
the future after the file format is better-understood.
