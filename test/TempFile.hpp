// helper class to maintain temporary file

#pragma once

#include <filesystem>
#include <fstream>
#include <string>

namespace cmf {

class TempFile
{
private:
    std::filesystem::path filename_;
    std::filesystem::path path_;

public:
    template <class File>
    TempFile(const File& filename) :
        filename_{filename},
        path_{std::filesystem::temp_directory_path() / filename}
    {
        // Remove any existing file
        if (std::filesystem::exists(path_))
            std::filesystem::remove(path_);
    }

    ~TempFile()
    {
        std::filesystem::remove(path_);
    }

    std::filesystem::path getFilename() const
    {
        return filename_;
    }

    std::filesystem::path getPath() const
    {
        return path_;
    }

    std::string getContents() const
    {
        std::string contents, line;
        std::ifstream fs(path_);

        bool first = true;
        while (std::getline(fs, line))
        {
            if (first)
                first = false;
            else
                contents += "\n";

            contents += line;
        }
        return contents;
    }
};

}
