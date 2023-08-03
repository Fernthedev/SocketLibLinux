#pragma once

#include <queue>
#include <cstdint>
#include <span>
#include "Message.hpp"

namespace SocketLib {

    struct ReadOnlyStreamQueue {

        ReadOnlyStreamQueue() = default;

        ReadOnlyStreamQueue(std::span<uint8_t> bytes) : bytes(bytes.begin(), bytes.end()) {};

        ReadOnlyStreamQueue(ReadOnlyStreamQueue &&) noexcept = default;

        explicit ReadOnlyStreamQueue(ReadOnlyStreamQueue const &) = default;

        virtual ~ReadOnlyStreamQueue() = default;

        std::deque<uint8_t> dequeue(std::size_t maxCount = std::numeric_limits<std::size_t>::max()) {
            // Determine the actual number of elements to dequeue
            std::size_t countToDequeue = std::min(maxCount, bytes.size());

            // Move elements from the internal deque to the new queue using bulk move
            std::deque<uint8_t> newDeque(std::make_move_iterator(bytes.begin()),
                                         std::make_move_iterator(bytes.begin() + countToDequeue));

            // Erase the moved elements from the internal deque
            bytes.erase(bytes.begin(), bytes.begin() + countToDequeue);

            return newDeque;

//            for (uint8_t i = 0; i < maxCount && !bytes.empty(); ++i) {
//                newQueue.push(bytes.front());
//                bytes.pop();
//            }
//
//            return newQueue;
        }

        std::vector<uint8_t> dequeueAsVec(std::size_t maxCount = std::numeric_limits<std::size_t>::max()) {
//            // Determine the actual number of elements to dequeue
            std::size_t countToDequeue = std::min(maxCount, bytes.size());
//
//            // Move elements from the internal deque to the new queue using bulk move
//            std::vector<uint8_t> newDeque(std::make_move_iterator(bytes.begin()),
//                                         std::make_move_iterator(bytes.begin() + countToDequeue));
//
//            // Erase the moved elements from the internal deque
//            bytes.erase(bytes.begin(), bytes.begin() + countToDequeue);
//
//            return newDeque;

            std::vector<uint8_t> newQueue(countToDequeue);

            for (uint8_t i = 0; i < maxCount && !bytes.empty(); ++i) {
                newQueue[i] = bytes.front();
                bytes.pop_front();
            }

            return newQueue;
        }

        Message dequeAsMessage(std::size_t maxCount = std::numeric_limits<std::size_t>::max()) {
            auto newQueue = this->dequeueAsVec(maxCount);

            return Message(newQueue);
        }

        std::vector<uint8_t> peek(std::size_t maxCount = std::numeric_limits<std::size_t>::max()) {
            // Determine the actual number of elements to dequeue
            std::size_t countToDequeue = std::min(maxCount, bytes.size());


            return {bytes.cbegin(), bytes.cbegin() + countToDequeue};
        }


    protected:
        std::deque<uint8_t> bytes;
    };

    struct StreamQueue final : public ReadOnlyStreamQueue {

        StreamQueue() = default;

        StreamQueue(std::span<uint8_t> bytes) : ReadOnlyStreamQueue(bytes) {};

        StreamQueue(StreamQueue &&) noexcept = default;

        explicit StreamQueue(StreamQueue const &) = default;

        ~StreamQueue() final = default;

        void enqueue(std::span<uint8_t> newBytes) {
            this->bytes.insert(this->bytes.end(), newBytes.begin(), newBytes.end());
        }

        void enqueueMove(std::span<uint8_t> newBytes) {
            this->bytes.insert(this->bytes.end(), std::make_move_iterator(newBytes.begin()),
                               std::make_move_iterator(newBytes.end()));
        }

        void enqueue(std::vector<uint8_t> &&newBytes) {
            // Move the elements from newBytes and insert them at the beginning of the internal deque
            this->bytes.insert(this->bytes.end(), std::make_move_iterator(newBytes.begin()),
                               std::make_move_iterator(newBytes.end()));
        }

        void enqueue(Message &&newBytes) {
            // Move the elements from newBytes and insert them at the beginning of the internal deque
            this->bytes.insert(this->bytes.begin(), std::make_move_iterator(newBytes.data()),
                               std::make_move_iterator(newBytes.data() + newBytes.length()));
        }

    };

}