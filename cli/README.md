[![Build Status](https://dev.azure.com/sjmulder/netwake/_apis/build/status/netwake?branchName=master)](https://dev.azure.com/sjmulder/netwake/_build/latest?definitionId=8&branchName=master)

Netwake CLI
===========
Simple Wake-on-LAN utility.

**netwake** *mac-address* | *name*

**[Other versions](..)**

Usage
-----
With Netwake, you can wake up a computer over the network using the
Wake-on-LAN protocol.

 1. Enable Wake-on-LAN on the target computer. Note the network card's
    MAC address.
 2. Build the Netwake CLI (see below) or install it with your package
    manager.
 3. Run `netwake <mac-address>` (e.g. `netwake 01:23:45:67:89:ab`).

To use names instead of MAC addresses, create a file at any of these
locations:

 - `~/.config/woltab`
 - `~/.woltab`
 - `/etc/woltab`
 - or set `$WOLTAB` to a path.

Then add entries like so:

    # mac-address name
    01:23:45:67:89:0a my-pc

Now you can do `netwake my-pc`.

Problems? Missing features? Ideas? File an
[issue](https://github.com/sjmulder/netwake/issues)!

Building
--------
Should work on any Unix, including Linux and macOS:

   cd cli/
   make
   sudo make install

Author
------
Sijmen J. Mulder (<ik@sjmulder.nl>)