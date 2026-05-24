#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x349e7f77, "sb_set_blocksize" },
	{ 0xdf6200b1, "current_time" },
	{ 0x69acdf38, "memcpy" },
	{ 0x3b6c41ea, "kstrtouint" },
	{ 0x37a0cba, "kfree" },
	{ 0xb1b2666b, "register_filesystem" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x65487097, "__x86_indirect_thunk_rax" },
	{ 0x518cb730, "kill_block_super" },
	{ 0x1af66bb, "unlock_new_inode" },
	{ 0x122c3a7e, "_printk" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0xa916b694, "strnlen" },
	{ 0xefd28f3f, "const_pcpu_hot" },
	{ 0xac61570e, "__brelse" },
	{ 0xcafe7778, "sync_dirty_buffer" },
	{ 0x69dd3b5b, "crc32_le" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0x47af53a7, "set_nlink" },
	{ 0xcefb0c9f, "__mutex_init" },
	{ 0x9f1b4749, "__bread_gfp" },
	{ 0x75ca79b5, "__fortify_panic" },
	{ 0x31549b2a, "__x86_indirect_thunk_r10" },
	{ 0xfcdf1335, "param_ops_charp" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0xe2d5255a, "strcmp" },
	{ 0x12066cf5, "d_add" },
	{ 0x67041de9, "mount_bdev" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0xd8ba0fc0, "__kmalloc_cache_noprof" },
	{ 0xda4fdc5f, "d_parent_ino" },
	{ 0x4b487466, "generic_file_llseek" },
	{ 0xf18a3a1b, "kmalloc_caches" },
	{ 0x608c74cb, "iget_failed" },
	{ 0x66a22499, "iget_locked" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x8808d55, "param_ops_uint" },
	{ 0x2cb0fdc2, "param_ops_ulong" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0xafd744c6, "__x86_indirect_thunk_rbp" },
	{ 0x52c5c991, "__kmalloc_noprof" },
	{ 0xa83dc460, "unregister_filesystem" },
	{ 0x99a8da74, "mark_buffer_dirty" },
	{ 0xcda5460e, "simple_statfs" },
	{ 0xad2340c4, "d_make_root" },
	{ 0x96848186, "scnprintf" },
	{ 0xbf1981cb, "module_layout" },
};

MODULE_INFO(depends, "");

