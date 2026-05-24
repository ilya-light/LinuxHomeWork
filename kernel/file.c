#include <linux/buffer_head.h>
#include <linux/crc32.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "myfs.h"

static ssize_t myfs_read(struct file *file,
                         char __user *buf,
                         size_t len,
                         loff_t *ppos)
{
    struct inode *inode = file_inode(file);
    struct super_block *sb = inode->i_sb;
    struct myfs_sb_info *sbi = MYFS_SB(sb);
    u32 file_id;
    loff_t pos = *ppos;
    ssize_t done = 0;
    u64 file_size;

    if (sbi->erased)
        return -ENODEV;

    if (!myfs_is_file_ino(sbi, inode->i_ino))
        return -EINVAL;

    file_id = inode->i_ino - MYFS_FIRST_FILE_INO;
    file_size = myfs_file_size_bytes(sbi);

    if (pos < 0)
        return -EINVAL;
    if (pos >= file_size)
        return 0;
    if (len > file_size - pos)
        len = file_size - pos;

    mutex_lock(&sbi->lock);
    while (len > 0) {
        u64 file_sector = div_u64(pos, MYFS_SECTOR_SIZE);
        u32 sector_off = do_div(pos, MYFS_SECTOR_SIZE);
        u64 data_index = (u64)file_id * sbi->file_size_sectors + file_sector;
        u64 phys = myfs_data_index_to_sector(sbi, data_index);
        size_t chunk = min_t(size_t, len, MYFS_SECTOR_SIZE - sector_off);
        struct buffer_head *bh;

        pos = *ppos + done;

        bh = sb_bread(sb, phys);
        if (!bh) {
            if (!done)
                done = -EIO;
            break;
        }

        if (copy_to_user(buf + done, bh->b_data + sector_off, chunk)) {
            brelse(bh);
            if (!done)
                done = -EFAULT;
            break;
        }

        brelse(bh);
        done += chunk;
        len -= chunk;
        pos = *ppos + done;
    }
    mutex_unlock(&sbi->lock);

    if (done > 0) {
        *ppos += done;
        inode_set_atime_to_ts(inode, current_time(inode));
    }

    return done;
}

static ssize_t myfs_write(struct file *file,
                          const char __user *buf,
                          size_t len,
                          loff_t *ppos)
{
    struct inode *inode = file_inode(file);
    struct super_block *sb = inode->i_sb;
    struct myfs_sb_info *sbi = MYFS_SB(sb);
    u32 file_id;
    loff_t pos = *ppos;
    ssize_t done = 0;
    u64 file_size;

    if (sbi->erased)
        return -ENODEV;

    if (!myfs_is_file_ino(sbi, inode->i_ino))
        return -EINVAL;

    file_id = inode->i_ino - MYFS_FIRST_FILE_INO;
    file_size = myfs_file_size_bytes(sbi);

    if (pos < 0)
        return -EINVAL;
    if (pos >= file_size)
        return -ENOSPC;
    if (len > file_size - pos)
        len = file_size - pos;

    mutex_lock(&sbi->lock);
    while (len > 0) {
        u64 file_sector;
        u32 sector_off;
        u64 data_index;
        u64 phys;
        size_t chunk;
        struct buffer_head *bh;
        u64 tmp_pos = *ppos + done;

        file_sector = div_u64(tmp_pos, MYFS_SECTOR_SIZE);
        sector_off = do_div(tmp_pos, MYFS_SECTOR_SIZE);
        data_index = (u64)file_id * sbi->file_size_sectors + file_sector;
        phys = myfs_data_index_to_sector(sbi, data_index);
        chunk = min_t(size_t, len, MYFS_SECTOR_SIZE - sector_off);

        bh = sb_bread(sb, phys);
        if (!bh) {
            if (!done)
                done = -EIO;
            break;
        }

        if (copy_from_user(bh->b_data + sector_off, buf + done, chunk)) {
            brelse(bh);
            if (!done)
                done = -EFAULT;
            break;
        }

        mark_buffer_dirty(bh);
        sync_dirty_buffer(bh);
        brelse(bh);
        done += chunk;
        len -= chunk;
    }
    mutex_unlock(&sbi->lock);

    if (done > 0) {
        struct timespec64 now = current_time(inode);

        *ppos += done;
        inode_set_mtime_to_ts(inode, now);
        inode_set_ctime_to_ts(inode, now);
    }

    return done;
}

u32 myfs_file_crc32(struct super_block *sb, u32 file_id, int *errp)
{
    struct myfs_sb_info *sbi = MYFS_SB(sb);
    u32 crc = 0;
    u32 i;

    *errp = 0;

    if (file_id >= sbi->file_count) {
        *errp = -ENOENT;
        return 0;
    }

    for (i = 0; i < sbi->file_size_sectors; i++) {
        u64 data_index = (u64)file_id * sbi->file_size_sectors + i;
        u64 phys = myfs_data_index_to_sector(sbi, data_index);
        struct buffer_head *bh = sb_bread(sb, phys);

        if (!bh) {
            *errp = -EIO;
            return 0;
        }

        crc = crc32_le(crc, bh->b_data, MYFS_SECTOR_SIZE);
        brelse(bh);
    }

    return crc;
}

const struct file_operations myfs_file_ops = {
    .owner = THIS_MODULE,
    .read = myfs_read,
    .write = myfs_write,
    .llseek = generic_file_llseek,
    .unlocked_ioctl = myfs_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = myfs_ioctl,
#endif
};
