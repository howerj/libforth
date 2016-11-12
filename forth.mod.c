#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x2ab9dba5, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0xc8cd03aa, __VMLINUX_SYMBOL_STR(class_unregister) },
	{ 0x3924302, __VMLINUX_SYMBOL_STR(device_destroy) },
	{ 0x6bc3fbc0, __VMLINUX_SYMBOL_STR(__unregister_chrdev) },
	{ 0x6d36dc13, __VMLINUX_SYMBOL_STR(class_destroy) },
	{ 0x23beab60, __VMLINUX_SYMBOL_STR(__mutex_init) },
	{ 0xbb704422, __VMLINUX_SYMBOL_STR(device_create) },
	{ 0x5268a83d, __VMLINUX_SYMBOL_STR(__class_create) },
	{ 0xb4b37155, __VMLINUX_SYMBOL_STR(__register_chrdev) },
	{ 0x91715312, __VMLINUX_SYMBOL_STR(sprintf) },
	{ 0x7d11c268, __VMLINUX_SYMBOL_STR(jiffies) },
	{ 0x1b17e06c, __VMLINUX_SYMBOL_STR(kstrtoll) },
	{ 0x20c55ae0, __VMLINUX_SYMBOL_STR(sscanf) },
	{ 0x13307fde, __VMLINUX_SYMBOL_STR(vsscanf) },
	{ 0xc671e369, __VMLINUX_SYMBOL_STR(_copy_to_user) },
	{ 0x28318305, __VMLINUX_SYMBOL_STR(snprintf) },
	{ 0x55b9699c, __VMLINUX_SYMBOL_STR(mutex_trylock) },
	{ 0xdb7305a1, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0x1e12b70c, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0xbf8ba54a, __VMLINUX_SYMBOL_STR(vprintk) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x99195078, __VMLINUX_SYMBOL_STR(vsnprintf) },
	{ 0xe914e41e, __VMLINUX_SYMBOL_STR(strcpy) },
	{ 0x11089ac7, __VMLINUX_SYMBOL_STR(_ctype) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "5C2F15CFE64E3788ED70DFE");
