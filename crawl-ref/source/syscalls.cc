/**
 * @file
 * @brief Wrappers for sys/libc calls, mostly for charset purposes.
**/

#include "AppHdr.h"

#ifdef TARGET_OS_WINDOWS
# ifdef TARGET_COMPILER_VC
#  include <direct.h>
# endif
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <wincrypt.h>
# include <io.h>
#else
# include <dirent.h>
# include <unistd.h>
# include <fcntl.h>
# include <sys/types.h>
# include <sys/stat.h>
#endif

#include "files.h"
#include "random.h"
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
        return res == len;
    }

    return false;
#endif
}

#ifdef TARGET_OS_WINDOWS
# ifndef UNIX
// should check the presence of alarm() instead
static void CALLBACK _abortion(PVOID dummy, BOOLEAN timedout)
{
    TerminateProcess(GetCurrentProcess(), 0);
}

void alarm(unsigned int seconds)
{
    HANDLE dummy;
    CreateTimerQueueTimer(&dummy, 0, _abortion, 0, seconds * 1000, 0, 0);
}
# endif

# ifndef HAVE_FDATASYNC
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

int mkstemp(char *dummy)
{
    HANDLE fh;

    for (int tries = 0; tries < 100; tries++)
    {
        wchar_t filename[MAX_PATH];
        int len = GetTempPathW(MAX_PATH - 8, filename);
        ASSERT(len);
        for (int i = 0; i < 6; i++)
            filename[len + i] = 'a' + random2(26);
        filename[len + 6] = 0;
        fh = CreateFileW(filename, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                         CREATE_NEW,
                         FILE_FLAG_DELETE_ON_CLOSE | FILE_ATTRIBUTE_TEMPORARY,
                         NULL);
        if (fh != INVALID_HANDLE_VALUE)
            return _open_osfhandle((intptr_t)fh, 0);
    }

    die("can't create temporary file in %%TMPDIR%%");
}

#else
// non-Windows
# ifndef HAVE_FDATASYNC
// At least MacOS X 10.6 has it (as required by Posix) but present only
// as a symbol in the libraries without a proper header.
int fdatasync(int fd)
{
#  ifdef F_FULLFSYNC
    // On MacOS X, fsync() doesn't even try to actually do what it was asked.
    // Sane systems might have this problem only on disks that do write caching
    // but ignore flush requests.  fsync() should never return before the disk
    // claims the flush completed, but this is not the case on OS X.
    //
    // Except, this is the case for internal drives only.  For "external" ones,
    // F_FULLFSYNC is said to fail (at least on some versions of OS X), while
    // fsync() actually works.  Thus, we need to try both.
    return fcntl(fd, F_FULLFSYNC, 0) && fsync(fd);
#  else
    return fsync(fd);
#  endif
}
# endif
#endif

// The old school way of doing short delays via low level I/O sync.
// Good for systems like old versions of Solaris that don't have usleep.
#ifdef NEED_USLEEP

# ifdef TARGET_OS_WINDOWS
void usleep(unsigned long time)
{
    ASSERT(time > 0);
    ASSERT(!(time % 1000));
    Sleep(time/1000);
}
# else

#include <sys/time.h>
#include <sys/types.h>
#include <sys/unistd.h>

void usleep(unsigned long time)
{
    struct timeval timer;

    timer.tv_sec  = (time / 1000000L);
    timer.tv_usec = (time % 1000000L);

    select(0, NULL, NULL, NULL, &timer);
}
# endif
#endif

bool file_exists(const string &name)
{
#ifdef TARGET_OS_WINDOWS
    DWORD lAttr = GetFileAttributesW(OUTW(name));
    return lAttr != INVALID_FILE_ATTRIBUTES
           && !(lAttr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    const int err = ::stat(OUTS(name), &st);
    return !err && S_ISREG(st.st_mode);
#endif
}

// Low-tech existence check.
bool dir_exists(const string &dir)
{
#ifdef TARGET_OS_WINDOWS
    DWORD lAttr = GetFileAttributesW(OUTW(dir));
    return lAttr != INVALID_FILE_ATTRIBUTES
           && (lAttr & FILE_ATTRIBUTE_DIRECTORY);
#elif defined(HAVE_STAT)
    struct stat st;
    const int err = ::stat(OUTS(dir), &st);
    return !err && S_ISDIR(st.st_mode);
#else
    DIR *d = opendir(OUTS(dir));
    const bool exists = !!d;
    if (d)
        closedir(d);

    return exists;
#endif
}

static inline bool _is_good_filename(const string &s)
{
    return s != "." && s != "..";
}

// Returns the names of all files in the given directory. Note that the
// filenames returned are relative to the directory.
vector<string> get_dir_files(const string &dirname)
{
    vector<string> files;

#ifdef TARGET_OS_WINDOWS
    WIN32_FIND_DATAW lData;
    string dir = dirname;
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
        return files;

    while (dirent *entry = readdir(dir))
    {
        string name = mb_to_utf8(entry->d_name);
        if (_is_good_filename(name))
            files.push_back(name);
    }
    closedir(dir);
#endif

    return files;
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
