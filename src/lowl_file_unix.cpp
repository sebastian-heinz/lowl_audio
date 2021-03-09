#ifdef LOWL_UNIX

#include "lowl_file.h"

#include <sys/stat.h>
#include <cerrno>

#ifdef MSVC
#define S_ISREG(m) ((m)&_S_IFREG)
#include <io.h>
#endif
#ifndef S_ISREG
#define S_ISREG(m) ((m)&S_IFREG)
#endif

#ifndef NO_FCNTL

#include <fcntl.h>

#else
#include <sys/ioctl.h>
#endif

struct LowlFileUnix {
    FILE *file;
};

std::string Lowl::LowlFile::get_path() {
    return path;
}

void Lowl::LowlFile::open(const std::string &p_path, Lowl::LowlError &error) {
    LowlFileUnix *unix = (LowlFileUnix *) user_data;
    path = p_path;

    if (unix->file) {
        fclose(unix->file);
    }
    unix->file = nullptr;

    struct stat st;
    int err = stat(path.c_str(), &st);
    if (!err) {
        switch (st.st_mode & S_IFMT) {
            case S_IFLNK:
            case S_IFREG: {
                break;
            }
            default: {
                //return ERR_FILE_CANT_OPEN;
                error.set_error(Lowl::LowlError::Code::Error);
                return;
            }
        }
    }

    unix->file = fopen(path.c_str(), "rb");
    if (unix->file == nullptr) {
        switch (errno) {
            case ENOENT: {
                //   last_error = ERR_FILE_NOT_FOUND;
                error.set_error(Lowl::LowlError::Code::Error);
                return;
            }
            default: {
                // last_error = ERR_FILE_CANT_OPEN;
                error.set_error(Lowl::LowlError::Code::Error);
                return;
            }
        }
    }

    // Set close on exec to avoid leaking it to subprocesses.
    int fd = fileno(unix->file);

    if (fd != -1) {
#if defined(NO_FCNTL)
        unsigned long par = 0;
        ioctl(fd, FIOCLEX, &par);
#else
        int opts = fcntl(fd, F_GETFD);
        fcntl(fd, F_SETFD, opts | FD_CLOEXEC);
#endif
    }
}

void Lowl::LowlFile::close() {
    LowlFileUnix *unix = (LowlFileUnix *) user_data;

    if (!unix->file) {
        return;
    }

    fclose(unix->file);
    unix->file = nullptr;
}

void Lowl::LowlFile::seek(size_t p_position) {
    LowlFileUnix *unix = (LowlFileUnix *) user_data;
    if (!unix->file) {
        return;
    }

    if (fseek(unix->file, p_position, SEEK_SET)) {

    }
}

size_t Lowl::LowlFile::get_position() const {
    LowlFileUnix *unix = (LowlFileUnix *) user_data;
    if (!unix->file) {
        // error
        return 0;
    }
    long pos = ftell(unix->file);
    if (pos < 0) {
        // error
        return 0;
    }
    return pos;
}

size_t Lowl::LowlFile::get_length() const {
    LowlFileUnix *unix = (LowlFileUnix *) user_data;
    if (!unix->file) {
        // error
        return 0;
    }

    long pos = ftell(unix->file);
    if (pos < 0) {
        // error
    }
    int seek_end = fseek(unix->file, 0, SEEK_END);
    if (!seek_end) {
        // error
    }
    long size = ftell(unix->file);
    if (size < 0) {
        // error
    }
    int seek_set = fseek(unix->file, pos, SEEK_SET);
    if (!seek_set) {
        // error
    }
    return size;
}

uint8_t Lowl::LowlFile::read_u8() const {
    LowlFileUnix *unix = (LowlFileUnix *) user_data;
    if (!unix->file) {
        // error
        return 0;
    }
    uint8_t b;
    if (fread(&b, 1, 1, unix->file) == 0) {
        // check_errors();
        b = '\0';
    }
    return b;
}

int Lowl::LowlFile::get_buffer(uint8_t *p_dst, int p_length) const {
    LowlFileUnix *unix = (LowlFileUnix *) user_data;
    if (!unix->file) {
        // error
        return 0;
    }
    int read = fread(p_dst, 1, p_length, unix->file);
    //check_errors();
    return read;
}

bool Lowl::LowlFile::is_eof() const {
    LowlFileUnix *unix = (LowlFileUnix *) user_data;
    if (!unix->file) {
        return true;
    }
    return feof(unix->file);
}

Lowl::LowlFile::LowlFile() {
    user_data = new LowlFileUnix;
    LowlFileUnix *unix = (LowlFileUnix *) user_data;
    unix->file = nullptr;
    path = std::string();
}

Lowl::LowlFile::~LowlFile() {
    delete (LowlFileUnix *) user_data;
}

#endif