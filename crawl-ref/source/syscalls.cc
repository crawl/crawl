/**
 * @file
 * @brief Wrappers for sys/libc calls, mostly for charset purposes.
**/

#include "AppHdr.h"
#include <dirent.h>

#ifdef TARGET_OS_WINDOWS
# ifdef TARGET_COMPILER_VC
#  include <direct.h>
# endif
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <wincrypt.h>
# include <io.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "syscalls.h"
#include "unicode.h"

bool lock_file(int fd, bool write, bool wait)
{
#ifdef TARGET_OS_WINDOWS
    OVERLAPPED pos;
    pos.hEvent     = 0;
    pos.Offset     = 0;
    pos.OffsetHigh = 0;
    return !!LockFileEx((HANDLE)_get_osfhandle(fd),
                        (write ? LOCKFILE_EXCLUSIVE_LOCK : 0) |
                        (wait ? 0 : LOCKFILE_FAIL_IMMEDIATELY),
                        0, -1, -1, &pos);
#else
    struct flock fl;
    fl.l_type = write ? F_WRLCK : F_RDLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    return !fcntl(fd, wait ? F_SETLKW : F_SETLK, &fl);
#endif
}

bool unlock_file(int fd)
{
#ifdef TARGET_OS_WINDOWS
    return !!UnlockFile((HANDLE)_get_osfhandle(fd), 0, 0, -1, -1);
#else
    struct flock fl;
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    return !fcntl(fd, F_SETLK, &fl);
#endif
}

bool read_urandom(char *buf, int len)
{
#ifdef TARGET_OS_WINDOWS
    HCRYPTPROV hProvider = 0;

    if (!CryptAcquireContextW(&hProvider, 0, 0, PROV_RSA_FULL,
                              CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
    {
        return false;
    }

    if (!CryptGenRandom(hProvider, len, (BYTE*)buf))
    {
        CryptReleaseContext(hProvider, 0);
        return false;
    }

    CryptReleaseContext(hProvider, 0);
    return true;
#else
    /* Try opening from various system provided (hopefully) CSPRNGs */
    FILE* seed_f = fopen("/dev/urandom", "rb");
    if (!seed_f)
        seed_f = fopen("/dev/random", "rb");
    if (!seed_f)
        seed_f = fopen("/dev/srandom", "rb");
    if (!seed_f)
        seed_f = fopen("/dev/arandom", "rb");
    if (seed_f)
    {
        int res = fread(buf, 1, len, seed_f);
        fclose(seed_f);
        return (res == len);
    }

    return false;
#endif
}

#ifdef TARGET_OS_WINDOWS
# ifndef UNIX
// implementation by Richard W.M. Jones
// He claims this is the equivalent to fsync(), reading the MSDN doesn't seem
// to show that vital metadata is indeed flushed, others report that at least
// non-vital isn't.
int fdatasync(int fd)
{
    HANDLE h = (HANDLE)_get_osfhandle(fd);

    if (h == INVALID_HANDLE_VALUE)
    {
        errno = EBADF;
        return -1;
    }

    if (!FlushFileBuffers(h))
    {
        /* Translate some Windows errors into rough approximations of Unix
         * errors.  MSDN is useless as usual - in this case it doesn't
         * document the full range of errors.
         */
        switch (GetLastError())
        {
        /* eg. Trying to fsync a tty. */
        case ERROR_INVALID_HANDLE:
            errno = EINVAL;
            break;

        default:
            errno = EIO;
        }
        return -1;
    }

    return 0;
}
# endif
#endif

#ifdef NEED_FAKE_FDATASYNC
// At least MacOS X 10.6 has it (as required by Posix) but present only
// as a symbol in the libraries without a proper header.
int fdatasync(int fd)
{
    return fsync(fd);
}
#endif

bool file_exists(const std::string &name)
{
#ifdef TARGET_OS_WINDOWS
    DWORD lAttr = GetFileAttributesW(OUTW(name));
    return (lAttr != INVALID_FILE_ATTRIBUTES
            && !(lAttr & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    const int err = ::stat(OUTS(name), &st);
    return (!err && S_ISREG(st.st_mode));
#endif
}

// Low-tech existence check.
bool dir_exists(const std::string &dir)
{
#ifdef TARGET_OS_WINDOWS
    DWORD lAttr = GetFileAttributesW(OUTW(dir));
    return (lAttr != INVALID_FILE_ATTRIBUTES
            && (lAttr & FILE_ATTRIBUTE_DIRECTORY));
#elif defined(HAVE_STAT)
    struct stat st;
    const int err = ::stat(OUTS(dir), &st);
    return (!err && S_ISDIR(st.st_mode));
#else
    DIR *d = opendir(OUTS(dir));
    const bool exists = !!d;
    if (d)
        closedir(d);

    return (exists);
#endif
}

static inline bool _is_good_filename(const std::string &s)
{
    return (s != "." && s != "..");
}

// Returns the names of all files in the given directory. Note that the
// filenames returned are relative to the directory.
std::vector<std::string> get_dir_files(const std::string &dirname)
{
    std::vector<std::string> files;

#ifdef TARGET_OS_WINDOWS
    WIN32_FIND_DATAW lData;
    std::string dir = dirname;
    if (!dir.empty() && dir[dir.length() - 1] != FILE_SEPARATOR)
        dir += FILE_SEPARATOR;
    dir += "*";
    HANDLE hFind = FindFirstFileW(OUTW(dir), &lData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (_is_good_filename(utf16_to_8(lData.cFileName)))
                files.push_back(utf16_to_8(lData.cFileName));
        } while (FindNextFileW(hFind, &lData));
        FindClose(hFind);
    }
#else

    DIR *dir = opendir(OUTS(dirname));
    if (!dir)
        return (files);

    while (dirent *entry = readdir(dir))
    {
        std::string name = mb_to_utf8(entry->d_name);
        if (_is_good_filename(name))
            files.push_back(name);
    }
    closedir(dir);
#endif

    return (files);
}

int rename_u(const char *oldpath, const char *newpath)
{
#ifdef TARGET_OS_WINDOWS
    return !MoveFileExW(OUTW(oldpath), OUTW(newpath),
                        MOVEFILE_REPLACE_EXISTING);
#else
    return rename(OUTS(oldpath), OUTS(newpath));
#endif
}

int unlink_u(const char *pathname)
{
#ifdef TARGET_OS_WINDOWS
    return _wunlink(OUTW(pathname));
#else
    return unlink(OUTS(pathname));
#endif
}

int chmod_u(const char *path, mode_t mode)
{
#ifdef TARGET_OS_WINDOWS
    return _wchmod(OUTW(path), mode);
#else
    return chmod(OUTS(path), mode);
#endif
}

FILE *fopen_u(const char *path, const char *mode)
{
#ifdef TARGET_OS_WINDOWS
    // Why it wants the mode string as double-byte is beyond me.
    return _wfopen(OUTW(path), OUTW(mode));
#else
    return fopen(OUTS(path), mode);
#endif
}

int mkdir_u(const char *pathname, mode_t mode)
{
#ifdef TARGET_OS_WINDOWS
    return _wmkdir(OUTW(pathname));
#else
    return mkdir(OUTS(pathname), mode);
#endif
}

int open_u(const char *pathname, int flags, mode_t mode)
{
#ifdef TARGET_OS_WINDOWS
    return _wopen(OUTW(pathname), flags, mode);
#else
    return open(OUTS(pathname), flags, mode);
#endif
}
