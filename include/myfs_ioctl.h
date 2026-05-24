#ifndef MYFS_IOCTL_H
#define MYFS_IOCTL_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/ioctl.h>
#else
#include <linux/types.h>
#include <sys/ioctl.h>
#endif

#define MYFS_IOCTL_MAGIC 'M'
#define MYFS_MAX_IOCTL_NAME 256

struct myfs_file_hash {
    __u32 file_id;
    __u32 crc32;
};


struct myfs_hashes_request {
    __u32 capacity;
    __u32 count;
    __u64 items_ptr;
};

struct myfs_mapping_request {
    char filename[MYFS_MAX_IOCTL_NAME];
    __u32 file_id;
    __u32 sector_count;
    __u32 capacity;
    __u32 reserved;
    __u64 sectors_ptr;
};

#define MYFS_IOC_ZERO_ALL    _IO(MYFS_IOCTL_MAGIC, 1)
#define MYFS_IOC_ERASE_FS    _IO(MYFS_IOCTL_MAGIC, 2)
#define MYFS_IOC_GET_HASHES  _IOWR(MYFS_IOCTL_MAGIC, 3, struct myfs_hashes_request)
#define MYFS_IOC_GET_MAPPING _IOWR(MYFS_IOCTL_MAGIC, 4, struct myfs_mapping_request)

#endif
