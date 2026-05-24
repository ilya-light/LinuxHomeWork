#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "myfs_ioctl.h"

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s test <mountpoint>\n"
            "  %s zero <mountpoint>\n"
            "  %s erase <mountpoint>\n"
            "  %s hashes <mountpoint>\n"
            "  %s mapping <mountpoint> <filename>\n",
            argv0, argv0, argv0, argv0, argv0);
}

static int open_mount_dir(const char *mountpoint)
{
    int fd = open(mountpoint, O_RDONLY | O_DIRECTORY);
    if (fd < 0)
        perror("open mountpoint");
    return fd;
}

static uint64_t random_u64(void)
{
    uint64_t value = 0;
    ssize_t n = getrandom(&value, sizeof(value), 0);

    if (n == (ssize_t)sizeof(value))
        return value;

    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        n = read(fd, &value, sizeof(value));
        close(fd);
        if (n == (ssize_t)sizeof(value))
            return value;
    }

    value = ((uint64_t)rand() << 32) ^ (uint64_t)rand();
    return value;
}

static int has_file_prefix(const char *name)
{
    return strncmp(name, "file_", 5) == 0;
}

static int cmd_test(const char *mountpoint)
{
    DIR *dir;
    struct dirent *de;
    unsigned long checked = 0;
    int failures = 0;

    dir = opendir(mountpoint);
    if (!dir) {
        perror("opendir");
        return 1;
    }

    while ((de = readdir(dir)) != NULL) {
        char path[4096];
        uint64_t expected;
        uint64_t actual = 0;
        ssize_t n;
        int fd;

        if (!has_file_prefix(de->d_name))
            continue;

        if (snprintf(path, sizeof(path), "%s/%s", mountpoint, de->d_name) >= (int)sizeof(path)) {
            fprintf(stderr, "path is too long: %s/%s\n", mountpoint, de->d_name);
            failures++;
            continue;
        }

        expected = random_u64();
        fd = open(path, O_RDWR);
        if (fd < 0) {
            perror(path);
            failures++;
            continue;
        }

        n = pwrite(fd, &expected, sizeof(expected), 0);
        if (n != (ssize_t)sizeof(expected)) {
            fprintf(stderr, "%s: pwrite failed: %s\n", path, strerror(errno));
            failures++;
            close(fd);
            continue;
        }

        n = pread(fd, &actual, sizeof(actual), 0);
        if (n != (ssize_t)sizeof(actual)) {
            fprintf(stderr, "%s: pread failed: %s\n", path, strerror(errno));
            failures++;
            close(fd);
            continue;
        }

        if (actual != expected) {
            fprintf(stderr,
                    "%s: mismatch: expected=%" PRIu64 ", actual=%" PRIu64 "\n",
                    path, expected, actual);
            failures++;
        }

        close(fd);
        checked++;
    }

    closedir(dir);

    printf("Checked files: %lu\n", checked);
    if (failures) {
        printf("Failures: %d\n", failures);
        return 1;
    }

    printf("OK\n");
    return 0;
}

static int cmd_zero(const char *mountpoint)
{
    int fd = open_mount_dir(mountpoint);
    int ret;

    if (fd < 0)
        return 1;

    ret = ioctl(fd, MYFS_IOC_ZERO_ALL);
    if (ret < 0) {
        perror("ioctl ZERO_ALL");
        close(fd);
        return 1;
    }

    close(fd);
    printf("All files were zeroed\n");
    return 0;
}

static int cmd_erase(const char *mountpoint)
{
    int fd = open_mount_dir(mountpoint);
    int ret;

    if (fd < 0)
        return 1;

    ret = ioctl(fd, MYFS_IOC_ERASE_FS);
    if (ret < 0) {
        perror("ioctl ERASE_FS");
        close(fd);
        return 1;
    }

    close(fd);
    printf("Filesystem was erased. Run umount before using this mountpoint again.\n");
    return 0;
}

static int cmd_hashes(const char *mountpoint)
{
    int fd = open_mount_dir(mountpoint);
    struct myfs_hashes_request req;
    struct myfs_file_hash *items = NULL;
    uint32_t i;
    int ret;

    if (fd < 0)
        return 1;

    memset(&req, 0, sizeof(req));
    ret = ioctl(fd, MYFS_IOC_GET_HASHES, &req);
    if (ret < 0 && errno != ENOSPC) {
        perror("ioctl GET_HASHES probe");
        close(fd);
        return 1;
    }

    if (req.count == 0) {
        printf("No files\n");
        close(fd);
        return 0;
    }

    items = calloc(req.count, sizeof(*items));
    if (!items) {
        perror("calloc");
        close(fd);
        return 1;
    }

    req.capacity = req.count;
    req.items_ptr = (uintptr_t)items;

    ret = ioctl(fd, MYFS_IOC_GET_HASHES, &req);
    if (ret < 0) {
        perror("ioctl GET_HASHES");
        free(items);
        close(fd);
        return 1;
    }

    for (i = 0; i < req.count; i++)
        printf("file_%06u crc32=0x%08x\n", items[i].file_id, items[i].crc32);

    free(items);
    close(fd);
    return 0;
}

static int cmd_mapping(const char *mountpoint, const char *filename)
{
    int fd = open_mount_dir(mountpoint);
    struct myfs_mapping_request req;
    uint64_t *sectors = NULL;
    uint32_t i;
    int ret;

    if (fd < 0)
        return 1;

    memset(&req, 0, sizeof(req));
    snprintf(req.filename, sizeof(req.filename), "%s", filename);

    ret = ioctl(fd, MYFS_IOC_GET_MAPPING, &req);
    if (ret < 0 && errno != ENOSPC) {
        perror("ioctl GET_MAPPING probe");
        close(fd);
        return 1;
    }

    if (req.sector_count == 0) {
        fprintf(stderr, "kernel returned empty mapping\n");
        close(fd);
        return 1;
    }

    sectors = calloc(req.sector_count, sizeof(*sectors));
    if (!sectors) {
        perror("calloc");
        close(fd);
        return 1;
    }

    req.capacity = req.sector_count;
    req.sectors_ptr = (uintptr_t)sectors;
    snprintf(req.filename, sizeof(req.filename), "%s", filename);

    ret = ioctl(fd, MYFS_IOC_GET_MAPPING, &req);
    if (ret < 0) {
        perror("ioctl GET_MAPPING");
        free(sectors);
        close(fd);
        return 1;
    }

    printf("%s: file_id=%u sector_count=%u\n",
           filename, req.file_id, req.sector_count);
    for (i = 0; i < req.sector_count; i++)
        printf("  [%u] sector=%" PRIu64 "\n", i, sectors[i]);

    free(sectors);
    close(fd);
    return 0;
}

int main(int argc, char **argv)
{
    const char *cmd;

    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    cmd = argv[1];

    if (strcmp(cmd, "test") == 0)
        return cmd_test(argv[2]);
    if (strcmp(cmd, "zero") == 0)
        return cmd_zero(argv[2]);
    if (strcmp(cmd, "erase") == 0)
        return cmd_erase(argv[2]);
    if (strcmp(cmd, "hashes") == 0)
        return cmd_hashes(argv[2]);
    if (strcmp(cmd, "mapping") == 0) {
        if (argc != 4) {
            usage(argv[0]);
            return 1;
        }
        return cmd_mapping(argv[2], argv[3]);
    }

    usage(argv[0]);
    return 1;
}
