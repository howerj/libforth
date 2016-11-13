# Forth interpreter

This is a branch of the [libforth][] interpreter designed to be run as a
[Linux][] [Kernel Module][]. It implements a character device that accepts
[Forth][] commands, from which input can be read back from it.

The project is a work in progress and likely to be very buggy, potentially
making your system unstable.

## Progress

The kernel module currently:

* loads
* makes a device called **forth** under **/dev/**
* initializes the Forth interpreter
* accepts input
* is very buggy

## Future direction

The idea of this module is that it could be used as a debugger of sorts for the
kernel, if a better one is not available, and would become more integrated will
become more featureful over time (as I learn more about kernel development). It
is more of a toy than anything at the moment, so should not be used for
anything serious.

Eventually block devices that roughly correspond to [Forth blocks][] could be
added, for the moment the module implements a single, simple, 
[character device][] that can read in, and execute, commands. 

## Forth documentation

The documentation is non-existent at the moment, and will not be rewritten
until the features and implementation have become somewhat stable. As such, the
only documentation is the source, [forth.c][].

However, here is some minimal information to get you start:

### building

This module was built against kernel version 3.16, on Debian 8, x86-64. There
is nothing hardware specific or special about the module, it should be quite
portable.

To build:

	make

This should build a test program, called **test**, and a kernel module,
**forth.ko**.

To load the module, as *root*:

	insmod forth.ko
	
You can view the output of [dmesg][], the module is quite verbose and will tell
you if it succeeded. If it did, **/dev/forth** should have now appeared on your
system.

To run the test program, [test.c][], again as *root*:

	./test

This should sent a simple test program to the device, the program then reads
back the output, then exits.

The module will also create files under:

	/sys/class/forth

The devices under here will be subject to a lot of change, so will not be
documented for now.

## Security

Much like **/dev/mem** and **/dev/kmem** this device should be accessible by
no one but the *root* user. The [udev][] rules in the project, in the file
[99-forth.rules][] set the device up to be only readable and writable by
*root*.

## License

The license for this branch has changed from an [MIT][] license to a [GPL][] 
one as used by the rest of the kernel.

[libforth]: https://github.com/howerj/libforth
[Linux]: https://en.wikipedia.org/wiki/Linux
[Kernel Module]: https://en.wikipedia.org/wiki/Loadable_kernel_module
[Forth]: https://en.wikipedia.org/wiki/Forth_%28programming_language%29
[GPL]: https://www.kernel.org/pub/linux/kernel/COPYING
[MIT]: https://en.wikipedia.org/wiki/MIT_License
[udev]: https://en.wikipedia.org/wiki/Udev
[99-forth.rules]: 99-forth.rules
[forth.c]: forth.c
[test.c]: test.c
[Forth blocks]: http://wiki.c2.com/?ForthBlocks
[character device]: https://en.wikipedia.org/wiki/Device_file#Character_devices
[dmesg]: http://www.linfo.org/dmesg.html

<style type="text/css">body{margin:40px auto;max-width:850px;line-height:1.6;font-size:16px;color:#444;padding:0 10px}h1,h2,h3{line-height:1.2}</style>
