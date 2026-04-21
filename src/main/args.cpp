#include "args.hpp"

constexpr std::string DATA_FILE_SUFFIX = ".mbo.json";

cmf::Config cmf::parse_args(const std::span<const char *> args) {
  auto print_usage_hint = [&]() {
    std::fprintf(stderr, "Usage: %s <file.mbo.json | folder>\n", args[0]);
  };

  if (args.size() != 2) {
    print_usage_hint();

    throw std::runtime_error("Invalid command line arguments");
  }

  Config cfg;
  if (const std::filesystem::path source = args[1];
      std::filesystem::is_directory(source)) {
    std::vector<std::filesystem::path> data_files;
    for (auto &entry : std::filesystem::directory_iterator(source)) {
      const auto &path = entry.path();
      if (!std::filesystem::is_regular_file(path) ||
          !path.string().ends_with(DATA_FILE_SUFFIX)) {
        continue;
      }

      data_files.push_back(path);
    }
    if (data_files.empty()) {
      print_usage_hint();

      throw std::runtime_error("Data files not found");
    }

    cfg.data_files = std::move(data_files);
  } else if (std::filesystem::is_regular_file(source)) {
    if (!source.string().ends_with(DATA_FILE_SUFFIX)) {
      print_usage_hint();

      throw std::runtime_error("Invalid file extension");
    }

    cfg.data_files = {source};
  }

  return cfg;
}