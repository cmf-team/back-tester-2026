#include "data_layer/JsonMarketDataFolderLoader.hpp"
#include <filesystem>
#include <stdexcept>
#include <string>

namespace data_layer {

std::vector<std::string> discoverJsonFiles(const std::string &folder_path) {
  namespace fs = std::filesystem;

  if (!fs::exists(folder_path) || !fs::is_directory(folder_path)) {
    throw std::runtime_error("Invalid input folder: " + folder_path);
  }

  std::vector<std::string> json_files;
  for (const auto &entry : fs::directory_iterator(folder_path)) {
    if (entry.is_regular_file() &&
        entry.path().filename().string().ends_with(".mbo.json")) {
      json_files.push_back(entry.path().string());
    }
  }
  return json_files;
}

} 
