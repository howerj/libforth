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
[Forth blocks]: http://wiki.c2.com/?ForthBlocks
[character device]: https://en.wikipedia.org/wiki/Device_file#Character_devices

<style type="text/css">body{margin:40px auto;max-width:850px;line-height:1.6;font-size:16px;color:#444;padding:0 10px}h1,h2,h3{line-height:1.2}</style>
