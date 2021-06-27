#pragma once

namespace SocketLib {
    template<typename T>
    struct FieldWrapper {
        T& reference;

        explicit FieldWrapper(T& r) : reference(std::move(r)) {}
    };
}

