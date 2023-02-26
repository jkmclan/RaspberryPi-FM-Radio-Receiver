#include <condition_variable>
#include <mutex>
#include <queue>
#include <chrono>

using namespace std::chrono_literals;
  
// Thread-safe queue
template <typename T>
class QueueThreadSafe {

public:

    QueueThreadSafe(volatile int& do_exit) :
        m_do_exit(do_exit)
    { }

    ~QueueThreadSafe()
    { }

    // Pushes an element to the queue
    void push(T item) {

        // Acquire lock
        std::unique_lock<std::mutex> lock(m_mutex);

        // Add item
        m_queue.push(item);

        // Notify one thread that
        // is waiting
        m_cond.notify_one();
    }

  
    // Pops an element off the queue
    T pop() {

        // acquire lock
        std::unique_lock<std::mutex> lock(m_mutex);

        while (m_do_exit == 0) {

            // wait until queue is not empty or a timeout occurs
            bool stat = m_cond.wait_for( lock, 10*100ms, [this]() { return !m_queue.empty(); } );

            if (stat == false) {

                // A timeout occured
                //printf("QueueThreadSafe.pop : wait timeout\n");

                continue;
            }

            // retrieve item
            T item = m_queue.front();
            m_queue.pop();

            // return item
            return item;
        }
        printf("QueueThreadSafe.pop : exiting\n");

        T item;
        return item;
    }

private:

    // Termination flag
    volatile int& m_do_exit;

    // Underlying queue
    std::queue<T> m_queue;
  
    // mutex for thread synchronization
    std::mutex m_mutex;
  
    // Condition variable for signaling
    std::condition_variable m_cond;
};

