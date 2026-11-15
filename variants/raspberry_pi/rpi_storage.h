#pragma once

#include <cstddef>
#include <string>

namespace RaspberryPiStorage
{
enum class ReadResult
{
    ok,
    missing,
    error
};

bool initialize();
ReadResult readFile(const char *name, std::string &contents);
bool writeFileAtomic(const char *name, const void *data, size_t size);
bool removeFile(const char *name);
}
