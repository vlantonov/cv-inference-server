#include "cvis/backends/fake_backend.hpp"

namespace cvis::backends {

core::Status FakeBackend::load(const ModelConfig& cfg) {
    metadata_.model_id = cfg.model_id;
    metadata_.backend = "fake";
    metadata_.input_names.clear();
    metadata_.output_names.clear();
    metadata_.ready = true;
    return core::Status::Ok();
}

const ModelMetadata& FakeBackend::metadata() const noexcept { return metadata_; }

core::Expected<std::vector<core::InferResponse>>
FakeBackend::executeBatch(std::span<const core::InferRequest* const> batch) {
    ++batch_calls_;
    std::vector<core::InferResponse> responses;
    responses.reserve(batch.size());
    for (const core::InferRequest* req : batch) {
        if (req == nullptr) {
            return core::Status::Error(core::StatusCode::kInternal,
                                       "null request in batch");
        }
        core::InferResponse resp;
        resp.model_id = req->model_id;
        resp.correlation_id = req->correlation_id;
        resp.outputs.reserve(req->inputs.size());
        for (const core::Tensor& in : req->inputs) {
            resp.outputs.push_back(in.clone()); // echo / identity
        }
        resp.status = core::Status::Ok();
        responses.push_back(std::move(resp));
    }
    return responses;
}

} // namespace cvis::backends
