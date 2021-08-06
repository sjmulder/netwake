Netwake CLI
===========
Simple Wake-on-LAN utility.

**[Other versions](https://github.com/sjmulder/netwake)**

Usage
-----
**netwake** [**-V**] *mac-address*|*name* ...

With Netwake, you can wake up computers over the network using the
Wake-on-LAN protocol.

 1. Enable Wake-on-LAN on the target computer(s). Note the network
    card's MAC address.
 2. Build the Netwake CLI (see below) or install it with your package
    manager.
 3. Run `netwake <mac-address>`, e.g. `netwake 01:23:45:67:89:ab`.

Multiple targets can be passed. If an error occurs, a message is
printed and execution continues with the next target.

With **-V**, netwake prints its version number and exits.

To use names instead of MAC addresses, create a file at
`~/.config/woltab`, `~/.woltab`, `/etc/woltab` or set `$WOLTAB` to a
path, then add entries like so:

    # mac-address name
    12:34:56:78:9a:bc sijmens-pc
    34:56:78:9a:bc:de sijmens-rpi

Now you can use `netwake sijmens-pc`, or even `netwake sijmens-pc
sijmens-rpi` to boot both at once.

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
