#include "TestSupport.hpp"

#include "concurrency/NonBlockingQueue.hpp"

namespace md::test {

void testNonBlockingQueue() {
    NonBlockingQueue<int> queue(2);

    require(queue.size() == 0, "non-blocking queue starts empty");
    queue.push(1);
    queue.push(2);
    queue.flush();
    require(queue.size() >= 2, "non-blocking queue reports queued data");
    require(queue.pop() == 1, "non-blocking queue pops first value");
    queue.push(3);
    queue.flush();
    require(queue.pop() == 2, "non-blocking queue pops second value");
    require(queue.pop() == 3, "non-blocking queue pops wrapped value");
}

} // namespace md::test
