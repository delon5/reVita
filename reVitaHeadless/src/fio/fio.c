#include <vitasdkkern.h>
#include <taihen.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "../log.h"

#define TRANSFER_SIZE 		 (32 * 1024)	// one buffer per copied tree, page-exact
#define ERROR_SHORT_WRITE	   0x90010003
#define MAX_PATH_SIZE 		 		(512)
#define ERROR_BLACKLISTED	   0x90010001
#define ERROR_SUBFOLDER	       0x90010002
#define SCE_ERROR_ERRNO_ENOENT 0x80010002
#define SCE_ERROR_ERRNO_EEXIST 0x80010011
#define SCE_ERROR_ERRNO_ENODEV 0x80010013

char *save_blacklist[] = {
    "sce_pfs/",
    "sce_sys/safemem.dat",
    "sce_sys/keystone",
    "sce_sys/sealedkey",
    NULL,
};

bool isBlacklisted(char* path){
    int i = 0;
    while (save_blacklist[i]) {
        if (strstr(path, save_blacklist[i])) {
			LOG(" Blacklisted: '%s'\n", path);
            return true;
        }
        i += 1;
    }
	return false;
}

bool fio_exist(const char *path) {
    SceIoStat stat = {0};
    int ret = ksceIoGetstat(path, &stat);
    return ret != SCE_ERROR_ERRNO_ENOENT && ret != SCE_ERROR_ERRNO_ENODEV;
}

// Reads at most size - 1 bytes and always leaves buff NUL-terminated.
bool fio_readFile(char* buff, int size, char* path, char* name, char* ext){
	if (size < 1)
		return false;
	char fname[128];
	snprintf(fname, sizeof(fname), "%s/%s.%s", path, name, ext);
	SceUID fd = ksceIoOpen(fname, SCE_O_RDONLY, 0777);
	if (fd < 0)
		return false;
	int read = ksceIoRead(fd, buff, size - 1);
	ksceIoClose(fd);
	if (read < 0){
		buff[0] = '\0';
		return false;
	}
	buff[read] = '\0';
	return true;
}
bool fio_writeFile(char* buff, int size, char* path, char* name, char* ext){
	//Create dir if not exists
	ksceIoMkdir(path, 0777); 

    char fname[128];
	snprintf(fname, sizeof(fname), "%s/%s.%s", path, name, ext);
	SceUID fd = ksceIoOpen(fname, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
	if (fd < 0)
		return false;
	int written = ksceIoWrite(fd, buff, size);
	int closed = ksceIoClose(fd);
	return written == size && closed >= 0;
}
bool fio_deleteFile(char* path, char* name, char* ext){
	char fname[128];
	snprintf(fname, sizeof(fname), "%s/%s.%s", path, name, ext);
	if (ksceIoRemove(fname) >= 0)
		return true;
	return false;
}
int fio_delete(char* path){
	return ksceIoRemove(path);
}

// Copies one file through the caller-provided transfer buffer.
static int copyFile(char *src, char *dest, char *buff, int buffSize){
	int ret = 0;

	// Check if blacklisted
    if (strcmp(src, dest) == 0 
			|| isBlacklisted(src) 
			|| isBlacklisted(dest))
		return ERROR_BLACKLISTED;

	// Open both files
	SceUID fdsrc = ksceIoOpen(src, SCE_O_RDONLY, 0);
	if (fdsrc < 0)
		return fdsrc;

	SceUID fddst = ksceIoOpen(dest, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
	if (fddst < 0){
		ksceIoClose(fdsrc);
		return fddst;
	}

	// Copy file using buffer
	while (true){
		int read = ksceIoRead(fdsrc, buff, buffSize);
		if (read < 0){
			ret = read;
			break;
		}
		if (read == 0)
			break;

		int written = ksceIoWrite(fddst, buff, read);
		if (written < 0){
			ret = written;
			break;
		}
		if (written != read){
			ret = ERROR_SHORT_WRITE;
			break;
		}
	}

	// Inherit file stat
	if (ret >= 0){
		SceIoStat stat;
		memset(&stat, 0, sizeof(SceIoStat));
		ksceIoGetstatByFd(fdsrc, &stat);
		ksceIoChstatByFd(fddst, &stat, 0x3B);
	}

	// Close / Clean IO
	ksceIoClose(fddst);
	ksceIoClose(fdsrc);
	if (ret < 0)
		ksceIoRemove(dest);

    return ret;
}

static int copyDir(char *src, char *dest, char *buff, int buffSize) {
    if (strcmp(src, dest) == 0 
			|| isBlacklisted(src) 
			|| isBlacklisted(dest))
		return 0;
    SceUID dfd = ksceIoDopen(src);
	if (dfd < 0)
		return dfd;

    if (!fio_exist(dest)) {
        int ret = ksceIoMkdir(dest, 0777);
        if (ret < 0 && ret != SCE_ERROR_ERRNO_EEXIST) {
            ksceIoDclose(dfd);
            return ret;
        }
    }

    int res = 0;

    do {
        SceIoDirent dir;
        memset(&dir, 0, sizeof(SceIoDirent));

        res = ksceIoDread(dfd, &dir);
        if (res > 0) {
            if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0)
                continue;

            char new_src[strlen(src) + strlen(dir.d_name) + 2];
            snprintf(new_src, sizeof(new_src), "%s/%s", src, dir.d_name);

            char new_dest[strlen(dest) + strlen(dir.d_name) + 2];
            snprintf(new_dest, sizeof(new_dest), "%s/%s", dest, dir.d_name);

            int ret = 0;

            if (SCE_S_ISDIR(dir.d_stat.st_mode)) {
                ret = copyDir(new_src, new_dest, buff, buffSize);
            } else {
                ret = copyFile(new_src, new_dest, buff, buffSize);
            }

            if (ret < 0 && ret != ERROR_BLACKLISTED) {
                ksceIoDclose(dfd);
                return ret;
            }
        }
    } while (res > 0);

    ksceIoDclose(dfd);
    return 0;
}

// Allocates the transfer buffer once, runs fn, frees it.
static int withTransferBuffer(char *src, char *dest, int (*fn)(char*, char*, char*, int)){
	char* buff = NULL;
	SceUID buff_uid = ksceKernelAllocMemBlock("reVita_filecopy", 
		SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_RW, TRANSFER_SIZE, NULL);
	if (buff_uid < 0)
		return buff_uid;
	if (ksceKernelGetMemBlockBase(buff_uid, (void**)&buff) != 0 || buff == NULL){
		ksceKernelFreeMemBlock(buff_uid);
		return -1;
	}
	int ret = fn(src, dest, buff, TRANSFER_SIZE);
	ksceKernelFreeMemBlock(buff_uid);
	return ret;
}

int fio_copyFile(char *src, char *dest){
	return withTransferBuffer(src, dest, copyFile);
}

int fio_copyDir(char *src, char *dest) {
	return withTransferBuffer(src, dest, copyDir);
}

int fio_deletePath(const char *path){
	SceUID dfd = ksceIoDopen(path);
	if (dfd >= 0) {
		int res = 0;

		char new_path[MAX_PATH_SIZE];
		do {
			SceIoDirent dir;
			memset(&dir, 0, sizeof(SceIoDirent));

			res = ksceIoDread(dfd, &dir);
			if (res > 0) {
				new_path[0] = '\0';
				snprintf(new_path, MAX_PATH_SIZE, "%s%s%s", path, (path[strlen(path) - 1] == '/') ? "" : "/", dir.d_name);

				if (SCE_S_ISDIR(dir.d_stat.st_mode)) {
					int ret = fio_deletePath(new_path);
					if (ret <= 0) {
						ksceIoDclose(dfd);
						return ret;
					}
				} else {
					int ret = ksceIoRemove(new_path);
					if (ret < 0) {
						ksceIoDclose(dfd);
						return ret;
					}
				}
			}
		} while (res > 0);

		ksceIoDclose(dfd);

		int ret = ksceIoRmdir(path);
		if (ret < 0)
			return ret;
	} else {
		int ret = ksceIoRemove(path);
		if (ret < 0)
			return ret;
	}

	return 1;
}