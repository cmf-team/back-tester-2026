// Unit tests for MappedFile: open/close, empty file, nonexistent, move
// semantics.

#include "market_data/MappedFile.hpp"

#include "TempFile.hpp"
#include "catch2/catch_all.hpp"

#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace cmf;

namespace {

void writeFile(const std::filesystem::path &p, const std::string &contents) {
  std::ofstream out(p, std::ios::binary);
  REQUIRE(out.is_open());
  out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
}

} // namespace

TEST_CASE("MappedFile - opens and mmaps a regular file", "[MappedFile]") {
  TempFile tf("mapped_file_basic.bin");
  const std::string body = "hello world\n";
  writeFile(tf.getPath(), body);

  MappedFile mf(tf.getPath());
  REQUIRE(mf.size() == body.size());
  REQUIRE(mf.data() != nullptr);
  REQUIRE(std::memcmp(mf.data(), body.data(), body.size()) == 0);
  REQUIRE(std::string{mf.view()} == body);
}

TEST_CASE("MappedFile - empty file yields null data and zero size",
          "[MappedFile]") {
  TempFile tf("mapped_file_empty.bin");
  writeFile(tf.getPath(), "");

  MappedFile mf(tf.getPath());
  REQUIRE(mf.size() == 0);
  REQUIRE(mf.data() == nullptr);
}

TEST_CASE("MappedFile - missing file throws", "[MappedFile]") {
  REQUIRE_THROWS_AS(
      MappedFile("/tmp/cmf_mapped_file_definitely_does_not_exist_98765.bin"),
      std::runtime_error);
}

TEST_CASE("MappedFile - move-construct transfers ownership", "[MappedFile]") {
  TempFile tf("mapped_file_move.bin");
  writeFile(tf.getPath(), "data");

  MappedFile a(tf.getPath());
  const char *orig_ptr = a.data();
  const std::size_t orig_size = a.size();

  MappedFile b(std::move(a));
  REQUIRE(b.data() == orig_ptr);
  REQUIRE(b.size() == orig_size);
  REQUIRE(a.data() == nullptr);
  REQUIRE(a.size() == 0);
}

TEST_CASE("MappedFile - move-assign releases old mapping", "[MappedFile]") {
  TempFile tf1("mapped_file_src1.bin");
  TempFile tf2("mapped_file_src2.bin");
  writeFile(tf1.getPath(), "aaaa");
  writeFile(tf2.getPath(), "bbbbbbbb");

  MappedFile a(tf1.getPath());
  MappedFile b(tf2.getPath());
  REQUIRE(a.size() == 4);
  REQUIRE(b.size() == 8);

  a = std::move(b);
  REQUIRE(a.size() == 8);
  REQUIRE(std::memcmp(a.data(), "bbbbbbbb", 8) == 0);
  REQUIRE(b.data() == nullptr);
  REQUIRE(b.size() == 0);
}
