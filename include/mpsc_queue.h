// Copyright (c) [2024] [Jovan J. E. Odassius]
//
// License: MIT (See the LICENSE file in the root directory)
// Github: https://github.com/untyper/simple-lock-free-queue

// Lock-free Multi-Producer Single-Consumer queue

template <typename T>
class MPSC_Queue
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

  std::atomic<Node*> tail;
  Node* head;

  // For cleaning up any existing nodes in the current queue upon exiting
  void clean_nodes()
  {
    Node* current = this->head;

    while (current)
    {
      Node* next_node = current->next.load(std::memory_order_relaxed);
      delete current;
      current = next_node;
    }
  }

public:
  // Approximate size function, with basic thread safety
  int size_approx() const
  {
    int count = 0;
    Node* current = this->head;
    Node* tail_node = this->tail.load(std::memory_order_acquire);

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

  // Enqueue function accepting both copyable and movable types
  template <typename U>
  void enqueue(U&& value)
  {
    Node* new_node = new Node(std::forward<U>(value));
    Node* prev_tail = this->tail.exchange(new_node, std::memory_order_acq_rel);
    prev_tail->next.store(new_node, std::memory_order_release);
  }

  // Consumer calls this to dequeue data
  std::unique_ptr<T> dequeue()
  {
    Node* next_node = this->head->next.load(std::memory_order_acquire);

    if (!next_node)
    {
      return nullptr; // Queue is empty
    }

    std::unique_ptr<T> result = std::move(next_node->data);
    delete this->head;
    this->head = next_node;

    return result;
  }

  // Check if the queue is empty without dequeuing
  bool is_empty() const
  {
    Node* current_head = this->head;
    Node* next_node = current_head->next.load(std::memory_order_acquire);
    return next_node == nullptr;
  }

  // Disable copy assignment operator
  MPSC_Queue& operator=(const MPSC_Queue&) = delete;

  // Move assignment operator
  MPSC_Queue& operator=(MPSC_Queue&& other) noexcept
  {
    if (this != &other)
    {
      this->clean_nodes();

      // Transfer ownership of nodes
      this->tail.store(other.tail.load(std::memory_order_relaxed), std::memory_order_relaxed);
      this->head = other.head;

      // Reset other to indicate moved-from state
      other.tail.store(nullptr, std::memory_order_relaxed);
      other.head = nullptr;
    }

    return *this;
  }

  // Disable copy constructor
  MPSC_Queue(const MPSC_Queue&) = delete;

  // Move constructor
  MPSC_Queue(MPSC_Queue&& other) noexcept :
    tail(other.tail.load(std::memory_order_relaxed)),
    head(other.head)
  {
    other.tail.store(nullptr, std::memory_order_relaxed);
    other.head = nullptr;
  }

  MPSC_Queue()
  {
    // Create a dummy node to simplify enqueue/dequeue logic
    Node* dummy = new Node();
    this->head = dummy;
    this->tail.store(dummy, std::memory_order_relaxed);
  }

  ~MPSC_Queue()
  {
    this->clean_nodes();
  }
};
