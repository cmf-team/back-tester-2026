// HW-2 driver:
//
//   back-tester2 [options] <input>
//
//   <input>                  path to a single NDJSON file or a directory of
//                            NDJSON files
//   --merger=flat|hierarchy  k-way merge strategy (default: flat)
//   --workers=N              0 == sequential dispatcher (default)
//                            >0 == sharded multi-threaded dispatcher
//   --snapshot-every=N       emit a snapshot every N events (default: 0)
//   --snapshot-depth=D       depth per side in each snapshot (default: 5)
//   --queue-cap=N            per-producer back-pressure queue capacity (4096)
//   --quiet                  suppress per-event banner
//
// All policy lives in PipelineConfig; this file is just argv munging.

#include "main2/PipelineApp.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void usage(std::ostream &os, const char *prog) {
  os << "usage: " << prog << " [options] <input>\n"
        "  --merger=flat|hierarchy   default: flat\n"
        "  --workers=N               default: 0 (sequential)\n"
        "  --snapshot-every=N        default: 0 (disabled)\n"
        "  --snapshot-depth=D        default: 5\n"
        "  --queue-cap=N             default: 4096\n"
        "  --quiet                   suppress banner\n";
}

bool consumeKV(std::string_view arg, std::string_view key,
               std::string_view &out) {
  if (arg.substr(0, key.size()) != key) return false;
  if (arg.size() == key.size())         return false;
  if (arg[key.size()] != '=')           return false;
  out = arg.substr(key.size() + 1);
  return true;
}

} // namespace

int main(int argc, const char *argv[]) {
  try {
    cmf::PipelineConfig cfg;
    bool got_input = false;
    for (int i = 1; i < argc; ++i) {
      std::string_view a = argv[i];
      std::string_view v;
      if (a == "-h" || a == "--help") {
        usage(std::cout, argv[0]);
        return 0;
      } else if (a == "--quiet") {
        cfg.quiet = true;
      } else if (consumeKV(a, "--merger", v)) {
        if (v == "flat")
          cfg.merger = cmf::MergerKind::Flat;
        else if (v == "hierarchy")
          cfg.merger = cmf::MergerKind::Hierarchy;
        else
          throw std::runtime_error("--merger expects flat|hierarchy");
      } else if (consumeKV(a, "--workers", v)) {
        cfg.workers = std::stoul(std::string(v));
      } else if (consumeKV(a, "--snapshot-every", v)) {
        cfg.snapshot_every = std::stoull(std::string(v));
      } else if (consumeKV(a, "--snapshot-depth", v)) {
        cfg.snapshot_depth = std::stoul(std::string(v));
      } else if (consumeKV(a, "--queue-cap", v)) {
        cfg.queue_capacity = std::stoul(std::string(v));
      } else if (!a.empty() && a[0] == '-') {
        throw std::runtime_error("unknown option: " + std::string(a));
      } else {
        cfg.input  = std::string(a);
        got_input  = true;
      }
    }
    if (!got_input) {
      usage(std::cerr, argv[0]);
      return 2;
    }

    cmf::PipelineApp app(std::move(cfg));
    app.run(std::cout, std::cerr);
  } catch (const std::exception &ex) {
    std::cerr << "back-tester2: " << ex.what() << '\n';
    return 1;
  }
  return 0;
}
