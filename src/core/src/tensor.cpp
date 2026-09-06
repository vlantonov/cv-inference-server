#include "cvis/core/tensor.hpp"

#include <algorithm>

namespace cvis::core {

std::int64_t elementCount(const Shape& shape) noexcept {
    if (shape.empty()) {
        return 1; // scalar
    }
    std::int64_t count = 1;
    for (std::int64_t dim : shape) {
        if (dim < 0) {
            return 0; // dynamic / un-allocatable
        }
        count *= dim;
    }
    return count;
}

Tensor::Tensor(std::string name, DataType dtype, Shape shape)
    : name_(std::move(name)), dtype_(dtype), shape_(std::move(shape)) {
    const std::int64_t elems = elementCount(shape_);
    const std::size_t bytes =
        static_cast<std::size_t>(elems) * byteSize(dtype_);
    data_.assign(bytes, std::byte{0});
}

Tensor Tensor::clone() const {
    Tensor copy;
    copy.name_ = name_;
    copy.dtype_ = dtype_;
    copy.shape_ = shape_;
    copy.data_ = data_;
    return copy;
}

} // namespace cvis::core
