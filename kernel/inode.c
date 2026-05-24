#include <linux/fs.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "myfs.h"

bool myfs_is_file_ino(const struct myfs_sb_info *sbi, unsigned long ino)
{
    unsigned long first = MYFS_FIRST_FILE_INO;
    unsigned long last = first + sbi->file_count;

    return ino >= first && ino < last;
}

int myfs_make_filename(u32 file_id, char *buf, size_t size)
{
    int len;

    len = scnprintf(buf, size, MYFS_FILE_PREFIX "%06u", file_id);
    if (len <= 0 || len >= size)
        return -ENAMETOOLONG;
    return len;
}

int myfs_parse_filename(const struct myfs_sb_info *sbi,
                        const char *name,
                        size_t len,
                        u32 *file_id)
{
    char tmp[MYFS_MAX_IOCTL_NAME];
    const char *num;
    size_t prefix_len = strlen(MYFS_FILE_PREFIX);
    size_t i;
    int ret;

    if (len == 0 || len >= sizeof(tmp))
        return -EINVAL;
    if (len > sbi->max_filename_len)
        return -ENAMETOOLONG;
    if (len <= prefix_len)
        return -EINVAL;
    if (memcmp(name, MYFS_FILE_PREFIX, prefix_len) != 0)
        return -EINVAL;

    for (i = prefix_len; i < len; i++) {
        if (!isdigit(name[i]))
            return -EINVAL;
    }

    memcpy(tmp, name, len);
    tmp[len] = '\0';
    num = tmp + prefix_len;

    ret = kstrtou32(num, 10, file_id);
    if (ret)
        return ret;

    if (*file_id >= sbi->file_count)
        return -ENOENT;

    return 0;
}

u64 myfs_data_index_to_sector(const struct myfs_sb_info *sbi, u64 data_index)
{
    u64 a = min(sbi->sb1_sector, sbi->sb2_sector);
    u64 b = max(sbi->sb1_sector, sbi->sb2_sector);
    u64 phys = data_index;

    if (phys >= a)
        phys++;
    if (phys >= b)
        phys++;

    return phys;
}

static void myfs_set_times(struct inode *inode)
{
    struct timespec64 now = current_time(inode);

    inode_set_atime_to_ts(inode, now);
    inode_set_mtime_to_ts(inode, now);
    inode_set_ctime_to_ts(inode, now);
}

struct inode *myfs_iget(struct super_block *sb, unsigned long ino)
{
    struct myfs_sb_info *sbi = MYFS_SB(sb);
    struct inode *inode;

    inode = iget_locked(sb, ino);
    if (!inode)
        return ERR_PTR(-ENOMEM);

    if (!(inode->i_state & I_NEW))
        return inode;

    inode->i_uid = current_fsuid();
    inode->i_gid = current_fsgid();
    inode->i_blocks = 0;
    myfs_set_times(inode);

    if (ino == MYFS_ROOT_INO) {
        inode->i_mode = S_IFDIR | 0755;
        set_nlink(inode, 2);
        inode->i_op = &myfs_dir_inode_ops;
        inode->i_fop = &myfs_dir_ops;
        i_size_write(inode, sbi->file_count);
    } else if (myfs_is_file_ino(sbi, ino)) {
        inode->i_mode = S_IFREG | 0644;
        set_nlink(inode, 1);
        inode->i_op = &myfs_file_inode_ops;
        inode->i_fop = &myfs_file_ops;
        i_size_write(inode, myfs_file_size_bytes(sbi));
        inode->i_blocks = sbi->file_size_sectors;
    } else {
        iget_failed(inode);
        return ERR_PTR(-ENOENT);
    }

    unlock_new_inode(inode);
    return inode;
}

const struct inode_operations myfs_file_inode_ops = {
};
