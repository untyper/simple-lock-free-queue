# API
```c++
// #include "mpsc_queue.h"
template <typename T> class MPSC_Queue // Example use: MPSC_Queue<int> queue;
```
```c++
// #include "mpmc_queue.h"
template <typename T> class MPMC_Queue // Example use: MPMC_Queue<int> queue;
```
```c++
template <typename U> void enqueue(U&& value) // Accepts any type by templates
```
```c++
std::unique_ptr<T> dequeue() // Returns nullptr if queue is empty, otherwise a pointer to the actual item
```
```c++
bool is_empty()
```
```c++
int size_approx() // The size will change during access in high concurrency apps, so an accurate size is not guaranteed
```

# Example
```c++
#include "mpsc_queue.h"
#include "mpmc_queue.h"

// Example
int main()
{
  // MPSC_Queue Usage Example
  MPSC_Queue<int> mpsc_queue;

  // Enqueue multiple items
  mpsc_queue.enqueue(1);
  mpsc_queue.enqueue(2);
  mpsc_queue.enqueue(3);

  // Check approximate size
  std::cout << "MPSC Queue Approximate Size: " << mpsc_queue.size_approx() << std::endl;

  // Dequeue items
  while (!mpsc_queue.is_empty())
  {
    auto item = mpsc_queue.dequeue();
    
    if (item)
    {
      std::cout << "MPSC Queue Dequeued: " << *item << std::endl;
    }
  }

  // MPMC_Queue Usage Example
  MPMC_Queue<int> mpmc_queue;

  // Enqueue items concurrently
  std::thread producer1([&]() {mpmc_queue.enqueue(1);});
  std::thread producer2([&]() {mpmc_queue.enqueue(2);});
  std::thread producer3([&]() {mpmc_queue.enqueue(3);});

  producer1.join();
  producer2.join();
  producer3.join();

  // Check approximate size
  std::cout << "MPMC Queue Approximate Size: " << mpmc_queue.size_approx() << std::endl;

  // Dequeue items concurrently
  std::thread consumer1([&]()
  {
    auto item = mpmc_queue.dequeue();
    
    if (item)
    {
      std::cout << "MPMC Queue Dequeued by Consumer 1: " << *item << std::endl;
    }
  });

  std::thread consumer2([&]()
  {
    auto item = mpmc_queue.dequeue();
    
    if (item)
    {
      std::cout << "MPMC Queue Dequeued by Consumer 2: " << *item << std::endl;
    }
  });

  std::thread consumer3([&]()
  {
    auto item = mpmc_queue.dequeue();
    
    if (item)
    {
      std::cout << "MPMC Queue Dequeued by Consumer 3: " << *item << std::endl;
    }
  });

  consumer1.join();
  consumer2.join();
  consumer3.join();

  return 0;
}
```

# Info
## **1. MPSC_Queue (Multi-Producer, Single-Consumer)**

### 1. **Structure and Functionality**

- **Nodes (`Node` Struct)**:
    - Each `Node` contains a `std::unique_ptr<T>` for data storage and an `std::atomic<Node*> next` pointer to link to the next node.
    - The use of `std::unique_ptr<T>` ensures that the data is safely managed and released when the node is deleted, supporting move-only types in the queue.
- **Queue Pointers**:
    - **`head`**: A standard pointer (`Node*`) to the front of the queue, from which items are dequeued.
    - **`tail`**: An atomic pointer (`std::atomic<Node*>`) that points to the last node in the queue, to which new items are enqueued.
- **Counter (`node_count`)**:
    - An atomic integer that tracks the number of items in the queue.
    - Updated during `enqueue` and `dequeue` operations to provide a size estimate.

### 2. **Enqueue and Dequeue Operations**

- **`enqueue`**:
    - Creates a new node with the given data and attempts to atomically update `tail` to point to this new node.
    - The previous `tail` node’s `next` pointer is updated to point to the new node.
    - After enqueuing, `node_count` is incremented.
    - **Concurrency**: This function is lock-free and allows multiple producers, making this queue a **multi-producer queue**.

- **`dequeue`**:
    - Attempts to read the next node from `head->next`.
    - If `next_node` is `nullptr`, the queue is empty, and `dequeue` returns `nullptr`.
    - If there’s a valid node, the `head` pointer is advanced to `next_node`, effectively dequeuing the item.
    - `node_count` is decremented after a successful dequeue.
    - **Concurrency**: This function is single-consumer because it modifies `head` directly without atomic updates, which makes this queue a **single-consumer queue**.

### 3. **Key Features**

- **Single Consumer, Multi-Producer**:
    - This queue is **single-consumer, multi-producer** (SCMP).
    - `enqueue` is lock-free and can support multiple concurrent producers.
    - `dequeue` can only safely be used by a single consumer.

- **Size Approximation**:
    - The `size()` method provides an approximate count of items in the queue, using the atomic counter `node_count`.
    - The counter itself is accurate, but due to possible interleaved operations in a multi-producer scenario, it may not always reflect the exact real-time size at any given moment.

- **Memory Management**:
    - The `clear_nodes` function traverses the queue from `head` and deletes each node, ensuring that no memory leaks occur.
    - The destructor calls `clear_nodes` to ensure all nodes are cleaned up when the queue is destroyed.

### 4. **Thread Safety and Memory Orderings**

- **Memory Orderings**:
    - `enqueue` uses `memory_order_acq_rel` on `tail.exchange` and `memory_order_release` on `prev_tail->next.store(new_node)`.
    - `dequeue` uses `memory_order_acquire` for loading `next_node`.
    - These memory orderings ensure visibility of updates across threads but do not block concurrent operations.

- **Lock-Free Operations**:
    - Both `enqueue` and `dequeue` are lock-free, meaning that each operation can proceed without blocking, but only `enqueue` supports multiple threads.

### 5. **Use Cases**

Given the characteristics of this queue, it would be suitable for:

- **Single Consumer, Multi-Producer Scenarios**:
    - Applications where multiple producers add tasks to a queue to be processed by a single consumer thread, such as work queues for background processing.
    - Systems where tasks are distributed from multiple threads to a single consumer (e.g., event logging, task queuing).

- **Approximate Size Tracking**:
    - Scenarios where an approximate size is sufficient (e.g., monitoring approximate queue load) since `node_count` is accurate in a general sense but not exact at any specific moment.

### 6. **Considerations and Potential Improvements**

- **Multiple Consumers**:
    - This queue does not support multiple consumers due to the non-atomic update of `head` in `dequeue`.
    - To support multiple consumers, `head` would need to be made atomic, and additional synchronization would be necessary for `dequeue`.

- **Size Accuracy**:
    - Although `node_count` is atomically incremented and decremented, it’s still approximate because concurrent `enqueue` and `dequeue` operations might slightly misrepresent the size in real-time.
    - For applications that require exact size tracking, an atomic counter may need to be combined with locking mechanisms, though this impacts performance.

---

## **2. MPMC_Queue (Multi-Producer, Multi-Consumer)**

### 1. **Structure and Functionality Analysis**

- **Nodes (`Node` Struct)**:
    - Each `Node` contains a `std::unique_ptr<T>` for storing data and an `std::atomic<Node*> next` pointer for linking to the next node.
    - The data is managed by `std::unique_ptr`, ensuring that memory is properly cleaned up when nodes are dequeued or when the queue is destroyed.
- **Queue Pointers**:
    - **`head`**: An atomic pointer (`std::atomic<Node*>`) to the front of the queue, from which items are dequeued.
    - **`tail`**: An atomic pointer (`std::atomic<Node*>`) to the end of the queue, where new items are enqueued.
- **Node Cleanup**:
    - The `clean_nodes` function traverses the queue from the `head`, deleting each node in sequence.
    - The destructor calls `clean_nodes` to ensure all nodes are deleted when the queue is destroyed, preventing memory leaks.

### 2. **Enqueue and Dequeue Operations**

- **`enqueue`**:
    - Creates a new node, then atomically updates `tail` to point to this new node.
    - The `next` pointer of the previous `tail` node is set to point to the new node.
    - The use of atomic operations with `memory_order_acq_rel` and `memory_order_release` ensures visibility across threads.
    - **Concurrency**: This function is lock-free and supports multiple producers, making this queue a **multi-producer queue**.

- **`dequeue`**:
    - Loads `head` and attempts to read the `next` node.
    - If `next_node` is `nullptr`, it returns `nullptr`, indicating the queue is empty.
    - If a valid `next_node` exists, it attempts to atomically update `head` to point to `next_node`, effectively dequeuing the item.
    - **Concurrency**: This function supports multiple consumers and is lock-free, making this a **multi-consumer queue**.

### 3. **Key Features**

- **Multi-Consumer, Multi-Producer (MPMC)**:
    - This queue supports multiple producers and multiple consumers, making it an **MPMC queue**.
    - Both `enqueue` and `dequeue` are designed to handle concurrent access by multiple threads without locks.
  
- **Approximate Size Tracking**:
    - The `size_approx` method traverses the queue from `head` to `tail`, counting nodes along the way.
    - This method provides an approximate size, as concurrent modifications can make the count inaccurate in real-time.

- **Memory Management**:
    - Each `Node` is managed by a `std::unique_ptr`, ensuring that memory is properly cleaned up when nodes are dequeued or when the queue is destroyed.

### 4. **Thread Safety and Memory Orderings**

- **Memory Orderings**:
    - `enqueue` uses `memory_order_acq_rel` on `tail.exchange` and `memory_order_release` on `prev_tail->next.store(new_node)`.
    - `dequeue` uses `memory_order_acquire` to load `head` and `next_node`.
    - These memory orderings ensure that each thread has a consistent view of the queue’s structure without blocking other threads.

- **Lock-Free Operation**:
    - Both `enqueue` and `dequeue` are lock-free, meaning that each operation can proceed without waiting for other operations to complete.

### 5. **Use Cases**

Given its characteristics, this queue would be suitable for:

- **High-Concurrency Environments**:
    - Applications where multiple producers and consumers need to access the queue concurrently, such as job dispatchers or event-driven systems.
    - Situations where minimal contention and high throughput are desired.

- **Task Distribution Among Multiple Consumers**:
    - The queue is well-suited for systems where tasks are dynamically generated and need to be processed by multiple worker threads, such as thread pools in server applications.

### 6. **Additional Functionality**

- **`is_empty` Method**:
    - Provides a quick way to check if the queue is empty without modifying its state.
    - Uses `memory_order_acquire` to load `head` and inspect `next_node`, returning `true` if `next_node` is `nullptr`.
- **Approximate `size_approx` Method**:
    - This method approximates the queue size by counting nodes from `head` to `tail`.
    - It’s not fully thread-safe, as concurrent modifications could alter the structure of the queue during counting, leading to an approximate value rather than an exact count.

# Licence
- MIT
