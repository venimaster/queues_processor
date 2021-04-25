#pragma once

#include <condition_variable>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <unordered_map>

namespace queues_processor {

template <typename Key, typename Value>
using Consumer = std::function<void(const Key&, const Value&)>;

constexpr auto MaxCapacity = 1000;

template <typename Key, typename Value>
class QueuesProcessor {
 public:
  QueuesProcessor() : th_{std::bind(&QueuesProcessor::Process, this)} {}

  ~QueuesProcessor() {
    StopProcessing();
    th_.join();
  }

  void Subscribe(const Key& key,
                 std::shared_ptr<Consumer<Key, Value>> consumer) {
    if (!(consumer && *consumer)) {
      return;
    }
    std::lock_guard<std::mutex> lock{mtx_};
    auto subscription_it = subscriptions_.find(key);

    if (subscription_it == cend(subscriptions_)) {
      subscriptions_.emplace(key,
                             Subscription{consumer, {}, end(process_list_)});
    } else if (!subscription_it->second.consumer) {
      auto& subscription = subscription_it->second;
      subscription.consumer = consumer;
      if (!subscription.queue.empty()) {
        subscription.process_it =
            process_list_.insert(cend(process_list_), key);
        cond_.notify_one();
      }
    }
  }

  void Unsubscribe(const Key& key) {
    std::lock_guard<std::mutex> lock{mtx_};
    if (auto subscription_it = subscriptions_.find(key);
        subscription_it != cend(subscriptions_)) {
      auto& subscription = subscription_it->second;
      if (subscription.process_it != cend(process_list_)) {
        process_list_.erase(subscription.process_it);
      }
      if (subscription.queue.empty()) {
        subscriptions_.erase(subscription_it);
      } else {
        subscription.process_it = end(process_list_);
      }
    }
  }

  void Enqueue(const Key& key, Value&& value) {
    std::lock_guard<std::mutex> lock{mtx_};
    auto subscription_it = subscriptions_.find(key);
    if (subscription_it == cend(subscriptions_)) {
      subscription_it =
          subscriptions_
              .emplace(key, Subscription{nullptr, {}, end(process_list_)})
              .first;
    }
    auto& subscription = subscription_it->second;
    if (subscription.queue.size() < MaxCapacity) {
      subscription.queue.push(std::move(value));
      if (subscription.consumer &&
          subscription.process_it == cend(process_list_)) {
        subscription.process_it =
            process_list_.insert(cend(process_list_), key);
        cond_.notify_one();
      }
    }
  }

  std::optional<Value> Dequeue(const Key& key) {
    std::lock_guard<std::mutex> lock{mtx_};
    return DequeueImpl(key);
  }

 private:
  struct Subscription {
    std::shared_ptr<Consumer<Key, Value>> consumer;
    std::queue<Value> queue;
    typename std::list<Key>::iterator process_it;
  };

  void Process() {
    while (true) {
      std::unique_lock<std::mutex> lock{mtx_};
      cond_.wait(lock,
                 [this]() { return !(process_list_.empty() && running_); });
      while (running_ && !process_list_.empty()) {
        Key key{std::move(process_list_.front())};
        auto& subscription = subscriptions_.at(key);
        subscription.process_it = end(process_list_);
        auto consumer = subscription.consumer;
        auto value = DequeueImpl(key).value();
        process_list_.pop_front();
        lock.unlock();
        (*consumer)(key, value);
        lock.lock();
        auto subscription_it = subscriptions_.find(key);
        if (subscription_it != cend(subscriptions_) &&
            subscription_it->second.process_it == end(process_list_)) {
          subscription_it->second.process_it =
              process_list_.insert(cend(process_list_), key);
        }
      }
      if (!running_) {
        return;
      }
    }
  }

  std::optional<Value> DequeueImpl(const Key& key) {
    auto subscription_it = subscriptions_.find(key);
    if (subscription_it != end(subscriptions_) &&
        !subscription_it->second.queue.empty()) {
      auto& subscription = subscription_it->second;
      auto& queue = subscription.queue;
      Value value{std::move(queue.front())};
      queue.pop();
      if (queue.empty()) {
        if (subscription.process_it != cend(process_list_)) {
          process_list_.erase(subscription.process_it);
        }
        if (!subscription.consumer) {
          subscriptions_.erase(subscription_it);
        } else {
          subscription.process_it = end(process_list_);
        }
      }
      return std::move(value);
    }
    return std::nullopt;
  }

  void StopProcessing() {
    std::lock_guard<std::mutex> lock{mtx_};
    running_ = false;
    cond_.notify_one();
  }

  std::unordered_map<Key, Subscription> subscriptions_;
  std::list<Key> process_list_;
  std::condition_variable cond_;
  std::mutex mtx_;
  std::thread th_;
  bool running_ = true;
};

}  // namespace queues_processor
