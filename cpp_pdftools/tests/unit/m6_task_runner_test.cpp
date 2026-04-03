#include <any>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <thread>

#include "pdftools/runtime.hpp"
#include "pdftools/task.hpp"

namespace {

class SlowHandler final : public pdftools::ICommandHandler {
 public:
  pdftools::Status Handle(const std::any&, std::any* result, const pdftools::RuntimeContext& context) override {
    for (int i = 0; i < 200; ++i) {
      if (context.IsCancellationRequested()) {
        return pdftools::Status::Error(pdftools::ErrorCode::kCancelled, "cancelled by caller");
      }
      if (context.progress != nullptr) {
        context.progress->OnProgress(static_cast<double>(i) / 200.0, "slow-op", "running");
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (result != nullptr) {
      *result = 1;
    }
    return pdftools::Status::Ok();
  }
};

}  // namespace

int main() {
  pdftools::CommandRegistry registry;
  auto status = registry.Register("slow.op", std::make_unique<SlowHandler>());
  if (!status.ok()) {
    return EXIT_FAILURE;
  }

  pdftools::TaskRunner runner(registry);
  pdftools::TaskRequest request;
  request.operation_id = "slow.op";
  request.payload = 0;

  bool finished = false;
  pdftools::Status finish_status = pdftools::Status::Ok();

  pdftools::TaskHandle handle = runner.Submit(
      request, {},
      nullptr,
      [&](pdftools::TaskHandle, const pdftools::Status& status, const std::any&) {
        finished = true;
        finish_status = status;
      });

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  status = runner.Cancel(handle);
  if (!status.ok()) {
    return EXIT_FAILURE;
  }

  pdftools::TaskState state;
  for (int i = 0; i < 400; ++i) {
    status = runner.Query(handle, &state);
    if (!status.ok()) {
      return EXIT_FAILURE;
    }
    if (state.status == pdftools::TaskStatus::kCancelled || state.status == pdftools::TaskStatus::kFailed ||
        state.status == pdftools::TaskStatus::kSucceeded) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  if (!finished) {
    return EXIT_FAILURE;
  }
  if (state.status != pdftools::TaskStatus::kCancelled) {
    return EXIT_FAILURE;
  }
  if (finish_status.code() != pdftools::ErrorCode::kCancelled) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

