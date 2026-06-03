// SPDX-License-Identifier: GPL-2.0
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "myfs.h"

static u32 myfs_sb_crc32(const struct myfs_disk_superblock *dsb)
{
    struct myfs_disk_superblock tmp = *dsb;

    tmp.checksum = 0;
    return crc32_le(0, (const unsigned char *)&tmp, sizeof(tmp));
}

static bool myfs_validate_disk_super(const struct myfs_disk_superblock *dsb,
                                     u64 total_sectors)
{
    if (le32_to_cpu(dsb->magic) != MYFS_MAGIC)
        return false;
    if (le32_to_cpu(dsb->version) != MYFS_VERSION)
        return false;
    if (le32_to_cpu(dsb->sector_size) != MYFS_SECTOR_SIZE)
        return false;
    if (le64_to_cpu(dsb->total_sectors) != total_sectors)
        return false;
    if (le64_to_cpu(dsb->sb1_sector) != sb1_sector)
        return false;
    if (le64_to_cpu(dsb->sb2_sector) != sb2_sector)
        return false;
    if (le32_to_cpu(dsb->file_size_sectors) != file_size_sectors)
        return false;
    if (le32_to_cpu(dsb->max_filename_len) != max_filename_len)
        return false;
    if (le32_to_cpu(dsb->checksum) != myfs_sb_crc32(dsb))
        return false;
    return true;
}

static int myfs_read_disk_super(struct super_block *sb,
                                u64 sector,
                                struct myfs_disk_superblock *out)
{
    struct buffer_head *bh;

    bh = sb_bread(sb, sector);
    if (!bh)
        return -EIO;

    memcpy(out, bh->b_data, sizeof(*out));
    brelse(bh);
    return 0;
}

static int myfs_write_disk_super(struct super_block *sb,
                                 u64 sector,
                                 const struct myfs_disk_superblock *dsb)
{
    struct buffer_head *bh;

    bh = sb_bread(sb, sector);
    if (!bh)
        return -EIO;

    memset(bh->b_data, 0, MYFS_SECTOR_SIZE);
    memcpy(bh->b_data, dsb, sizeof(*dsb));
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    return 0;
}

static void myfs_fill_new_disk_super(struct myfs_disk_superblock *dsb,
                                     u64 total_sectors,
                                     u32 file_count)
{
    memset(dsb, 0, sizeof(*dsb));
    dsb->magic = cpu_to_le32(MYFS_MAGIC);
    dsb->version = cpu_to_le32(MYFS_VERSION);
    dsb->generation = cpu_to_le64(1);
    dsb->total_sectors = cpu_to_le64(total_sectors);
    dsb->sb1_sector = cpu_to_le64(sb1_sector);
    dsb->sb2_sector = cpu_to_le64(sb2_sector);
    dsb->sector_size = cpu_to_le32(MYFS_SECTOR_SIZE);
    dsb->file_size_sectors = cpu_to_le32(file_size_sectors);
    dsb->max_filename_len = cpu_to_le32(max_filename_len);
    dsb->file_count = cpu_to_le32(file_count);
    dsb->checksum = cpu_to_le32(myfs_sb_crc32(dsb));
}

static void myfs_import_super(struct myfs_sb_info *sbi,
                              const struct myfs_disk_superblock *dsb)
{
    sbi->dsb = *dsb;
    sbi->total_sectors = le64_to_cpu(dsb->total_sectors);
    sbi->sb1_sector = le64_to_cpu(dsb->sb1_sector);
    sbi->sb2_sector = le64_to_cpu(dsb->sb2_sector);
    sbi->file_size_sectors = le32_to_cpu(dsb->file_size_sectors);
    sbi->max_filename_len = le32_to_cpu(dsb->max_filename_len);
    sbi->file_count = le32_to_cpu(dsb->file_count);
    sbi->erased = false;
}

int myfs_write_super_copies(struct super_block *sb, struct myfs_sb_info *sbi)
{
    struct myfs_disk_superblock dsb = sbi->dsb;
    int ret;

    dsb.checksum = 0;
    dsb.checksum = cpu_to_le32(myfs_sb_crc32(&dsb));
    sbi->dsb = dsb;

    ret = myfs_write_disk_super(sb, sbi->sb1_sector, &dsb);
    if (ret)
        return ret;
    return myfs_write_disk_super(sb, sbi->sb2_sector, &dsb);
}

int myfs_load_or_format_super(struct super_block *sb, struct myfs_sb_info *sbi)
{
    u64 bytes = bdev_nr_bytes(sb->s_bdev);
    u64 total_sectors = bytes / MYFS_SECTOR_SIZE;
    u64 usable_sectors;
    u32 file_count;
    struct myfs_disk_superblock dsb1, dsb2, chosen;
    bool valid1, valid2;
    int ret1, ret2, ret;

    if (total_sectors < 4)
        return -EINVAL;

    if (sb1_sector >= total_sectors || sb2_sector >= total_sectors)
        return -EINVAL;

    usable_sectors = total_sectors - 2;
    file_count = div_u64(usable_sectors, file_size_sectors);
    if (file_count == 0)
        return -EINVAL;

    ret1 = myfs_read_disk_super(sb, sb1_sector, &dsb1);
    ret2 = myfs_read_disk_super(sb, sb2_sector, &dsb2);
    valid1 = !ret1 && myfs_validate_disk_super(&dsb1, total_sectors);
    valid2 = !ret2 && myfs_validate_disk_super(&dsb2, total_sectors);

    if (valid1 && valid2) {
        if (le64_to_cpu(dsb2.generation) > le64_to_cpu(dsb1.generation))
            chosen = dsb2;
        else
            chosen = dsb1;
        myfs_import_super(sbi, &chosen);
        return 0;
    }

    if (valid1 || valid2) {
        chosen = valid1 ? dsb1 : dsb2;
        myfs_import_super(sbi, &chosen);
        pr_warn("myfs: one superblock copy is invalid, restoring it\n");
        return myfs_write_super_copies(sb, sbi);
    }

    pr_info("myfs: no valid superblock found, formatting new filesystem\n");
    return 1;
}

static int myfs_zero_sector(struct super_block *sb, u64 sector)
{
    struct buffer_head *bh;

    bh = sb_bread(sb, sector);
    if (!bh)
        return -EIO;

    memset(bh->b_data, 0, MYFS_SECTOR_SIZE);
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    return 0;
}

int myfs_zero_all_files(struct super_block *sb)
{
    struct myfs_sb_info *sbi = MYFS_SB(sb);
    u64 data_sectors = (u64)sbi->file_count * sbi->file_size_sectors;
    u64 i;
    int ret;

    if (sbi->erased)
        return -ENODEV;

    for (i = 0; i < data_sectors; i++) {
        ret = myfs_zero_sector(sb, myfs_data_index_to_sector(sbi, i));
        if (ret)
            return ret;
    }

    return 0;
}

int myfs_erase_fs(struct super_block *sb)
{
    struct myfs_sb_info *sbi = MYFS_SB(sb);
    int ret;

    ret = myfs_zero_all_files(sb);
    if (ret)
        return ret;

    ret = myfs_zero_sector(sb, sbi->sb1_sector);
    if (ret)
        return ret;
    ret = myfs_zero_sector(sb, sbi->sb2_sector);
    if (ret)
        return ret;

    sbi->erased = true;
    return 0;
}

static void myfs_put_super(struct super_block *sb)
{
    struct myfs_sb_info *sbi = MYFS_SB(sb);

    kfree(sbi);
    sb->s_fs_info = NULL;
}

const struct super_operations myfs_super_ops = {
    .put_super = myfs_put_super,
    .statfs = simple_statfs,
};
