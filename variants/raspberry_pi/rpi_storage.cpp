#include "rpi_storage.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <string>
#include <system_error>
#include <unistd.h>

namespace
{
bool validName(const char *name)
{
    if (!name || !name[0] || name[0] == '.')
        return false;

    for (const char *p = name; *p; ++p)
    {
        const char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
            return false;
    }
    return true;
}

bool dataDirectory(std::filesystem::path &path)
{
    const char *configured = std::getenv("MESHCOM_DATA_DIR");
    const char *home = std::getenv("HOME");
    if (configured && configured[0])
        path = configured;
    else if (home && home[0])
        path = std::filesystem::path(home) / ".meshcom" / "raspberry_pi";
    else
    {
        std::fprintf(stderr, "[STORAGE];init;missing_HOME\n");
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec)
    {
        std::fprintf(stderr, "[STORAGE];init;mkdir_failed;%s\n", ec.message().c_str());
        return false;
    }
    return true;
}

bool writeAll(int fd, const uint8_t *data, size_t size)
{
    size_t written = 0;
    while (written < size)
    {
        const ssize_t n = ::write(fd, data + written, size - written);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return false;
        written += static_cast<size_t>(n);
    }
    return true;
}
}

namespace RaspberryPiStorage
{
bool initialize()
{
    std::filesystem::path dir;
    return dataDirectory(dir);
}

ReadResult readFile(const char *name, std::string &contents)
{
    if (!validName(name))
    {
        std::fprintf(stderr, "[STORAGE];read;invalid_name\n");
        return ReadResult::error;
    }

    std::filesystem::path dir;
    if (!dataDirectory(dir))
        return ReadResult::error;

    const auto path = dir / name;
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec)
    {
        std::fprintf(stderr, "[STORAGE];read;stat_failed;%s\n", ec.message().c_str());
        return ReadResult::error;
    }
    if (!exists)
        return ReadResult::missing;

    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
    {
        std::fprintf(stderr, "[STORAGE];read;open_failed;errno=%d\n", errno);
        return ReadResult::error;
    }

    contents.clear();
    char buffer[1024];
    bool ok = true;
    while (true)
    {
        const ssize_t n = ::read(fd, buffer, sizeof(buffer));
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0)
        {
            std::fprintf(stderr, "[STORAGE];read;read_failed;errno=%d\n", errno);
            ok = false;
            break;
        }
        if (n == 0)
            break;
        contents.append(buffer, static_cast<size_t>(n));
    }

    if (::close(fd) != 0 && ok)
    {
        std::fprintf(stderr, "[STORAGE];read;close_failed;errno=%d\n", errno);
        ok = false;
    }
    return ok ? ReadResult::ok : ReadResult::error;
}

bool writeFileAtomic(const char *name, const void *data, size_t size)
{
    if (!validName(name) || (!data && size > 0))
    {
        std::fprintf(stderr, "[STORAGE];write;invalid_argument\n");
        return false;
    }

    std::filesystem::path dir;
    if (!dataDirectory(dir))
        return false;

    const std::string tempName = std::string(name) + ".tmp";
    const auto tempPath = dir / tempName;
    const auto targetPath = dir / name;
    const int fd = ::open(tempPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0)
    {
        std::fprintf(stderr, "[STORAGE];write;open_failed;errno=%d\n", errno);
        return false;
    }

    bool ok = writeAll(fd, static_cast<const uint8_t *>(data), size);
    if (!ok)
        std::fprintf(stderr, "[STORAGE];write;write_failed;errno=%d\n", errno);
    if (ok && ::fsync(fd) != 0)
    {
        std::fprintf(stderr, "[STORAGE];write;fsync_failed;errno=%d\n", errno);
        ok = false;
    }
    if (::close(fd) != 0 && ok)
    {
        std::fprintf(stderr, "[STORAGE];write;close_failed;errno=%d\n", errno);
        ok = false;
    }

    if (ok && ::rename(tempPath.c_str(), targetPath.c_str()) != 0)
    {
        std::fprintf(stderr, "[STORAGE];write;rename_failed;errno=%d\n", errno);
        ok = false;
    }
    if (ok)
    {
        const int dirFd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (dirFd < 0 || ::fsync(dirFd) != 0)
        {
            std::fprintf(stderr, "[STORAGE];write;directory_sync_failed;errno=%d\n", errno);
            ok = false;
        }
        if (dirFd >= 0 && ::close(dirFd) != 0 && ok)
        {
            std::fprintf(stderr, "[STORAGE];write;directory_close_failed;errno=%d\n", errno);
            ok = false;
        }
    }
    if (!ok)
    {
        std::error_code ec;
        std::filesystem::remove(tempPath, ec);
    }
    return ok;
}

bool removeFile(const char *name)
{
    if (!validName(name))
    {
        std::fprintf(stderr, "[STORAGE];remove;invalid_name\n");
        return false;
    }

    std::filesystem::path dir;
    if (!dataDirectory(dir))
        return false;

    std::error_code ec;
    std::filesystem::remove(dir / name, ec);
    if (ec)
    {
        std::fprintf(stderr, "[STORAGE];remove;failed;%s\n", ec.message().c_str());
        return false;
    }
    return true;
}
}
