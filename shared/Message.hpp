#pragma once

#include <memory>
#include <cstring>
#include <string>

// Heavily inspired from https://github.com/shuai132/SocketPP/blob/7741e80603b3a7ee06ee7ebbc74488935f2de41c/socketpp/RawMsg.h
// Please don't mind, good library inspiration

namespace SocketLib {
    using byte = unsigned char;

    struct Message {
        inline void init(const byte* data, size_t len) {
            if (data == nullptr || len == 0) {
                _len = 0;
                _data = nullptr;
                return;
            }

            _len = len;
            _data = new byte[len];

            memcpy(_data, data, len);
        }

        explicit Message(const byte* data, size_t len) {
            init(data, len);
        }

        explicit Message(const std::string& data) {
            init((byte*) data.data(), data.length());
        }

        Message(const Message& msg) {
            init(msg._data, msg._len);
        }

        Message& operator=(const Message& msg) {
            if (&msg == this)
                return *this;

            delete[] _data;
            init(msg._data, msg._len);
            return *this;
        }

        inline Message& move(Message& other) {
            if (&other == this)
                return *this;

            _len = other._len;
            delete[] _data;

            _data = other._data;
            other._data = nullptr;

            return *this;
        }

        Message(Message&& other) noexcept {
            move(other);
        };

        Message& operator=(Message&& other) noexcept {
            return move(other);
        }

        ~Message() {
            delete[] _data;
        }

        [[nodiscard]] byte* data() const {
            return _data;
        }

        [[nodiscard]] size_t length() const {
            return _len;
        }

        [[nodiscard]] std::string toString() const {
            return std::string((char*) _data, 0, _len);
        }

    private:
        size_t _len = 0;
        byte* _data = nullptr;


    };
}