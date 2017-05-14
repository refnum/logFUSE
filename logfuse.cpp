/*	NAME:
		logfuse.cpp

	DESCRIPTION:
		Logging/passthrough filesystem for FUSE.

	COPYRIGHT:
		Copyright (c) 2017, refNum Software
		<http://www.refnum.com/>

		All rights reserved. Released under the terms of LICENSE.md.
	__________________________________________________________________________
*/
//============================================================================
//		Include files
//----------------------------------------------------------------------------
#include <string>

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/attr.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/xattr.h>

#include <fuse.h>

#if FUSE_APPLE
	#include <os/log.h>
#endif





//============================================================================
//		Internal constants
//----------------------------------------------------------------------------
enum {
	kMaxLogMsg														= 10 * 1024
};





//============================================================================
//		Internal macros
//----------------------------------------------------------------------------
// Extended attributes
#if FUSE_APPLE
	#define XATTR_POSITION											, uint32_t position
#else
	#define XATTR_POSITION
#endif


// Errors
#define RETURN_FUSE_ERRNO()											\
	do																\
		{															\
		if (sysErr == -1)											\
			return(-errno);											\
																	\
		return(sysErr);												\
		}															\
	while (0)


// Bitfield text
#define TEXT_BEGIN()												\
			std::string		theText;


#define TEXT_BIT(_flag, _bit)										\
			do														\
				{													\
				if ( ((_flag) & (_bit)) == (_bit) )					\
					{												\
					if (theText.empty())							\
						theText  = #_bit;							\
					else											\
						theText += #_bit " | ";						\
					}												\
				}													\
			while(0)

#define TEXT_END(_flag)												\
			do														\
				{													\
				if (theText.empty())								\
					theText = std::to_string(_flag);				\
																	\
				return(theText);									\
				}													\
			while(0)





//============================================================================
//		Internal types
//----------------------------------------------------------------------------
// Directory info
struct logfuse_dir_info {
	DIR				*dir;
    dirent			*entry;
	off_t			offset;
};





//============================================================================
//		Internal functions
//----------------------------------------------------------------------------
//		logfuse_log : Emit a log message.
//----------------------------------------------------------------------------
__attribute__((__format__ (__printf__, 1, 2)))
static void logfuse_log(const char *formatMsg, ...)
{	char		theBuffer[kMaxLogMsg];
	va_list		argList;



	// Format the message
	va_start(argList, formatMsg);
	vsprintf(theBuffer, formatMsg, argList);
	va_end(argList);



	// Emit the log
#if FUSE_APPLE
	os_log(OS_LOG_DEFAULT, "%{public}s", theBuffer);
#else
	syslog(LOG_INFO, "%s", theBuffer);
#endif
}





//============================================================================
//		logfuse_get_dir : Get the directory info.
//----------------------------------------------------------------------------
static logfuse_dir_info *logfuse_get_dir(fuse_file_info *fileInfo)
{


	// Validate our state
	static_assert(sizeof(logfuse_dir_info *) <= sizeof(fileInfo->fh), "Unable to store pointer");



	// Get the directory
	return((logfuse_dir_info *) ((uintptr_t) fileInfo->fh));
}





//============================================================================
//		logfuse_fset_timespec : Set a file time.
//----------------------------------------------------------------------------
static int logfuse_fset_timespec(int fd, attrgroup_t theAttribute, timespec theTime)
{	attrlist		attributeInfo;
	int				sysErr;



	// Get the state we need
	memset(&attributeInfo, 0x00, sizeof(attributeInfo));

	attributeInfo.bitmapcount = ATTR_BIT_MAP_COUNT;
	attributeInfo.commonattr  = theAttribute;



	// Set the time
	sysErr = fsetattrlist(fd, &attributeInfo, &theTime, sizeof(timespec), FSOPT_NOFOLLOW);

	return(sysErr);
}





//============================================================================
//		logfuse_set_timespec : Set a file time.
//----------------------------------------------------------------------------
static int logfuse_set_timespec(const char *path, attrgroup_t theAttribute, timespec theTime)
{	attrlist		attributeInfo;
	int				sysErr;



	// Get the state we need
	memset(&attributeInfo, 0x00, sizeof(attributeInfo));

	attributeInfo.bitmapcount = ATTR_BIT_MAP_COUNT;
	attributeInfo.commonattr  = theAttribute;



	// Set the time
	sysErr = setattrlist(path, &attributeInfo, &theTime, sizeof(timespec), FSOPT_NOFOLLOW);

	return(sysErr);
}





//============================================================================
//		logfuse_str_access_mode : Access mode string.
//----------------------------------------------------------------------------
static std::string logfuse_str_access_mode(int mode)
{


	// Get the text
	TEXT_BEGIN();
		TEXT_BIT(mode, F_OK);
		TEXT_BIT(mode, R_OK);
		TEXT_BIT(mode, W_OK);
		TEXT_BIT(mode, X_OK);
	TEXT_END(mode);
}





//============================================================================
//		logfuse_str_open_flags : open flags string.
//----------------------------------------------------------------------------
static std::string logfuse_str_open_flags(int flags)
{


	// Get the text
	TEXT_BEGIN();
		TEXT_BIT(flags, O_RDONLY);
		TEXT_BIT(flags, O_WRONLY);
		TEXT_BIT(flags, O_RDWR);
		TEXT_BIT(flags, O_NONBLOCK);
		TEXT_BIT(flags, O_APPEND);
		TEXT_BIT(flags, O_SHLOCK);
		TEXT_BIT(flags, O_EXLOCK);
		TEXT_BIT(flags, O_NOFOLLOW);
		TEXT_BIT(flags, O_CREAT);
		TEXT_BIT(flags, O_TRUNC);
		TEXT_BIT(flags, O_EXCL);
		TEXT_BIT(flags, O_EVTONLY);
		TEXT_BIT(flags, O_SYMLINK);
		TEXT_BIT(flags, O_CLOEXEC);
	TEXT_END(flags);
}





//============================================================================
//		logfuse_str_fcntl_cmd : fcntl command string.
//----------------------------------------------------------------------------
static const char *logfuse_str_fcntl_cmd(int cmd)
{


	// Get the text
	switch (cmd) {
		case F_DUPFD:						return("F_DUPFD");						break;
		case F_GETFD:						return("F_GETFD");						break;
		case F_SETFD:						return("F_SETFD");						break;
		case F_GETFL:						return("F_GETFL");						break;
		case F_SETFL:						return("F_SETFL");						break;
		case F_GETOWN:						return("F_GETOWN");						break;
		case F_SETOWN:						return("F_SETOWN");						break;
		case F_GETLK:						return("F_GETLK");						break;
		case F_SETLK:						return("F_SETLK");						break;
		case F_SETLKW:						return("F_SETLKW");						break;
		case F_SETLKWTIMEOUT:				return("F_SETLKWTIMEOUT");				break;
		case F_FLUSH_DATA:					return("F_FLUSH_DATA");					break;
		case F_PREALLOCATE:					return("F_PREALLOCATE");				break;
		case F_SETSIZE:						return("F_SETSIZE");					break;
		case F_RDADVISE:					return("F_RDADVISE");					break;
		case F_RDAHEAD:						return("F_RDAHEAD");					break;
		case F_NOCACHE:						return("F_NOCACHE");					break;
		case F_LOG2PHYS:					return("F_LOG2PHYS");					break;
		case F_GETPATH:						return("F_GETPATH");					break;
		case F_FULLFSYNC:					return("F_FULLFSYNC");					break;
		case F_PATHPKG_CHECK:				return("F_PATHPKG_CHECK");				break;
		case F_FREEZE_FS:					return("F_FREEZE_FS");					break;
		case F_THAW_FS:						return("F_THAW_FS");					break;
		case F_GLOBAL_NOCACHE:				return("F_GLOBAL_NOCACHE");				break;
		case F_ADDSIGS:						return("F_ADDSIGS");					break;
		case F_ADDFILESIGS:					return("F_ADDFILESIGS");				break;
		case F_NODIRECT:					return("F_NODIRECT");					break;
		case F_GETPROTECTIONCLASS:			return("F_GETPROTECTIONCLASS");			break;
		case F_SETPROTECTIONCLASS:			return("F_SETPROTECTIONCLASS");			break;
		case F_LOG2PHYS_EXT:				return("F_LOG2PHYS_EXT");				break;
		case F_GETLKPID:					return("F_GETLKPID");					break;
		case F_SETBACKINGSTORE:				return("F_SETBACKINGSTORE");			break;
		case F_GETPATH_MTMINFO:				return("F_GETPATH_MTMINFO");			break;
		case F_GETCODEDIR:					return("F_GETCODEDIR");					break;
		case F_SETNOSIGPIPE:				return("F_SETNOSIGPIPE");				break;
		case F_GETNOSIGPIPE:				return("F_GETNOSIGPIPE");				break;
		case F_TRANSCODEKEY:				return("F_TRANSCODEKEY");				break;
		case F_SINGLE_WRITER:				return("F_SINGLE_WRITER");				break;
		case F_GETPROTECTIONLEVEL:			return("F_GETPROTECTIONLEVEL");			break;
		case F_FINDSIGS:					return("F_FINDSIGS");					break;
		case F_ADDFILESIGS_FOR_DYLD_SIM:	return("F_ADDFILESIGS_FOR_DYLD_SIM");	break;
		case F_BARRIERFSYNC:				return("F_BARRIERFSYNC");				break;
		case F_ADDFILESIGS_RETURN:			return("F_ADDFILESIGS_RETURN");			break;
		case F_CHECK_LV:					return("F_CHECK_LV");					break;
		case F_PUNCHHOLE:					return("F_PUNCHHOLE");					break;
		case F_TRIM_ACTIVE_FILE:			return("F_TRIM_ACTIVE_FILE");			break;
		default:
			break;
		}

	return("UNKNOWN");
}





#pragma mark FUSE
//============================================================================
//		FUSE methods.
//----------------------------------------------------------------------------
//		logfuse_getattr : Get file attributes.
//----------------------------------------------------------------------------
static int logfuse_getattr(const char *path, struct stat *statInfo)
{	int		sysErr;



	// Get the attributes
	//
	// Setting st_blksize to 0 ensures FUSE uses the global iosize option.
	sysErr               = lstat(path, statInfo);
	statInfo->st_blksize = 0;
	
	logfuse_log("logfuse_getattr(%s) err=%d", path, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_readlink : Read a symbol link.
//----------------------------------------------------------------------------
static int logfuse_readlink(const char *path, char *buffer, size_t size)
{	int		sysErr;



	// Read the link
	sysErr = readlink(path, buffer, size - 1);
	buffer[sysErr == -1 ? 0 : sysErr] = 0x00;

	logfuse_log("logfuse_readlink(%s, %s) err=%d", path, buffer, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_mknod : Create a file node.
//----------------------------------------------------------------------------
static int logfuse_mknod(const char *path, mode_t mode, dev_t rdev)
{	int		sysErr;



	// Create the node
	if (S_ISFIFO(mode))
		sysErr = mkfifo(path, mode);
	else
		sysErr = mknod( path, mode, rdev);

	logfuse_log("logfuse_mknod(%s, %d, %d) err=%d", path, mode, rdev, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_mkdir : Create a directory.
//----------------------------------------------------------------------------
static int logfuse_mkdir(const char *path, mode_t mode)
{	int		sysErr;



	// Create the directory
	sysErr = mkdir(path, mode);
	logfuse_log("logfuse_mkdir(%s, %d) err=%d", path, mode, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_unlink : Remove a file.
//----------------------------------------------------------------------------
static int logfuse_unlink(const char *path)
{	int		sysErr;



	// Remove the file
	sysErr = unlink(path);
	logfuse_log("logfuse_unlink(%s) err=%d", path, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_rmdir : Remove a directory.
//----------------------------------------------------------------------------
static int logfuse_rmdir(const char *path)
{	int		sysErr;



	// Remove the directory
	sysErr = rmdir(path);
	logfuse_log("logfuse_rmdir(%s) err=%d", path, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_symlink : Create a symbolic link.
//----------------------------------------------------------------------------
static int logfuse_symlink(const char *from, const char *to)
{	int		sysErr;



	// Create the link
	sysErr = symlink(from, to);
	logfuse_log("logfuse_symlink(%s, %s) err=%d", from, to, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_rename : Rename a file.
//----------------------------------------------------------------------------
static int logfuse_rename(const char *from, const char *to)
{	int		sysErr;



	// Rename the file
	sysErr = rename(from, to);
	logfuse_log("logfuse_rename(%s, %s) err=%d", from, to, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_link : Create a hard link.
//----------------------------------------------------------------------------
static int logfuse_link(const char *from, const char *to)
{	int		sysErr;



	// Create the link
	sysErr = link(from, to);
	logfuse_log("logfuse_link(%s, %s) err=%d", from, to, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_chmod : Change the permission bits.
//----------------------------------------------------------------------------
static int logfuse_chmod(const char *path, mode_t mode)
{	int		sysErr;



	// Change the permission
	sysErr = chmod(path, mode);
	logfuse_log("logfuse_chmod(%s, %d) err=%d", path, mode, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_chown : Change the owner and group of a file.
//----------------------------------------------------------------------------
static int logfuse_chown(const char *path, uid_t owner, gid_t group)
{	int		sysErr;



	// Change the owner/group
	sysErr = chown(path, owner, group);
	logfuse_log("logfuse_chown(%s, %d, %d) err=%d", path, owner, group, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_truncate : Change the size of a file.
//----------------------------------------------------------------------------
static int logfuse_truncate(const char *path, off_t length)
{	int		sysErr;



	// Change the size
	sysErr = truncate(path, length);
	logfuse_log("logfuse_truncate(%s, %lld) err=%d", path, length, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_open : Open a file.
//----------------------------------------------------------------------------
static int logfuse_open(const char *path, fuse_file_info *fileInfo)
{	int		fd;



	// Open the file
	fd = open(path, fileInfo->flags);
	logfuse_log("logfuse_open(%s, %s) fd=%d",
					path,
					logfuse_str_open_flags(fileInfo->flags).c_str(),
					fd);

	if (fd == -1)
		return(-errno);
	
	fileInfo->fh = fd;

	return(0);
}





//============================================================================
//		logfuse_read : Read from a file.
//----------------------------------------------------------------------------
static int logfuse_read(const char *path, char *buffer, size_t size, off_t offset, fuse_file_info *fileInfo)
{	int		sysErr;



	// Read the file
	sysErr = pread(fileInfo->fh, buffer, size, offset);
	logfuse_log("logfuse_read(%s, size=%ld, offset=%lld) %s=%d",
					path,
					size,
					offset,
					sysErr >= 0 ? "read" : "err",
					sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_write : Write to a file.
//----------------------------------------------------------------------------
static int logfuse_write(const char *path, const char *buffer, size_t size, off_t offset, fuse_file_info *fileInfo)
{	int		sysErr;



	// Write the file
	sysErr = pwrite(fileInfo->fh, buffer, size, offset);
	logfuse_log("logfuse_write(%s, size=%ld, offset=%lld) %s=%d",
					path,
					size,
					offset,
					sysErr >= 0 ? "wrote" : "err",
					sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_statfs : Get file statistics.
//----------------------------------------------------------------------------
static int logfuse_statfs(const char *path, struct statvfs *statInfo)
{	int		sysErr;



	// Get the info
	sysErr = statvfs(path, statInfo);
	logfuse_log("logfuse_statfs(%s) err=%d", path, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_flush : Flush cached data.
//----------------------------------------------------------------------------
static int logfuse_flush(const char *path, fuse_file_info *fileInfo)
{	int		sysErr;



	// Flush the file
	sysErr = close(dup(fileInfo->fh));
	logfuse_log("logfuse_flush(%s, fd=%lld) err=%d", path, fileInfo->fh, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_release : Release an open file.
//----------------------------------------------------------------------------
static int logfuse_release(const char *path, fuse_file_info *fileInfo)
{	int		sysErr;



	// Release the file
	sysErr = close(fileInfo->fh);
	logfuse_log("logfuse_close(%s) err=%d", path, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_fsync : Synchronize a file.
//----------------------------------------------------------------------------
static int logfuse_fsync(const char *path, int dataSync, fuse_file_info *fileInfo)
{	int		sysErr;



	// Sync the file
	sysErr = fsync(fileInfo->fh);
	logfuse_log("logfuse_fsync(%s, %d) err=%d", path, dataSync, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_setxattr : Set an extended attribute.
//----------------------------------------------------------------------------
static int logfuse_setxattr(const char *path, const char *name, const char *value, size_t size, int flags XATTR_POSITION)
{	int		sysErr;



	// Set the attribute
#if FUSE_APPLE
	sysErr =  setxattr(path, name, value, size, position, XATTR_NOFOLLOW);
#else
	sysErr = lsetxattr(path, name, value, size, flags);
#endif

	logfuse_log("logfuse_setxattr(%s, %s, %s) err=%d", path, name, value, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_getxattr : Get an extended attribute.
//----------------------------------------------------------------------------
static int logfuse_getxattr(const char *path, const char *name, char *value, size_t size XATTR_POSITION)
{	int		sysErr;



	// Get the attribute
#if FUSE_APPLE
	sysErr = getxattr(path, name, value, size, position, XATTR_NOFOLLOW);
#else
	sysErr = lgetxattr(path, name, value, size);
#endif

	logfuse_log("logfuse_getxattr(%s, %s) value='%s' err=%d", path, name, value, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_listxattr : List extended attributes.
//----------------------------------------------------------------------------
static int logfuse_listxattr(const char *path, char *list, size_t size)
{	ssize_t		sysErr;



	// List the attributes
	sysErr = listxattr(path, list, size, XATTR_NOFOLLOW);
	logfuse_log("logfuse_listxattr(%s, %s) err=%ld", path, list, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_removexattr : Remove an extended attribute.
//----------------------------------------------------------------------------
static int logfuse_removexattr(const char *path, const char *name)
{	int		sysErr;



	// Remove the attribute
	sysErr = removexattr(path, name, XATTR_NOFOLLOW);
	logfuse_log("logfuse_removexattr(%s, %s) err=%d", path, name, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_opendir : Open a directory.
//----------------------------------------------------------------------------
static int logfuse_opendir(const char *path, fuse_file_info *fileInfo)
{	logfuse_dir_info	*dirInfo;
	int					sysErr;
	DIR					*dir;



	// Open the directory
	dir    = opendir(path);
	sysErr = (dir != nullptr) ? 0 : errno;

	logfuse_log("logfuse_opendir(%s) err=%d", path, sysErr);

	if (sysErr != 0)
		return(-sysErr);



	// Create our info
	dirInfo = (logfuse_dir_info *) malloc(sizeof(logfuse_dir_info));
	if (dirInfo == nullptr)
		{
		closedir(dir);
		return(-ENOMEM);
		}

	dirInfo->dir    = dir;
	dirInfo->entry  = nullptr;
	dirInfo->offset = 0;

	fileInfo->fh = (uintptr_t) dirInfo;

	return(0);
}





//============================================================================
//		logfuse_readdir : Read a directory.
//----------------------------------------------------------------------------
static int logfuse_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, fuse_file_info *fileInfo)
{	logfuse_dir_info	*dirInfo = logfuse_get_dir(fileInfo);
	off_t				nextOffset;
	struct stat			statInfo;



	// Seek to the entry
	if (offset != dirInfo->offset)
		{
		seekdir(dirInfo->dir, offset);
		
		dirInfo->entry  = nullptr;
		dirInfo->offset = offset;
		}



	// Read the info
	while (true)
		{
		// Read the entry
		if (dirInfo->entry == nullptr)
			{
			dirInfo->entry = readdir(dirInfo->dir);
			if (dirInfo->entry == nullptr)
				break;
			}



		// Get the info
        memset(&statInfo, 0, sizeof(statInfo));
        statInfo.st_ino  = dirInfo->entry->d_ino;
        statInfo.st_mode = dirInfo->entry->d_type << 12;

        nextOffset = telldir(dirInfo->dir);

        if (filler(buffer, dirInfo->entry->d_name, &statInfo, nextOffset))
			{
			logfuse_log("logfuse_readdir(%s, %s) err=0", path, dirInfo->entry->d_name);
			break;
			}



		// Update our state
		dirInfo->entry  = nullptr;
        dirInfo->offset = nextOffset;
		}
	
	return(0);
}





//============================================================================
//		logfuse_releasedir : Release a directory.
//----------------------------------------------------------------------------
static int logfuse_releasedir(const char *path, fuse_file_info *fileInfo)
{	logfuse_dir_info	*dirInfo = logfuse_get_dir(fileInfo);



	// Release the directory
	logfuse_log("logfuse_releasedir(%s) err=0", path);

	closedir(dirInfo->dir);
	free(    dirInfo);

	return(0);
}





//============================================================================
//		logfuse_fsyncdir : Synchronise a directory.
//----------------------------------------------------------------------------
static int logfuse_fsyncdir(const char *path, int dataSync, fuse_file_info */*fileInfo*/)
{


	// Synchronise the directory
	logfuse_log("logfuse_fsyncdir(%s, %d) err=0", path, dataSync);
	
	return(0);
}





//============================================================================
//		logfuse_init : Initialise the filesystem.
//----------------------------------------------------------------------------
static void *logfuse_init(fuse_conn_info *fsConnection)
{


	// Initialise the filesystem
	logfuse_log("logfuse_init: protocol=%d.%d, max_write=%d, max_read=%d, caps=0x%0x",
						fsConnection->proto_major,
						fsConnection->proto_minor,
						fsConnection->max_write,
						fsConnection->max_readahead,
						fsConnection->capable);

	fsConnection->want |= FUSE_CAP_ASYNC_READ;
	fsConnection->want |= FUSE_CAP_POSIX_LOCKS;
	fsConnection->want |= FUSE_CAP_ATOMIC_O_TRUNC;
	fsConnection->want |= FUSE_CAP_BIG_WRITES;
	fsConnection->want |= FUSE_CAP_FLOCK_LOCKS;

#if FUSE_APPLE
	fsConnection->want |= FUSE_CAP_ALLOCATE;
	fsConnection->want |= FUSE_CAP_EXCHANGE_DATA;
	fsConnection->want |= FUSE_CAP_CASE_INSENSITIVE;
	fsConnection->want |= FUSE_CAP_VOL_RENAME;
	fsConnection->want |= FUSE_CAP_XTIMES;
#endif

	return(nullptr);
}





//============================================================================
//		logfuse_destroy : Destroy the filesystem.
//----------------------------------------------------------------------------
static void logfuse_destroy(void */*userData*/)
{


	// Destroy the filesyste,
	logfuse_log("logfuse_destroy");
}





//============================================================================
//		logfuse_access : Check file access permissions.
//----------------------------------------------------------------------------
static int logfuse_access(const char *path, int mode)
{	int		sysErr;



	// Check the permissions
	sysErr = access(path, mode);
	logfuse_log("logfuse_access(%s, %s) err=%d",
					path,
					logfuse_str_access_mode(mode).c_str(),
					sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_create : Create and open a file.
//----------------------------------------------------------------------------
static int logfuse_create(const char *path, mode_t mode, fuse_file_info *fileInfo)
{	int		fd;



	// Open the file
	fd = open(path, fileInfo->flags, mode);
	logfuse_log("logfuse_create(%s, 0x%0X, %d) fd=%d", path, mode, fileInfo->flags, fd);

	if (fd == -1)
		return(-errno);
	
	fileInfo->fh = fd;
	
	return(0);
}





//============================================================================
//		logfuse_ftruncate : Change the size of an open file.
//----------------------------------------------------------------------------
static int logfuse_ftruncate(const char *path, off_t length, fuse_file_info *fileInfo)
{	int		sysErr;



	// Change the size
	sysErr = ftruncate(fileInfo->fh, length);
	logfuse_log("logfuse_ftruncate(%s, %lld) err=%d", path, length, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_fgetattr : Get attributes from an open file.
//----------------------------------------------------------------------------
static int logfuse_fgetattr(const char *path, struct stat *statInfo, fuse_file_info *fileInfo)
{	int		sysErr;



	// Get the attributes
	//
	// Setting st_blksize to 0 ensures FUSE uses the global iosize option.
	sysErr               = fstat(fileInfo->fh, statInfo);
	statInfo->st_blksize = 0;

	logfuse_log("logfuse_fgetattr(%s) err=%d", path, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_lock : Perform POSIX file locking.
//----------------------------------------------------------------------------
static int logfuse_lock(const char *path, struct fuse_file_info *fileInfo, int cmd, struct flock *lockInfo)
{	int		sysErr;



	// Perform the lock
	sysErr = fcntl(fileInfo->fh, cmd, lockInfo);
	logfuse_log("logfuse_lock(%s, %s) err=%d",
					path,
					logfuse_str_fcntl_cmd(cmd),
					sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_utimens : Change the access+modification times of a file.
//----------------------------------------------------------------------------
static int logfuse_utimens(const char *path, const timespec timeSpec[2])
{	int		sysErr;



#if FUSE_APPLE
	attrlist				attributeInfo;

	struct __attribute__((packed)) {
		timespec		modTime;
		timespec		accessTime;
	} attributeData;



	// Get the state we need
	memset(&attributeInfo, 0x00, sizeof(attributeInfo));

	attributeInfo.bitmapcount = ATTR_BIT_MAP_COUNT;
    attributeInfo.commonattr  = ATTR_CMN_ACCTIME | ATTR_CMN_MODTIME;
	
	attributeData.modTime    = timeSpec[1];
	attributeData.accessTime = timeSpec[0];



	// Set the timestamps
	sysErr = setattrlist(path, &attributeInfo, &attributeData, sizeof(attributeData), FSOPT_NOFOLLOW);
#else
	sysErr = utimensat(0, path, timeSpec, AT_SYMLINK_NOFOLLOW);
#endif

	logfuse_log("logfuse_utimens(%s) err=%d", path, sysErr);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_ioctl : Invoke a device control command.
//----------------------------------------------------------------------------
static int logfuse_ioctl(const char *path, int cmd, void *arg, fuse_file_info *fileInfo, unsigned int flags, void *data)
{


	// Invoke the command
	logfuse_log("logfuse_ioctl(%s)", path);

	return(-ENOMEM);
}





//============================================================================
//		logfuse_poll : Poll for IO readiness events.
//----------------------------------------------------------------------------
static int logfuse_poll(const char *path, fuse_file_info *fileInfo, fuse_pollhandle *pollHnd, unsigned *reventsp)
{


	// Poll for IO
	logfuse_log("logfuse_poll(%s)", path);

	return(-ENOMEM);
}





//============================================================================
//		logfuse_flock : Perform BSD file locking.
//----------------------------------------------------------------------------
static int logfuse_flock(const char *path, fuse_file_info *fileInfo, int lockOp)
{	int		sysErr;


	// Perform the lock
	sysErr = flock(fileInfo->fh, lockOp);
	logfuse_log("logfuse_flock(%s, %d)", path, lockOp);

	RETURN_FUSE_ERRNO();
}





//============================================================================
//		logfuse_fallocate : Allocate space for a file.
//----------------------------------------------------------------------------
static int logfuse_fallocate(const char *path, int mode, off_t offset, off_t length, fuse_file_info *fileInfo)
{	fstore_t		theInfo;
	int				sysErr;



	// Check our parameters
    if ((mode & PREALLOCATE) == 0)
		return(-ENOTSUP);



	// Get the state we need
	memset(&theInfo, 0x00, sizeof(theInfo));

	theInfo.fst_offset = offset;
	theInfo.fst_length = length;

	if ((mode & ALLOCATECONTIG) != 0)
		theInfo.fst_flags |= F_ALLOCATECONTIG;

	if ((mode & ALLOCATEALL) != 0)
		theInfo.fst_flags |= F_ALLOCATEALL;

	if ((mode & ALLOCATEFROMPEOF) != 0)
		theInfo.fst_posmode = F_PEOFPOSMODE;

	if ((mode & ALLOCATEFROMVOL) != 0)
		theInfo.fst_posmode = F_VOLPOSMODE;



	// Allocate the space
	sysErr = fcntl(fileInfo->fh, F_PREALLOCATE, &theInfo);
	logfuse_log("logfuse_fallocate(%s, %d, %lld, %lld) err=%d", path, mode, offset, length, sysErr);

	RETURN_FUSE_ERRNO();
}





#if FUSE_APPLE
//============================================================================
//		logfuse_setvolname : Set the volume name.
//----------------------------------------------------------------------------
static int logfuse_setvolname(const char *name)
{	attrlist		attributeInfo;
	int				sysErr;

	struct __attribute__((packed)) {
		attrreference	info;
		char			name[NAME_MAX + 1];
	} attributeData;



	// Get the state we need
	memset(&attributeInfo, 0x00, sizeof(attributeInfo));
	memset(&attributeData, 0x00, sizeof(attributeData));

	attributeInfo.bitmapcount = ATTR_BIT_MAP_COUNT;
	attributeInfo.volattr  = ATTR_VOL_INFO | ATTR_VOL_NAME;
	
	attributeData.info.attr_dataoffset = sizeof(attrreference);
	attributeData.info.attr_length     = strlen(name)+1;
	
	strncpy(attributeData.name, name, NAME_MAX);



	// Set the name
	//
	// setattrlist requires the path to the volume mount point to set the name.
	if (false)
		sysErr = setattrlist(nullptr, &attributeInfo, &attributeData, sizeof(attributeData), FSOPT_NOFOLLOW);

	logfuse_log("logfuse_setvolname(%s)", name);

	return(-EACCES);
}
#endif // FUSE_APPLE





#if FUSE_APPLE
//============================================================================
//		logfuse_exchange : Exchange two files.
//----------------------------------------------------------------------------
static int logfuse_exchange(const char *path1, const char *path2, unsigned long options)
{	int	sysErr;



	// Exchange the files
	sysErr = exchangedata(path1, path2, options);
	logfuse_log("logfuse_exchange(%s, %s, %ld) err=%d", path1, path2, options, sysErr);

	RETURN_FUSE_ERRNO();
}
#endif // FUSE_APPLE





#if FUSE_APPLE
//============================================================================
//		logfuse_getxtimes : Get extended time info.
//----------------------------------------------------------------------------
static int logfuse_getxtimes(const char *path, timespec *backupTime, timespec *createTime)
{	attrlist				attributeInfo;
	int						sysErr;

	struct __attribute__((packed)) {
		uint32_t		size;
		timespec		createTime;
		timespec		backupTime;
	} attributeData;



	// Get the state we need
	memset(&attributeInfo, 0x00, sizeof(attributeInfo));

	attributeInfo.bitmapcount = ATTR_BIT_MAP_COUNT;
    attributeInfo.commonattr  = ATTR_CMN_CRTIME | ATTR_CMN_BKUPTIME;



	// Get the attributes
	sysErr = getattrlist(path, &attributeInfo, &attributeData, sizeof(attributeData), FSOPT_NOFOLLOW);
	if (sysErr == 0)
		{
		*backupTime = attributeData.backupTime;
		*createTime = attributeData.createTime;
		}
	else
		{
		memset(backupTime, 0x00, sizeof(timespec));
		memset(createTime, 0x00, sizeof(timespec));
		}

	logfuse_log("logfuse_getxtimes(%s) err=%d", path, sysErr);

	RETURN_FUSE_ERRNO();
}
#endif // FUSE_APPLE





#if FUSE_APPLE
//============================================================================
//		logfuse_setbkuptime : Set the backup time.
//----------------------------------------------------------------------------
static int logfuse_setbkuptime(const char *path, const timespec *theTime)
{	int		sysErr;



	// Set the time
	sysErr = logfuse_set_timespec(path, ATTR_CMN_BKUPTIME, *theTime);
	logfuse_log("logfuse_setbkuptime(%s) err=%d", path, sysErr);

	RETURN_FUSE_ERRNO();
}
#endif // FUSE_APPLE





#if FUSE_APPLE
//============================================================================
//		logfuse_setchgtime : Set the attribute change time.
//----------------------------------------------------------------------------
static int logfuse_setchgtime(const char *path, const timespec *theTime)
{	int		sysErr;



	// Set the time
	sysErr = logfuse_set_timespec(path, ATTR_CMN_CHGTIME, *theTime);
	logfuse_log("logfuse_setchgtime(%s) err=%d", path, sysErr);

	RETURN_FUSE_ERRNO();
}
#endif // FUSE_APPLE





#if FUSE_APPLE
//============================================================================
//		logfuse_setcrtime : Set the creation time.
//----------------------------------------------------------------------------
static int logfuse_setcrtime(const char *path, const timespec *theTime)
{	int		sysErr;



	// Set the time
	sysErr = logfuse_set_timespec(path, ATTR_CMN_CRTIME, *theTime);
	logfuse_log("logfuse_setcrtime(%s) err=%d", path, sysErr);

	RETURN_FUSE_ERRNO();
}
#endif // FUSE_APPLE





#if FUSE_APPLE
//============================================================================
//		logfuse_chflags : Set the file flags.
//----------------------------------------------------------------------------
static int logfuse_chflags(const char *path, uint32_t theFlags)
{	int		sysErr;



	// Set the flags
	sysErr = lchflags(path, theFlags);
	logfuse_log("logfuse_setcrtime(%s) err=%d", path, sysErr);

	RETURN_FUSE_ERRNO();
}
#endif // FUSE_APPLE





#if FUSE_APPLE
//============================================================================
//		logfuse_setattr_x : Set extended attributes.
//----------------------------------------------------------------------------
static int logfuse_setattr_x(const char *path, struct setattr_x *theAttributes)
{	int		sysErr;



	// Set the attributes
	if (SETATTR_WANTS_MODE(theAttributes))
		{
		sysErr = lchmod(path, theAttributes->mode);
		if (sysErr == -1)
			goto done;
		}

	if (SETATTR_WANTS_UID(theAttributes) || SETATTR_WANTS_GID(theAttributes))
		{
		sysErr = lchown(path,
						SETATTR_WANTS_UID(theAttributes) ? theAttributes->uid : -1,
						SETATTR_WANTS_GID(theAttributes) ? theAttributes->gid : -1);
		if (sysErr == -1)
			goto done;
		}

	if (SETATTR_WANTS_SIZE(theAttributes))
		{
		sysErr = truncate(path, theAttributes->size);
		if (sysErr != -1)
			goto done;
		}

	if (SETATTR_WANTS_ACCTIME(theAttributes))
		{
		sysErr = logfuse_set_timespec(path, ATTR_CMN_ACCTIME, theAttributes->acctime);
		if (sysErr != -1)
			goto done;
		}

	if (SETATTR_WANTS_MODTIME(theAttributes))
		{
		sysErr = logfuse_set_timespec(path, ATTR_CMN_MODTIME, theAttributes->modtime);
		if (sysErr != -1)
			goto done;
		}

	if (SETATTR_WANTS_CRTIME(theAttributes))
		{
		sysErr = logfuse_set_timespec(path, ATTR_CMN_CRTIME, theAttributes->crtime);
		if (sysErr != -1)
			goto done;
		}

	if (SETATTR_WANTS_CHGTIME(theAttributes))
		{
		sysErr = logfuse_set_timespec(path, ATTR_CMN_CHGTIME, theAttributes->chgtime);
		if (sysErr != -1)
			goto done;
		}

	if (SETATTR_WANTS_BKUPTIME(theAttributes))
		{
		sysErr = logfuse_set_timespec(path, ATTR_CMN_BKUPTIME, theAttributes->bkuptime);
		if (sysErr != -1)
			goto done;
		}

	if (SETATTR_WANTS_FLAGS(theAttributes))
		{
		sysErr = lchflags(path, theAttributes->flags);
		if (sysErr != -1)
			goto done;
		}

done:
	logfuse_log("logfuse_setattr_x(%s) err=%d", path, sysErr);

	RETURN_FUSE_ERRNO();
}
#endif // FUSE_APPLE





#if FUSE_APPLE
//============================================================================
//		logfuse_fsetattr_x : Set extended attributes.
//----------------------------------------------------------------------------
static int logfuse_fsetattr_x(const char *path, setattr_x *theAttributes, fuse_file_info *fileInfo)
{	int		sysErr;



	// Set the attributes
	if (SETATTR_WANTS_MODE(theAttributes))
		{
		sysErr = fchmod(fileInfo->fh, theAttributes->mode);
		if (sysErr == -1)
			goto done;
		}

	if (SETATTR_WANTS_UID(theAttributes) || SETATTR_WANTS_GID(theAttributes))
		{
		sysErr = fchown(fileInfo->fh,
						SETATTR_WANTS_UID(theAttributes) ? theAttributes->uid : -1,
						SETATTR_WANTS_GID(theAttributes) ? theAttributes->gid : -1);
		if (sysErr == -1)
			goto done;
		}

	if (SETATTR_WANTS_SIZE(theAttributes))
		{
		sysErr = ftruncate(fileInfo->fh, theAttributes->size);
		if (sysErr != -1)
			goto done;
		}

	if (SETATTR_WANTS_ACCTIME(theAttributes))
		{
		sysErr = logfuse_fset_timespec(fileInfo->fh, ATTR_CMN_ACCTIME, theAttributes->acctime);
		if (sysErr != -1)
			goto done;
		}

	if (SETATTR_WANTS_MODTIME(theAttributes))
		{
		sysErr = logfuse_fset_timespec(fileInfo->fh, ATTR_CMN_MODTIME, theAttributes->modtime);
		if (sysErr != -1)
			goto done;
		}

	if (SETATTR_WANTS_CRTIME(theAttributes))
		{
		sysErr = logfuse_fset_timespec(fileInfo->fh, ATTR_CMN_CRTIME, theAttributes->crtime);
		if (sysErr != -1)
			goto done;
		}

	if (SETATTR_WANTS_CHGTIME(theAttributes))
		{
		sysErr = logfuse_fset_timespec(fileInfo->fh, ATTR_CMN_CHGTIME, theAttributes->chgtime);
		if (sysErr != -1)
			goto done;
		}

	if (SETATTR_WANTS_BKUPTIME(theAttributes))
		{
		sysErr = logfuse_fset_timespec(fileInfo->fh, ATTR_CMN_BKUPTIME, theAttributes->bkuptime);
		if (sysErr != -1)
			goto done;
		}

	if (SETATTR_WANTS_FLAGS(theAttributes))
		{
		sysErr = fchflags(fileInfo->fh, theAttributes->flags);
		if (sysErr != -1)
			goto done;
		}

done:
	logfuse_log("logfuse_setattr_x(%s) err=%d", path, sysErr);

	RETURN_FUSE_ERRNO();
}
#endif // FUSE_APPLE





//============================================================================
//		main : Entry point.
//----------------------------------------------------------------------------
int main(int argc, char **argv)
{	fuse_args			fuseArgs = FUSE_ARGS_INIT(argc, argv);
	fuse_operations		fuseOps;
	int					sysErr;



	// Initialise ourselves
	memset(&fuseOps, 0x00, sizeof(fuseOps));

	fuseOps.getattr			= logfuse_getattr;
	fuseOps.readlink		= logfuse_readlink;
//	fuseOps.getdir			= -> readdir
	fuseOps.mknod			= logfuse_mknod;
	fuseOps.mkdir			= logfuse_mkdir;
	fuseOps.unlink			= logfuse_unlink;
	fuseOps.rmdir			= logfuse_rmdir;
	fuseOps.symlink			= logfuse_symlink;
	fuseOps.rename			= logfuse_rename;
	fuseOps.link			= logfuse_link;
	fuseOps.chmod			= logfuse_chmod;
	fuseOps.chown			= logfuse_chown;
	fuseOps.truncate		= logfuse_truncate;
//	fuseOps.utime			= -> utimens
	fuseOps.open			= logfuse_open;
	fuseOps.read			= logfuse_read;
	fuseOps.write			= logfuse_write;
	fuseOps.statfs			= logfuse_statfs;
	fuseOps.flush			= logfuse_flush;
	fuseOps.release			= logfuse_release;
	fuseOps.fsync			= logfuse_fsync;
	fuseOps.setxattr		= logfuse_setxattr;
	fuseOps.getxattr		= logfuse_getxattr;
	fuseOps.listxattr		= logfuse_listxattr;
	fuseOps.removexattr		= logfuse_removexattr;
	fuseOps.opendir			= logfuse_opendir;
	fuseOps.readdir			= logfuse_readdir;
	fuseOps.releasedir		= logfuse_releasedir;
	fuseOps.fsyncdir		= logfuse_fsyncdir;
	fuseOps.init			= logfuse_init;
	fuseOps.destroy			= logfuse_destroy;
	fuseOps.access			= logfuse_access;
	fuseOps.create			= logfuse_create;
	fuseOps.ftruncate		= logfuse_ftruncate;
	fuseOps.fgetattr		= logfuse_fgetattr;
	fuseOps.lock			= logfuse_lock;
	fuseOps.utimens			= logfuse_utimens;
//	fuseOps.bmap			= Block device only
	fuseOps.ioctl			= logfuse_ioctl;
	fuseOps.poll			= logfuse_poll;
//	fuseOps.write_buf		= -> write
//	fuseOps.read_buf		= -> read
	fuseOps.flock			= logfuse_flock;
	fuseOps.fallocate		= logfuse_fallocate;

#if FUSE_APPLE
	fuseOps.setvolname		= logfuse_setvolname;
	fuseOps.exchange		= logfuse_exchange;
	fuseOps.getxtimes		= logfuse_getxtimes;
	fuseOps.setbkuptime		= logfuse_setbkuptime;
	fuseOps.setchgtime		= logfuse_setchgtime;
	fuseOps.setcrtime		= logfuse_setcrtime;
	fuseOps.chflags			= logfuse_chflags;
	fuseOps.setattr_x		= logfuse_setattr_x;
	fuseOps.fsetattr_x		= logfuse_fsetattr_x;
#endif



	// Run the filesystem
	umask(0);

	sysErr = fuse_opt_parse(&fuseArgs, nullptr, nullptr, nullptr);
	if (sysErr == 0)
		sysErr = fuse_main(fuseArgs.argc, fuseArgs.argv, &fuseOps, nullptr);
	
    return(sysErr);
}



