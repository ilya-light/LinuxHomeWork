#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "myfs.h"

static long myfs_ioctl_hashes(struct super_block *sb, unsigned long arg)
{
    struct myfs_sb_info *sbi = MYFS_SB(sb);
    struct myfs_hashes_request req;
    struct myfs_file_hash *items = NULL;
    u32 i;
    int ret = 0;

    if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
        return -EFAULT;

    req.count = sbi->file_count;

    if (req.capacity < req.count || req.items_ptr == 0) {
        if (copy_to_user((void __user *)arg, &req, sizeof(req)))
            return -EFAULT;
        return -ENOSPC;
    }

    items = kcalloc(req.count, sizeof(*items), GFP_KERNEL);
    if (!items)
        return -ENOMEM;

    mutex_lock(&sbi->lock);
    for (i = 0; i < req.count; i++) {
        int err;

        items[i].file_id = i;
        items[i].crc32 = myfs_file_crc32(sb, i, &err);
        if (err) {
            ret = err;
            break;
        }
    }
    mutex_unlock(&sbi->lock);

    if (!ret && copy_to_user((void __user *)(uintptr_t)req.items_ptr,
                             items,
                             sizeof(*items) * req.count))
        ret = -EFAULT;

    if (!ret && copy_to_user((void __user *)arg, &req, sizeof(req)))
        ret = -EFAULT;

    kfree(items);
    return ret;
}

static long myfs_ioctl_mapping(struct super_block *sb, unsigned long arg)
{
    struct myfs_sb_info *sbi = MYFS_SB(sb);
    struct myfs_mapping_request req;
    u64 *sectors = NULL;
    u32 file_id;
    u32 i;
    size_t name_len;
    int ret;

    if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
        return -EFAULT;

    req.filename[MYFS_MAX_IOCTL_NAME - 1] = '\0';
    name_len = strnlen(req.filename, MYFS_MAX_IOCTL_NAME);

    ret = myfs_parse_filename(sbi, req.filename, name_len, &file_id);
    if (ret)
        return ret;

    req.file_id = file_id;
    req.sector_count = sbi->file_size_sectors;

    if (req.capacity < req.sector_count || req.sectors_ptr == 0) {
        if (copy_to_user((void __user *)arg, &req, sizeof(req)))
            return -EFAULT;
        return -ENOSPC;
    }

    sectors = kcalloc(req.sector_count, sizeof(*sectors), GFP_KERNEL);
    if (!sectors)
        return -ENOMEM;

    for (i = 0; i < req.sector_count; i++) {
        u64 data_index = (u64)file_id * sbi->file_size_sectors + i;

        sectors[i] = myfs_data_index_to_sector(sbi, data_index);
    }

    if (copy_to_user((void __user *)(uintptr_t)req.sectors_ptr,
                     sectors,
                     sizeof(*sectors) * req.sector_count)) {
        ret = -EFAULT;
        goto out;
    }

    if (copy_to_user((void __user *)arg, &req, sizeof(req)))
        ret = -EFAULT;

out:
    kfree(sectors);
    return ret;
}

long myfs_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct inode *inode = file_inode(file);
    struct super_block *sb = inode->i_sb;
    struct myfs_sb_info *sbi = MYFS_SB(sb);
    int ret;

    if (sbi->erased && cmd != MYFS_IOC_ERASE_FS)
        return -ENODEV;

    switch (cmd) {
    case MYFS_IOC_ZERO_ALL:
        mutex_lock(&sbi->lock);
        ret = myfs_zero_all_files(sb);
        mutex_unlock(&sbi->lock);
        return ret;

    case MYFS_IOC_ERASE_FS:
        mutex_lock(&sbi->lock);
        ret = myfs_erase_fs(sb);
        mutex_unlock(&sbi->lock);
        return ret;

    case MYFS_IOC_GET_HASHES:
        return myfs_ioctl_hashes(sb, arg);

    case MYFS_IOC_GET_MAPPING:
        return myfs_ioctl_mapping(sb, arg);

    default:
        return -ENOTTY;
    }
}
