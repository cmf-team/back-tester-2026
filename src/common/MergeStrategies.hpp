#pragma once

#include "MarketDataEvent.hpp"
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <queue>
#include <string>
#include <vector>

namespace cmf {
namespace EventParser {

// Base class for event sources
class EventSource {
protected:
  std::ifstream file_;
  bool has_next_ = false;
  MarketDataEvent next_event_;

public:
  virtual ~EventSource() = default;

  virtual bool hasNext() const { return has_next_; }
  virtual const MarketDataEvent &peekNext() const { return next_event_; }
  virtual void advance() = 0;
};

// Single file event source
class FileEventSource : public EventSource {
public:
  explicit FileEventSource(const std::string &filepath) {
    file_.open(filepath);
    if (!file_.is_open()) {
      throw std::runtime_error("Failed to open file: " + filepath);
    }
    advance();
  }

  void advance() override {
    std::string line;
    has_next_ = false;

    while (std::getline(file_, line)) {
      if (line.empty()) {
        continue;
      }

      try {
        auto json = nlohmann::json::parse(line);
        next_event_ = MarketDataEvent::fromJson(json);
        has_next_ = true;
        break;
      } catch (const std::exception &ex) {
        // Skip invalid lines silently
        continue;
      }
    }
  }
};

// Comparison functor for priority queue (min-heap based on timestamp)
struct EventComparator {
  bool operator()(const std::pair<MarketDataEvent, std::shared_ptr<EventSource>>
                      &a,
                  const std::pair<MarketDataEvent, std::shared_ptr<EventSource>>
                      &b) const {
    return a.first.timestamp > b.first.timestamp; // min-heap
  }
};

// Merged event source (for hierarchical merging)
class MergedEventSource : public EventSource {
private:
  std::priority_queue<
      std::pair<MarketDataEvent, std::shared_ptr<EventSource>>,
      std::vector<std::pair<MarketDataEvent, std::shared_ptr<EventSource>>>,
      EventComparator>
      pq_;

public:
  explicit MergedEventSource(
      std::vector<std::shared_ptr<EventSource>> sources) {
    for (auto &source : sources) {
      if (source->hasNext()) {
        pq_.push({source->peekNext(), source});
      }
    }
    advance();
  }

  void advance() override {
    has_next_ = false;

    if (!pq_.empty()) {
      auto [event, source] = pq_.top();
      pq_.pop();

      next_event_ = event;
      has_next_ = true;

      source->advance();
      if (source->hasNext()) {
        pq_.push({source->peekNext(), source});
      }
    }
  }
};

// Base class for merge strategies
class MergeStrategy {
public:
  virtual ~MergeStrategy() = default;
  virtual bool hasNext() const = 0;
  virtual MarketDataEvent getNext() = 0;
  virtual const char *getName() const = 0;
};

// Strategy 1: Flat Merger (k-way merge using single priority queue)
template <typename Comparator = EventComparator>
class FlatMerger : public MergeStrategy {
private:
  std::priority_queue<
      std::pair<MarketDataEvent, std::shared_ptr<EventSource>>,
      std::vector<std::pair<MarketDataEvent, std::shared_ptr<EventSource>>>,
      Comparator>
      pq_;

public:
  explicit FlatMerger(const std::vector<std::string> &filepaths) {
    for (const auto &filepath : filepaths) {
      try {
        auto source = std::make_shared<FileEventSource>(filepath);
        if (source->hasNext()) {
          pq_.push({source->peekNext(), source});
        }
      } catch (const std::exception &) {
        // Skip files that can't be opened
      }
    }
  }

  bool hasNext() const override { return !pq_.empty(); }

  MarketDataEvent getNext() override {
    if (pq_.empty()) {
      throw std::runtime_error("No more events");
    }

    auto [event, source] = pq_.top();
    pq_.pop();

    source->advance();
    if (source->hasNext()) {
      pq_.push({source->peekNext(), source});
    }

    return event;
  }

  const char *getName() const override { return "Flat Merger"; }
};

// Strategy 2: Hierarchy Merger (tree-based merge)
template <typename Comparator = EventComparator>
class HierarchyMerger : public MergeStrategy {
private:
  std::shared_ptr<EventSource> root_;

  std::shared_ptr<EventSource>
  buildTree(std::vector<std::shared_ptr<EventSource>> sources) {
    if (sources.empty()) {
      return nullptr;
    }
    if (sources.size() == 1) {
      return sources[0];
    }

    // Merge in pairs
    std::vector<std::shared_ptr<EventSource>> next_level;
    for (size_t i = 0; i < sources.size(); i += 2) {
      if (i + 1 < sources.size()) {
        std::vector<std::shared_ptr<EventSource>> pair = {sources[i],
                                                           sources[i + 1]};
        next_level.push_back(std::make_shared<MergedEventSource>(pair));
      } else {
        next_level.push_back(sources[i]);
      }
    }

    return buildTree(next_level);
  }

public:
  explicit HierarchyMerger(const std::vector<std::string> &filepaths) {
    std::vector<std::shared_ptr<EventSource>> sources;
    for (const auto &filepath : filepaths) {
      try {
        sources.push_back(std::make_shared<FileEventSource>(filepath));
      } catch (const std::exception &) {
        // Skip files that can't be opened
      }
    }
    root_ = buildTree(sources);
  }

  bool hasNext() const override { return root_ && root_->hasNext(); }

  MarketDataEvent getNext() override {
    if (!root_ || !root_->hasNext()) {
      throw std::runtime_error("No more events");
    }

    MarketDataEvent event = root_->peekNext();
    root_->advance();
    return event;
  }

  const char *getName() const override { return "Hierarchy Merger"; }
};

} // namespace EventParser
} // namespace cmf
