#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>
#include "myfs.h"

char *disk_name;
unsigned long sb1_sector = 0;
unsigned long sb2_sector = 128;
unsigned int max_filename_len = 32;
unsigned int file_size_sectors = 1;

module_param(disk_name, charp, 0444);
MODULE_PARM_DESC(disk_name, "Block device path allowed for mount, for example /dev/loop10");
module_param(sb1_sector, ulong, 0444);
MODULE_PARM_DESC(sb1_sector, "Sector offset of the first superblock copy");
module_param(sb2_sector, ulong, 0444);
MODULE_PARM_DESC(sb2_sector, "Sector offset of the second superblock copy");
module_param(max_filename_len, uint, 0444);
MODULE_PARM_DESC(max_filename_len, "Maximum generated file name length");
module_param(file_size_sectors, uint, 0444);
MODULE_PARM_DESC(file_size_sectors, "Fixed file size in sectors");

static int myfs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct myfs_sb_info *sbi;
    struct inode *root_inode;
    int ret;

    if (!sb_set_blocksize(sb, MYFS_SECTOR_SIZE))
        return -EINVAL;

    sbi = kzalloc(sizeof(*sbi), GFP_KERNEL);
    if (!sbi)
        return -ENOMEM;

    mutex_init(&sbi->lock);
    sb->s_fs_info = sbi;
    sb->s_magic = MYFS_MAGIC;
    sb->s_op = &myfs_super_ops;

    ret = myfs_load_or_format_super(sb, sbi);
    if (ret)
        goto fail;

    root_inode = myfs_iget(sb, MYFS_ROOT_INO);
    if (IS_ERR(root_inode)) {
        ret = PTR_ERR(root_inode);
        goto fail;
    }

    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root) {
        ret = -ENOMEM;
        goto fail;
    }

    return 0;

fail:
    kfree(sbi);
    sb->s_fs_info = NULL;
    return ret;
}

static struct dentry *myfs_mount(struct file_system_type *fs_type,
                                 int flags,
                                 const char *dev_name,
                                 void *data)
{
    if (!disk_name || !*disk_name) {
        pr_err("myfs: disk_name module parameter is required\n");
        return ERR_PTR(-EINVAL);
    }

    if (strcmp(dev_name, disk_name) != 0) {
        pr_err("myfs: mounted device '%s' does not match disk_name '%s'\n",
               dev_name, disk_name);
        return ERR_PTR(-EINVAL);
    }

    return mount_bdev(fs_type, flags, dev_name, data, myfs_fill_super);
}

static struct file_system_type myfs_fs_type = {
    .owner = THIS_MODULE,
    .name = "myfs",
    .mount = myfs_mount,
    .kill_sb = kill_block_super,
    .fs_flags = FS_REQUIRES_DEV,
};

static int __init myfs_init(void)
{
    if (!disk_name || !*disk_name) {
        pr_err("myfs: disk_name is required\n");
        return -EINVAL;
    }

    if (sb1_sector == sb2_sector) {
        pr_err("myfs: superblock sectors must be different\n");
        return -EINVAL;
    }

    if (file_size_sectors < 1) {
        pr_err("myfs: file_size_sectors must be >= 1\n");
        return -EINVAL;
    }

    if (max_filename_len < MYFS_MIN_NAME_LEN) {
        pr_err("myfs: max_filename_len must be >= %u\n", MYFS_MIN_NAME_LEN);
        return -EINVAL;
    }

    return register_filesystem(&myfs_fs_type);
}

static void __exit myfs_exit(void)
{
    unregister_filesystem(&myfs_fs_type);
}

module_init(myfs_init);
module_exit(myfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Educational assignment implementation");
MODULE_DESCRIPTION("Simple fixed-file block-device filesystem with duplicated superblock and IOCTLs");
