#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

inline std::vector<std::uint8_t> ReadCSO(std::string_view filename)
{
    const std::string path{filename};
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file: " + path);
    }

    const auto end = file.tellg();
    if (end <= 0)
    {
        throw std::runtime_error("File is empty: " + path);
    }

    const auto file_size = static_cast<std::size_t>(end);
    std::vector<std::uint8_t> buffer(file_size);
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(file_size)))
    {
        throw std::runtime_error("Failed to read file: " + path);
    }

    return buffer;
}
