#include "cvis/core/data_type.hpp"

namespace cvis::core {

std::size_t byteSize(DataType dt) noexcept {
    switch (dt) {
        case DataType::kFloat32: return 4;
        case DataType::kFloat16: return 2;
        case DataType::kInt8:    return 1;
        case DataType::kUInt8:   return 1;
        case DataType::kInt32:   return 4;
        case DataType::kInt64:   return 8;
        case DataType::kUnspecified: return 0;
    }
    return 0;
}

const char* toString(DataType dt) noexcept {
    switch (dt) {
        case DataType::kFloat32: return "float32";
        case DataType::kFloat16: return "float16";
        case DataType::kInt8:    return "int8";
        case DataType::kUInt8:   return "uint8";
        case DataType::kInt32:   return "int32";
        case DataType::kInt64:   return "int64";
        case DataType::kUnspecified: return "unspecified";
    }
    return "unspecified";
}

} // namespace cvis::core
