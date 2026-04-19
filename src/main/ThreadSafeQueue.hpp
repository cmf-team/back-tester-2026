#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>

namespace cmf{
    template <typename T>
    class ThreadSafeQueue {

        private:
            std::queue<T> queue_;
            std::mutex mtx_;
            std::condition_variable cv_;
            bool finished_ = false;
    

        public:

            void push(const T& item) {
                
                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    queue_.push(item);
                }
                cv_.notify_one();

            }

            bool pop(T& item) {

                std::unique_lock<std::mutex> lock(mtx_);
                cv_.wait(lock, [&] {return !queue_.empty() || finished_;});
                if (queue_.empty()){
                    return false;
                }

                item = queue_.front();
                queue_.pop();
                return true;
            }

            void setDone() {
                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    finished_ = true;
                }
                cv_.notify_one();
            }
    };
} //namespace cmf