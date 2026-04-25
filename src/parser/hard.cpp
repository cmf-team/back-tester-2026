#include "ParserUtils.hpp"
#include "common/BasicTypes.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using MarketDataEvent = cmf::MarketDataEvent;

std::vector<std::vector<MarketDataEvent>> Events(30);
std::vector<MarketDataEvent> merged_events1, merged_events2;

void processMarketDataEvent(const MarketDataEvent &order) { return; }
// --- Shared state ---
std::queue<std::string> fileQueue;
std::mutex queueMutex;
std::condition_variable cv;
std::atomic<bool> done{false};

int global_file_id = 0;
// --- Consumer ---
void consumer(int id) {
  while (true) {
    std::string file;
    int file_id;

    {
      std::unique_lock<std::mutex> lock(queueMutex);

      // Sleep until there's work OR we're done
      cv.wait(lock, [] { return !fileQueue.empty() || done.load(); });

      if (fileQueue.empty() && done.load())
        return; // No more work, exit

      file = fileQueue.front();
      fileQueue.pop();
      file_id = global_file_id;
      global_file_id++;
    } // Lock released here — process file without holding mutex

    cmf::parseNdjsonFile(file, Events[file_id]);
  }
}

// --- Producer ---
void producer(const std::vector<std::string> &files) {
  for (const auto &f : files) {
    {
      std::lock_guard<std::mutex> lock(queueMutex);
      fileQueue.push(f);
    }
    cv.notify_one(); // Wake one consumer per file added
  }

  done = true;
  cv.notify_all(); // Wake all consumers so they can check done and exit
}

void merge2arrays(std::vector<MarketDataEvent> &nums1,
                  std::vector<MarketDataEvent> &nums2) {
  int n = (int)nums1.size(), m = (int)nums2.size();
  nums1.resize(n + m);

  int i = n - 1, j = m - 1;

  for (int k = n + m - 1; k >= 0; --k) {
    if (i == -1) {
      nums1[k] = nums2[j];
      j--;
    } else if (j == -1) {
      nums1[k] = nums1[i];
      i--;
    } else {
      if (nums1[i].ts_event > nums2[j].ts_event) {
        nums1[k] = nums1[i];
        i--;
      } else {
        nums1[k] = nums2[j];
        j--;
      }
    }
  }
  nums2.clear();
}

int main() {
  // path to folder
  std::string folderPath;
  std::cin >> folderPath;
  auto start = std::chrono::steady_clock::now();

  // reading folder
  std::vector<std::string> files;
  for (const auto &file : std::filesystem::directory_iterator(folderPath)) {
    files.push_back(file.path().string());
  }
  const int num_files = (int)files.size();

  // threads logic
  std::vector<std::thread> consumers;
  const int num_consumers = std::thread::hardware_concurrency();
  for (int i = 0; i < num_consumers; ++i) {
    consumers.emplace_back(consumer, i);
  }

  producer(files);

  for (auto &t : consumers) {
    t.join();
  }

  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed1 = end - start;
  std::cout << "Total parsing time: " << elapsed1 << " seconds" << std::endl;
  // flat merger

  start = std::chrono::steady_clock::now();
  // timestamp, file_id, line
  using T = std::pair<uint64_t, std::pair<int, int>>;

  std::priority_queue<T, std::vector<T>, std::greater<T>> cur_min;

  std::vector<int> line(num_files, 0);
  for (int i = 0; i < num_files; ++i) {
    cur_min.push(std::make_pair(Events[i][line[i]].ts_event,
                                std::make_pair(i, line[i])));
  }

  while (!cur_min.empty()) {
    T cur = cur_min.top();
    cur_min.pop();
    int i = cur.second.first;

    merged_events1.push_back(Events[i][line[i]]);
    line[i]++;
    if (line[i] < (int)Events[i].size()) {
      cur_min.push(std::make_pair(Events[i][line[i]].ts_event,
                                  std::make_pair(i, line[i])));
    }
  }

  for (int i = 0; i < (int)merged_events1.size(); ++i) {
    processMarketDataEvent(merged_events1[i]);
  }
  end = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed2 = end - start;

  std::cout << "Total: " << (int)merged_events1.size() << " messages"
            << std::endl;
  std::cout << "Wall-clock time for merging: " << elapsed2.count() << " seconds"
            << std::endl;
  std::cout << "(merging) Throughput: "
            << (int)merged_events1.size() / elapsed2.count()
            << " messages per second" << std::endl;
  /*
// hierarchy merger
start = std::chrono::steady_clock::now();

std::queue <int> q_merge;
for(int i = 0; i < num_files; ++i) {
  q_merge.push(i);
}

while((int) q_merge.size() != 1) {
  int num1 = q_merge.front();
  q_merge.pop();
  int num2 = q_merge.front();
  q_merge.pop();
  merge2arrays(Events[num1], Events[num2]);
  q_merge.push(num1);
}
int num = q_merge.front();
merged_events2 = Events[num];

end = std::chrono::steady_clock::now();
elapsed2 = end - start;
std::cout << "Total: " << (int) merged_events2.size() << " messages" <<
std::endl; std::cout << "Wall-clock time for merging: " << elapsed2.count() << "
seconds" << std::endl; std::cout << "(merging) Throughput: " << (int)
merged_events2.size() / elapsed2.count() << " messages per second" << std::endl;
*/
}
