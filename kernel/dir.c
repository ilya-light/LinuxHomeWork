#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "myfs.h"

static struct dentry *myfs_lookup(struct inode *dir,
                                  struct dentry *dentry,
                                  unsigned int flags)
{
    struct super_block *sb = dir->i_sb;
    struct myfs_sb_info *sbi = MYFS_SB(sb);
    struct inode *inode;
    u32 file_id;
    int ret;

    if (sbi->erased)
        return ERR_PTR(-ENODEV);

    ret = myfs_parse_filename(sbi,
                              dentry->d_name.name,
                              dentry->d_name.len,
                              &file_id);
    if (ret) {
        d_add(dentry, NULL);
        return NULL;
    }

    inode = myfs_iget(sb, MYFS_FIRST_FILE_INO + file_id);
    if (IS_ERR(inode))
        return ERR_CAST(inode);

    d_add(dentry, inode);
    return NULL;
}

static int myfs_iterate(struct file *file, struct dir_context *ctx)
{
    struct inode *inode = file_inode(file);
    struct super_block *sb = inode->i_sb;
    struct myfs_sb_info *sbi = MYFS_SB(sb);
    u32 file_id;
    char name[MYFS_MAX_IOCTL_NAME];
    int len;

    if (sbi->erased)
        return -ENODEV;

    if (!dir_emit_dots(file, ctx))
        return 0;

    file_id = ctx->pos - 2;
    for (; file_id < sbi->file_count; file_id++) {
        len = myfs_make_filename(file_id, name, sizeof(name));
        if (len < 0)
            return len;
        if ((u32)len > sbi->max_filename_len)
            return -ENAMETOOLONG;

        if (!dir_emit(ctx, name, len,
                      MYFS_FIRST_FILE_INO + file_id,
                      DT_REG))
            return 0;
        ctx->pos++;
    }

    return 0;
}

const struct inode_operations myfs_dir_inode_ops = {
    .lookup = myfs_lookup,
};

const struct file_operations myfs_dir_ops = {
    .owner = THIS_MODULE,
    .iterate_shared = myfs_iterate,
    .unlocked_ioctl = myfs_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = myfs_ioctl,
#endif
};
