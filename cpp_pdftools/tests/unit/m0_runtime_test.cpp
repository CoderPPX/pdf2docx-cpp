#include <any>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <thread>

#include "pdftools/runtime.hpp"
#include "pdftools/task.hpp"

namespace {

class AddOneHandler final : public pdftools::ICommandHandler {
 public:
  pdftools::Status Handle(const std::any& request, std::any* result, const pdftools::RuntimeContext& context) override {
    if (context.progress != nullptr) {
      context.progress->OnProgress(0.5, "compute", "adding one");
    }
    int value = 0;
    try {
      value = std::any_cast<int>(request);
    } catch (...) {
      return pdftools::Status::Error(pdftools::ErrorCode::kInvalidArgument, "request is not int");
    }
    if (result == nullptr) {
      return pdftools::Status::Error(pdftools::ErrorCode::kInvalidArgument, "result pointer is null");
    }
    *result = value + 1;
    return pdftools::Status::Ok();
  }
};

}  // namespace

int main() {
  pdftools::CommandRegistry registry;
  auto register_status = registry.Register("math.add_one", std::make_unique<AddOneHandler>());
  if (!register_status.ok()) {
    return EXIT_FAILURE;
  }

  auto duplicate_status = registry.Register("math.add_one", std::make_unique<AddOneHandler>());
  if (duplicate_status.ok()) {
    return EXIT_FAILURE;
  }

  std::any sync_result;
  auto execute_status = registry.Execute("math.add_one", 41, &sync_result, {});
  if (!execute_status.ok()) {
    return EXIT_FAILURE;
  }
  if (std::any_cast<int>(sync_result) != 42) {
    return EXIT_FAILURE;
  }

  pdftools::TaskRunner runner(registry);
  pdftools::TaskRequest request;
  request.operation_id = "math.add_one";
  request.payload = 99;

  bool finished = false;
  bool succeeded = false;
  int async_value = 0;
  const pdftools::TaskHandle handle = runner.Submit(
      request, {},
      nullptr,
      [&](pdftools::TaskHandle, const pdftools::Status& status, const std::any& result) {
        finished = true;
        succeeded = status.ok();
        if (status.ok()) {
          async_value = std::any_cast<int>(result);
        }
      });

  for (int i = 0; i < 200 && !finished; ++i) {
    pdftools::TaskState state;
    auto query_status = runner.Query(handle, &state);
    if (!query_status.ok()) {
      return EXIT_FAILURE;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  if (!finished || !succeeded || async_value != 100) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
