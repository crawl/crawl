/**
 * @file
 * @brief Wrappers for sys/libc calls, mostly for charset purposes.
**/

#include "AppHdr.h"

#include "syscalls.h"

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
#include "unicode.h"

#ifdef __ANDROID__
#define HAVE_STAT
#include "player.h"
#include <errno.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>

extern "C"
{
    extern JNIEnv *Android_JNI_GetEnv(); // sigh
}

AAssetManager *_android_asset_manager = nullptr; // XXX

// Used to save the game on SDLActivity.onPause
extern "C" JNIEXPORT void JNICALL
Java_org_libsdl_app_SDLActivity_nativeSaveGame(
    JNIEnv* env, jclass thiz)
{
    if (you.save)
        save_game(false);
}

bool jni_keyboard_control(bool toggle)
{
    JNIEnv *env = Android_JNI_GetEnv();
    jclass sdlClass = env->FindClass("org/libsdl/app/SDLActivity");

    if (!sdlClass)
        return false;

    jmethodID mid =
        env->GetStaticMethodID(sdlClass, "jniKeyboardControl", "(Z)Z");
    jboolean shown = env->CallStaticBooleanMethod(sdlClass, mid, toggle);

    return shown;
}
#endif

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
static void CALLBACK _abortion(PVOID /*dummy*/, BOOLEAN /*timedout*/)
{
    TerminateProcess(GetCurrentProcess(), 0);
}

void alarm(unsigned int seconds)
{
    HANDLE dummy;
    CreateTimerQueueTimer(&dummy, 0, _abortion, 0, seconds * 1000, 0, 0);
}
# endif

# ifndef CRAWL_HAVE_FDATASYNC
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
         * errors. MSDN is useless as usual - in this case it doesn't
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

# ifndef CRAWL_HAVE_MKSTEMP
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
        fh = CreateFileW(filename, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                         CREATE_NEW,
                         FILE_FLAG_DELETE_ON_CLOSE | FILE_ATTRIBUTE_TEMPORARY,
                         nullptr);
        if (fh != INVALID_HANDLE_VALUE)
            return _open_osfhandle((intptr_t)fh, 0);
    }

    die("can't create temporary file in %%TMPDIR%%");
}
# endif

#else
// non-Windows
# ifndef CRAWL_HAVE_FDATASYNC
// At least MacOS X 10.6 has it (as required by Posix) but present only
// as a symbol in the libraries without a proper header.
int fdatasync(int fd)
{
#  ifdef F_FULLFSYNC
    // On MacOS X, fsync() doesn't even try to actually do what it was asked.
    // Sane systems might have this problem only on disks that do write caching
    // but ignore flush requests. fsync() should never return before the disk
    // claims the flush completed, but this is not the case on OS X.
    //
    // Except, this is the case for internal drives only. For "external" ones,
    // F_FULLFSYNC is said to fail (at least on some versions of OS X), while
    // fsync() actually works. Thus, we need to try both.
    return fcntl(fd, F_FULLFSYNC, 0) && fsync(fd);
#  else
    return fsync(fd);
#  endif
}
# endif
#endif

// The old school way of doing short delays via low level I/O sync.
// Good for systems like old versions of Solaris that don't have usleep.
#ifndef CRAWL_HAVE_USLEEP

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

    select(0, nullptr, nullptr, nullptr, &timer);
}
# endif
#endif

#ifdef __ANDROID__
AAssetManager *_android_get_asset_manager()
{
    JNIEnv *env = Android_JNI_GetEnv();
    jclass sdlClass = env->FindClass("org/libsdl/app/SDLActivity");

    if (!sdlClass)
        return nullptr;

    jmethodID mid =
        env->GetStaticMethodID(sdlClass, "getContext",
                               "()Landroid/content/Context;");
    jobject context = env->CallStaticObjectMethod(sdlClass, mid);

    if (!context)
        return nullptr;

    mid = env->GetMethodID(env->GetObjectClass(context), "getAssets",
                           "()Landroid/content/res/AssetManager;");
    jobject assets = env->CallObjectMethod(context, mid);

    if (!assets)
        return nullptr;

    return AAssetManager_fromJava(env, assets);
}
#endif

bool file_exists(const string &name)
{
#ifdef TARGET_OS_WINDOWS
    DWORD lAttr = GetFileAttributesW(OUTW(name));
    return lAttr != INVALID_FILE_ATTRIBUTES
           && !(lAttr & FILE_ATTRIBUTE_DIRECTORY);
#else
#ifdef __ANDROID__
    if (name.find(ANDROID_ASSETS) == 0)
    {
        if (!_android_asset_manager)
            _android_asset_manager = _android_get_asset_manager();

        ASSERT(_android_asset_manager);

        AAsset* asset = AAssetManager_open(_android_asset_manager,
                                           name.substr(strlen(ANDROID_ASSETS)
                                                       + 1)
                                               .c_str(),
                                           AASSET_MODE_UNKNOWN);
        if (asset)
        {
            AAsset_close(asset);
            return true;
        }
        return false;
    }
#endif
    struct stat st;
    const int err = ::stat(OUTS(name), &st);
    return !err && S_ISREG(st.st_mode);
#endif
}

#ifdef __ANDROID__
/**
 * Remove an ANDROID_ASSETS prefix and strip any trailing slashes
 * from a directory name.
 */
static string _android_strip_dir_slash(const string &in)
{
    string out = in.substr(strlen(ANDROID_ASSETS) + 1);
    if (out.back() == '/')
        out = out.substr(0, out.length() - 1);

    return out;
}
#endif

// Low-tech existence check.
bool dir_exists(const string &dir)
{
#ifdef TARGET_OS_WINDOWS
    DWORD lAttr = GetFileAttributesW(OUTW(dir));
    return lAttr != INVALID_FILE_ATTRIBUTES
           && (lAttr & FILE_ATTRIBUTE_DIRECTORY);
#else
#ifdef __ANDROID__
    if (dir.find(ANDROID_ASSETS) == 0)
    {
        if (!_android_asset_manager)
            _android_asset_manager = _android_get_asset_manager();

        ASSERT(_android_asset_manager);

        AAssetDir* adir = AAssetManager_openDir(_android_asset_manager,
                                                _android_strip_dir_slash(dir)
                                                    .c_str());
        if (adir)
        {
            AAssetDir_close(adir);
            return true;
        }
        return false;
    }
#endif
#ifdef HAVE_STAT
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
#ifdef __ANDROID__
    if (dirname.find(ANDROID_ASSETS) == 0)
    {
        if (!_android_asset_manager)
            _android_asset_manager = _android_get_asset_manager();

        ASSERT(_android_asset_manager);

        AAssetDir* adir =
            AAssetManager_openDir(_android_asset_manager,
                                  _android_strip_dir_slash(dirname).c_str());

        if (!adir)
            return files;

        const char *file;
        while ((file = AAssetDir_getNextFileName(adir)) != nullptr)
            files.emplace_back(file);

        AAssetDir_close(adir);
        return files;
    }
#endif
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

#ifdef __ANDROID__
/**
 * This implementation of handling Android fopens to Android assets
 * appears to originate from here:
 * http://www.50ply.com/blog/2013/01/19/
 *   loading-compressed-android-assets-with-file-pointer/
 */

static int _android_read(void* cookie, char* buf, int size)
{
    return AAsset_read((AAsset*)cookie, buf, size);
}

static int _android_write(void* cookie, const char* buf, int size)
{
    return EACCES; // can't provide write access to the apk
}

static fpos_t _android_seek(void* cookie, fpos_t offset, int whence)
{
    return AAsset_seek((AAsset*)cookie, offset, whence);
}

static int _android_close(void* cookie)
{
    AAsset_close((AAsset*)cookie);
    return 0;
}
#endif

FILE *fopen_u(const char *path, const char *mode)
{
#ifdef TARGET_OS_WINDOWS
    // Why it wants the mode string as double-byte is beyond me.
    return _wfopen(OUTW(path), OUTW(mode));
#else
#ifdef __ANDROID__
    if (strstr(path, ANDROID_ASSETS) == path)
    {
        if (!mode || mode[0] == 'w')
            return nullptr;

        if (!_android_asset_manager)
            _android_asset_manager = _android_get_asset_manager();

        ASSERT(_android_asset_manager);

        AAsset* asset = AAssetManager_open(_android_asset_manager,
                                           path + strlen(ANDROID_ASSETS) + 1,
                                           AASSET_MODE_RANDOM);
        if (!asset)
            return nullptr;

        return funopen(asset, _android_read, _android_write, _android_seek,
                       _android_close);
    }
#endif
    return fopen(OUTS(path), mode);
#endif
}

int mkdir_u(const char *pathname, mode_t mode)
{
#ifdef TARGET_OS_WINDOWS
    UNUSED(mode);
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
