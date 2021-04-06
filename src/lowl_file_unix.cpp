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

void Lowl::LowlFile::open(const std::string &p_path, Lowl::Error &error) {
    LowlFileUnix *usr_data = (LowlFileUnix *) user_data;
    path = p_path;

    if (usr_data->file) {
        fclose(usr_data->file);
    }
    usr_data->file = nullptr;

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
                error.set_error(ErrorCode::Error);
                return;
            }
        }
    }

    usr_data->file = fopen(path.c_str(), "rb");
    if (usr_data->file == nullptr) {
        switch (errno) {
            case ENOENT: {
                //   last_error = ERR_FILE_NOT_FOUND;
                error.set_error(ErrorCode::Error);
                return;
            }
            default: {
                // last_error = ERR_FILE_CANT_OPEN;
                error.set_error(ErrorCode::Error);
                return;
            }
        }
    }

    // Set close on exec to avoid leaking it to subprocesses.
    int fd = fileno(usr_data->file);

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
    LowlFileUnix *usr_data = (LowlFileUnix *) user_data;

    if (!usr_data->file) {
        return;
    }

    fclose(usr_data->file);
    usr_data->file = nullptr;
}

void Lowl::LowlFile::seek(size_t p_position) {
    LowlFileUnix *usr_data = (LowlFileUnix *) user_data;
    if (!usr_data->file) {
        return;
    }

    if (fseek(usr_data->file, p_position, SEEK_SET)) {

    }
}

size_t Lowl::LowlFile::get_position() const {
    LowlFileUnix *usr_data = (LowlFileUnix *) user_data;
    if (!usr_data->file) {
        // error
        return 0;
    }
    long pos = ftell(usr_data->file);
    if (pos < 0) {
        // error
        return 0;
    }
    return pos;
}

size_t Lowl::LowlFile::get_length() const {
    LowlFileUnix *usr_data = (LowlFileUnix *) user_data;
    if (!usr_data->file) {
        // error
        return 0;
    }

    long pos = ftell(usr_data->file);
    if (pos < 0) {
        // error
    }
    int seek_end = fseek(usr_data->file, 0, SEEK_END);
    if (!seek_end) {
        // error
    }
    long size = ftell(usr_data->file);
    if (size < 0) {
        // error
    }
    int seek_set = fseek(usr_data->file, pos, SEEK_SET);
    if (!seek_set) {
        // error
    }
    return size;
}

uint8_t Lowl::LowlFile::read_u8() const {
    LowlFileUnix *usr_data = (LowlFileUnix *) user_data;
    if (!usr_data->file) {
        // error
        return 0;
    }
    uint8_t b;
    if (fread(&b, 1, 1, usr_data->file) == 0) {
        // check_errors();
        b = '\0';
    }
    return b;
}

std::unique_ptr<uint8_t[]> Lowl::LowlFile::read_buffer(size_t &length) const {
    LowlFileUnix *usr_data = (LowlFileUnix *) user_data;
    if (!usr_data->file) {
        // error
        return 0;
    }
    std::unique_ptr<uint8_t[]> data = std::make_unique<uint8_t[]>(length);
    length = fread(data.get(), 1, length, usr_data->file);
    return data;
}

bool Lowl::LowlFile::is_eof() const {
    LowlFileUnix *usr_data = (LowlFileUnix *) user_data;
    if (!usr_data->file) {
        return true;
    }
    return feof(usr_data->file);
}

Lowl::LowlFile::LowlFile() {
    user_data = new LowlFileUnix;
    LowlFileUnix *usr_data = (LowlFileUnix *) user_data;
    usr_data->file = nullptr;
    path = std::string();
}

Lowl::LowlFile::~LowlFile() {
    delete (LowlFileUnix *) user_data;
}


#endif