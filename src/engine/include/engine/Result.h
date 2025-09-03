#pragma once
#include <string>
#include <utility>

namespace engine {

template <typename T>
struct Result {
    bool ok;
    T value;
    std::string error;

    static Result Ok(T v) { return Result{true, std::move(v), {}}; }
    static Result Err(std::string e) { return Result{false, T{}, std::move(e)}; }

    bool is_ok() const { return ok; }
    const std::string& err() const { return error; }
};

template <>
struct Result<void> {
    bool ok;
    std::string error;

    static Result Ok() { return Result{true, {}}; }
    static Result Err(std::string e) { return Result{false, std::move(e)}; }

    bool is_ok() const { return ok; }
    const std::string& err() const { return error; }
};

} // namespace engine
