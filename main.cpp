#include "queues_processor.hpp"

int main() {
  auto consumer =
      std::make_shared<queues_processor::Consumer<int, int>>([](const int&, const int&) {});
  queues_processor::QueuesProcessor<int, int> proc;
  proc.Subscribe(1, consumer);
  proc.Enqueue(1, 1);
  proc.Dequeue(1);
  proc.Unsubscribe(1);
}
