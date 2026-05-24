#ifndef MYFS_H
#define MYFS_H

#include <linux/buffer_head.h>
#include <linux/crc32.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include "myfs_ioctl.h"

#define MYFS_MAGIC 0x4D594653U
#define MYFS_VERSION 1U
#define MYFS_SECTOR_SIZE 512U

#define MYFS_ROOT_INO 1UL
#define MYFS_FIRST_FILE_INO 2UL
#define MYFS_FILE_PREFIX "file_"
#define MYFS_FILE_DIGITS 6
#define MYFS_MIN_NAME_LEN 11

struct myfs_disk_superblock {
    __le32 magic;
    __le32 version;
    __le64 generation;

    __le64 total_sectors;
    __le64 sb1_sector;
    __le64 sb2_sector;

    __le32 sector_size;
    __le32 file_size_sectors;
    __le32 max_filename_len;
    __le32 file_count;

    __le32 checksum;
    __le32 reserved;
} __packed;

struct myfs_sb_info {
    struct myfs_disk_superblock dsb;
    u64 total_sectors;
    u64 sb1_sector;
    u64 sb2_sector;
    u32 file_size_sectors;
    u32 max_filename_len;
    u32 file_count;
    bool erased;
    struct mutex lock;
};

extern char *disk_name;
extern unsigned long sb1_sector;
extern unsigned long sb2_sector;
extern unsigned int max_filename_len;
extern unsigned int file_size_sectors;

extern const struct super_operations myfs_super_ops;
extern const struct inode_operations myfs_dir_inode_ops;
extern const struct file_operations myfs_dir_ops;
extern const struct inode_operations myfs_file_inode_ops;
extern const struct file_operations myfs_file_ops;

int myfs_load_or_format_super(struct super_block *sb, struct myfs_sb_info *sbi);
int myfs_write_super_copies(struct super_block *sb, struct myfs_sb_info *sbi);
int myfs_zero_all_files(struct super_block *sb);
int myfs_erase_fs(struct super_block *sb);

struct inode *myfs_iget(struct super_block *sb, unsigned long ino);
bool myfs_is_file_ino(const struct myfs_sb_info *sbi, unsigned long ino);
int myfs_make_filename(u32 file_id, char *buf, size_t size);
int myfs_parse_filename(const struct myfs_sb_info *sbi, const char *name, size_t len, u32 *file_id);
u64 myfs_data_index_to_sector(const struct myfs_sb_info *sbi, u64 data_index);
u32 myfs_file_crc32(struct super_block *sb, u32 file_id, int *errp);
long myfs_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

static inline struct myfs_sb_info *MYFS_SB(struct super_block *sb)
{
    return (struct myfs_sb_info *)sb->s_fs_info;
}

static inline u64 myfs_file_size_bytes(const struct myfs_sb_info *sbi)
{
    return (u64)sbi->file_size_sectors * MYFS_SECTOR_SIZE;
}

#endif
