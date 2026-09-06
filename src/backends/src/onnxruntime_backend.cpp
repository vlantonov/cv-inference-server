// Compiled only under CVIS_ENABLE_ONNXRUNTIME (guarded in CMakeLists.txt). Not
// part of the default CPU build's compile set unless ONNX Runtime is found.
#include "cvis/backends/onnxruntime_backend.hpp"

#include <onnxruntime_cxx_api.h>

#include <cstring>
#include <numeric>

namespace cvis::backends {
namespace {

ONNXTensorElementDataType toOrt(core::DataType dt) {
    switch (dt) {
        case core::DataType::kFloat32: return ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT;
        case core::DataType::kFloat16: return ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16;
        case core::DataType::kInt8:    return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8;
        case core::DataType::kUInt8:   return ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8;
        case core::DataType::kInt32:   return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32;
        case core::DataType::kInt64:   return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64;
        case core::DataType::kUnspecified: return ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
    }
    return ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
}

core::DataType fromOrt(ONNXTensorElementDataType dt) {
    switch (dt) {
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:   return core::DataType::kFloat32;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16: return core::DataType::kFloat16;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:    return core::DataType::kInt8;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8:   return core::DataType::kUInt8;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:   return core::DataType::kInt32;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:   return core::DataType::kInt64;
        default:                                    return core::DataType::kUnspecified;
    }
}

} // namespace

struct OnnxRuntimeBackend::Impl {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "cvis"};
    Ort::SessionOptions session_options{};
    std::unique_ptr<Ort::Session> session;
    Ort::AllocatorWithDefaultOptions allocator{};
    std::vector<std::string> input_names;
    std::vector<std::string> output_names;
};

OnnxRuntimeBackend::OnnxRuntimeBackend() : impl_(std::make_unique<Impl>()) {}
OnnxRuntimeBackend::~OnnxRuntimeBackend() = default;

core::Status OnnxRuntimeBackend::load(const ModelConfig& cfg) {
    try {
        impl_->session_options.SetIntraOpNumThreads(1);
        impl_->session_options.SetGraphOptimizationLevel(
            GraphOptimizationLevel::ORT_ENABLE_ALL);

#ifdef CVIS_ENABLE_CUDA
        if (cfg.use_gpu) {
            // Transparent CPU fallback (FR-22): if appending the CUDA EP fails,
            // continue on the CPU EP rather than failing the load.
            try {
                OrtCUDAProviderOptions cuda_opts{};
                impl_->session_options.AppendExecutionProvider_CUDA(cuda_opts);
            } catch (const Ort::Exception&) {
                // fall through to CPU
            }
        }
#endif

        impl_->session = std::make_unique<Ort::Session>(
            impl_->env, cfg.path.c_str(), impl_->session_options);

        const size_t n_in = impl_->session->GetInputCount();
        for (size_t i = 0; i < n_in; ++i) {
            auto name = impl_->session->GetInputNameAllocated(i, impl_->allocator);
            impl_->input_names.emplace_back(name.get());
        }
        const size_t n_out = impl_->session->GetOutputCount();
        for (size_t i = 0; i < n_out; ++i) {
            auto name = impl_->session->GetOutputNameAllocated(i, impl_->allocator);
            impl_->output_names.emplace_back(name.get());
        }

        metadata_.model_id = cfg.model_id;
        metadata_.backend = "onnxruntime";
        metadata_.input_names = impl_->input_names;
        metadata_.output_names = impl_->output_names;
        metadata_.ready = true;
        return core::Status::Ok();
    } catch (const Ort::Exception& e) {
        return core::Status::Error(core::StatusCode::kFailedPrecondition,
                                   std::string("onnxruntime load failed: ") + e.what());
    }
}

const ModelMetadata& OnnxRuntimeBackend::metadata() const noexcept { return metadata_; }

core::Expected<std::vector<core::InferResponse>>
OnnxRuntimeBackend::executeBatch(std::span<const core::InferRequest* const> batch) {
    if (batch.empty()) {
        return std::vector<core::InferResponse>{};
    }
    // v1 reference geometry is fixed (OQ-2): stack per-request inputs along a new
    // leading batch dimension, run once, then slice outputs row-wise (§7.2).
    try {
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        const auto& first = *batch.front();
        const std::size_t n_inputs = first.inputs.size();
        const auto n = static_cast<std::int64_t>(batch.size());

        std::vector<Ort::Value> ort_inputs;
        std::vector<std::vector<std::byte>> staging(n_inputs);
        std::vector<std::vector<std::int64_t>> shapes(n_inputs);
        std::vector<const char*> in_names;
        std::vector<const char*> out_names;
        for (const auto& s : impl_->input_names) in_names.push_back(s.c_str());
        for (const auto& s : impl_->output_names) out_names.push_back(s.c_str());

        for (std::size_t j = 0; j < n_inputs; ++j) {
            const core::Tensor& proto = first.inputs[j];
            core::Shape shape = proto.shape;
            shapes[j].assign({n});
            shapes[j].insert(shapes[j].end(), shape.begin(), shape.end());

            std::size_t per = proto.byteCount();
            staging[j].resize(per * batch.size());
            for (std::size_t r = 0; r < batch.size(); ++r) {
                auto bytes = batch[r]->inputs[j].bytes();
                std::memcpy(staging[j].data() + r * per, bytes.data(), per);
            }
            ort_inputs.push_back(Ort::Value::CreateTensor(
                mem, staging[j].data(), staging[j].size(),
                shapes[j].data(), shapes[j].size(), toOrt(proto.dtype)));
        }

        auto outputs = impl_->session->Run(
            Ort::RunOptions{nullptr}, in_names.data(), ort_inputs.data(),
            ort_inputs.size(), out_names.data(), out_names.size());

        std::vector<core::InferResponse> responses(batch.size());
        for (std::size_t r = 0; r < batch.size(); ++r) {
            responses[r].model_id = batch[r]->model_id;
            responses[r].correlation_id = batch[r]->correlation_id;
            responses[r].status = core::Status::Ok();
        }
        for (std::size_t o = 0; o < outputs.size(); ++o) {
            auto info = outputs[o].GetTensorTypeAndShapeInfo();
            auto full = info.GetShape();
            core::DataType dt = fromOrt(info.GetElementType());
            core::Shape per_shape(full.begin() + 1, full.end());
            std::int64_t per_elems = core::elementCount(per_shape);
            std::size_t per_bytes = static_cast<std::size_t>(per_elems) * core::byteSize(dt);
            const auto* base = static_cast<const std::byte*>(outputs[o].GetTensorRawData());
            for (std::size_t r = 0; r < batch.size(); ++r) {
                core::Tensor t(impl_->output_names[o], dt, per_shape);
                std::memcpy(t.bytes().data(), base + r * per_bytes, per_bytes);
                responses[r].outputs.push_back(std::move(t));
            }
        }
        return responses;
    } catch (const Ort::Exception& e) {
        return core::Status::Error(core::StatusCode::kInternal,
                                   std::string("onnxruntime run failed: ") + e.what());
    }
}

} // namespace cvis::backends
