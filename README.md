# wii-netconf
Hacky experiment for changing Wii network settings. Not designed to be 
user-friendly or safe [yet].

## Usage
Build the DOL and boot it (you'll devkitPPC and libogc). 

You'll need to have a USB storage device attached to your Wii in order for this 
to work. No SD card support right now. I'm using the `wii-netconf.py` script to 
translate between binary/JSON representations of network settings. 
Read the code for more details (there are lots of things I've left out). 

In the DOL:
* Pressing **A** will read your current network settings to `current_config.dat`.
* Pressing **B** will read a `new_config.dat` and write it to your NAND flash.
