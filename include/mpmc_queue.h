// Copyright (c) [2024] [Jovan J. E. Odassius]
//
// License: MIT (See the LICENSE file in the root directory)
// Github: https://github.com/untyper/simple-lock-free-queue

// Lock-free (Multi-Producer, Multi-Consumer) queue

template <typename T>
class MPMC_Queue
{
private:
  struct Node
  {
    std::unique_ptr<T> data;
    std::atomic<Node*> next;

    Node() : data(nullptr), next(nullptr) {}

    template <typename U>
    explicit Node(U&& value)
      : data(std::make_unique<T>(std::forward<U>(value))), next(nullptr) {}
  };

  std::atomic<Node*> head;
  std::atomic<Node*> tail;

  // For cleaning up any existing nodes in the current queue upon exiting
  void clean_nodes()
  {
    Node* node = head.load(std::memory_order_relaxed);

    while (node)
    {
      Node* next = node->next.load(std::memory_order_relaxed);
      delete node;
      node = next;
    }
  }

public:
  // Enqueue function accepting both copyable and movable types
  template <typename U>
  void enqueue(U&& value)
  {
    Node* new_node = new Node(std::forward<U>(value));
    new_node->next.store(nullptr, std::memory_order_relaxed);

    Node* prev_tail = tail.exchange(new_node, std::memory_order_acq_rel);
    prev_tail->next.store(new_node, std::memory_order_release);
  }

  // Dequeue function for multiple consumers
  std::unique_ptr<T> dequeue()
  {
    while (true)
    {
      Node* old_head = head.load(std::memory_order_acquire);
      Node* next_node = old_head->next.load(std::memory_order_acquire);

      if (next_node == nullptr)
      {
        return nullptr; // Queue is empty
      }

      // Attempt to move the head pointer forward
      if (head.compare_exchange_weak(
        old_head, next_node, std::memory_order_acq_rel, std::memory_order_acquire))
      {
        std::unique_ptr<T> result = std::move(next_node->data);
        delete old_head;
        return result;
      }

      // If the CAS failed, another consumer may have dequeued the item; retry
    }
  }

  // Check if the queue is empty without dequeuing
  bool is_empty() const
  {
    Node* current_head = head.load(std::memory_order_acquire);
    Node* next_node = current_head->next.load(std::memory_order_acquire);
    return next_node == nullptr;
  }

  // Approximate size function, with basic thread safety
  int size_approx() const
  {
    int count = 0;
    Node* current = head.load(std::memory_order_acquire);
    Node* tail_node = tail.load(std::memory_order_acquire);

    // Traverse nodes, counting until reaching the tail
    while (current != tail_node)
    {
      Node* next = current->next.load(std::memory_order_acquire);
      if (next == nullptr) break; // Stop if we reach a null (avoid possible inconsistency)
      current = next;
      ++count;
    }

    return count;
  }

  // Move assignment operator
  MPMC_Queue& operator=(MPMC_Queue&& other) noexcept
  {
    if (this != &other)
    {
      // Clean up current nodes
      this->clean_nodes();

      // Move the pointers
      head.store(other.head.load(std::memory_order_relaxed), std::memory_order_relaxed);
      tail.store(other.tail.load(std::memory_order_relaxed), std::memory_order_relaxed);
      other.head.store(nullptr, std::memory_order_relaxed);
      other.tail.store(nullptr, std::memory_order_relaxed);
    }

    return *this;
  }

  // Disable copy assignment operator
  MPMC_Queue& operator=(const MPMC_Queue&) = delete;

  // Move constructor
  MPMC_Queue(MPMC_Queue&& other) noexcept
  {
    head.store(other.head.load(std::memory_order_relaxed), std::memory_order_relaxed);
    tail.store(other.tail.load(std::memory_order_relaxed), std::memory_order_relaxed);
    other.head.store(nullptr, std::memory_order_relaxed);
    other.tail.store(nullptr, std::memory_order_relaxed);
  }

  // Disable copy constructor
  MPMC_Queue(const MPMC_Queue&) = delete;

  MPMC_Queue()
  {
    // Create a dummy node to simplify enqueue/dequeue logic
    Node* dummy = new Node();
    head.store(dummy, std::memory_order_relaxed);
    tail.store(dummy, std::memory_order_relaxed);
  }

  ~MPMC_Queue()
  {
    // Clean up any remaining nodes
    this->clean_nodes();
  }
};
