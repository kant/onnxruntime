#pragma once

#include <functional>

#include "core/common/exceptions.h"
#include "core/common/logging/logging.h"
#include "core/common/status.h"
#include "core/framework/execution_provider.h"
#include "core/framework/kernel_def_builder.h"
#include "core/framework/ml_value.h"
#include "core/framework/op_kernel_info.h"
#include "core/framework/op_node_proto_helper.h"
#include "core/framework/tensor.h"
#include "core/graph/constants.h"
#include "core/graph/graph.h"
#include "gsl/span"
#include "onnx/defs/schema.h"

using namespace LotusIR;

namespace Lotus {
class ExecutionFrame;
class OpKernelContext;
class OpKernelWrapper;

class OpKernel {
 public:
  using DoneCallback = std::function<void()>;

  explicit OpKernel(const OpKernelInfo& info) : op_kernel_info_(info) {}
  virtual ~OpKernel() = default;

  const LotusIR::Node& Node() const {
    return op_kernel_info_.node();
  }

  const ::Lotus::KernelDef& KernelDef() const {
    return op_kernel_info_.GetKernelDef();
  }

  virtual Status Compute(OpKernelContext* context) const = 0;

  virtual Status ComputeAsync(OpKernelContext*,
                              DoneCallback) const {
    LOTUS_NOT_IMPLEMENTED(__FUNCTION__, " is not implemented");
  }

  const AllocatorInfo& Allocator(MemType mem_type) const {
    return op_kernel_info_.GetAllocatorInfo(mem_type);
  }

  const OpKernelInfo& Info() const { return op_kernel_info_; }

 private:
  LOTUS_DISALLOW_COPY_ASSIGN_AND_MOVE(OpKernel);
  OpKernelInfo op_kernel_info_;
};

class OpKernelContext {
 public:
  typedef std::unordered_map<std::string, size_t> ArgMap;

  explicit OpKernelContext(ExecutionFrame* frame,
                           const OpKernel* kernel,
                           const Logging::Logger& logger);

  virtual ~OpKernelContext() = default;

  /**
  Return the number of inputs for a variadic argument.
  @param arg_num The operator argument number.
  @returns Number of inputs the argument has.
  */
  int NumVariadicInputs(size_t arg_num) const;

  MLDataType InputType(int index) const;
  MLDataType OutputType(int index) const;

  template <typename T>
  const T* Input(int index) const {
    const MLValue* p_ml_value = GetInputMLValue(index);
    return p_ml_value ? &(p_ml_value->Get<T>()) : nullptr;
  }

  // Fetch output (non-tensor) with specified index.
  template <typename T>
  T* Output(int index) {
    if (index < 0 || index >= OutputCount())
      return nullptr;

    MLValue* p_ml_value = nullptr;
    LOTUS_ENFORCE(GetOrCreateOutputMLValue(index, p_ml_value).IsOK());
    return p_ml_value ? p_ml_value->GetMutable<T>() : nullptr;
  }

  // In the case that memory allocation has not been done for an output tensor,
  // The memory allocation will be done on-the-fly with given tensor shape.
  // Return nullptr if the output is an unused optional output.
  Tensor* Output(int index, const TensorShape& shape);

  const Logging::Logger& Logger() const {
    return *logger_;
  }

  int InputCount() const {
    return static_cast<int>(kernel_->Node().InputDefs().size());
  }

  int OutputCount() const {
    return static_cast<int>(kernel_->Node().OutputDefs().size());
  }

  Status GetTempSpaceAllocator(AllocatorPtr* output) const;

  /**
  Return the fence of current node's input.
  @param index The index of the input.
  @returns Point to the Fence of the input MLValue.
  It is null if the input MLValue doesn't have fence or the input is optional. 
  */
  Fence_t InputFence(int index) const;

  /**
  Return the fence of current node's output identifed by index.
  @param index The index of the output.
  @returns Point to the Fence of the output MLValue.
  It is null if the output MLValue doesn't have fence or the output is optional. 
  */
  Fence_t OutputFence(int index) const;

 protected:
  LotusIR::NodeIndex GetNodeIndex() const;
  const SessionState& GetSessionState() const;

  const MLValue* GetInputMLValue(int index) const;
  MLValue* GetOutputMLValue(int index);

 private:
  Status GetOrCreateOutputMLValue(int index, MLValue*& value);

  int GetInputArgIndex(int index) const;
  int GetOutputArgIndex(int index) const;

  ExecutionFrame* execution_frame_{nullptr};
  const OpKernel* kernel_{nullptr};
  const Logging::Logger* logger_{nullptr};

  // The argument starting index in ExecutionFrame.
  int node_input_start_index_{-1};
  int node_output_start_index_{-1};
};

// Fetching output tensor without shape is not allowed.
template <>
inline Tensor* OpKernelContext::Output<Tensor>(int) {
  LOTUS_ENFORCE(false, "Please fetch output tensor with specified shape.");
  return nullptr;
}

using KernelCreateFn = std::function<OpKernel*(const OpKernelInfo& info)>;

struct KernelCreateInfo {
  std::unique_ptr<KernelDef> kernel_def;  // Owned and stored in the global kernel registry.
  KernelCreateFn kernel_create_func;
  Status status;

  KernelCreateInfo(std::unique_ptr<KernelDef> definition,
                   KernelCreateFn create_func)
      : kernel_def(std::move(definition)),
        kernel_create_func(create_func) {}

  KernelCreateInfo(KernelCreateInfo&& other)
      : kernel_def(std::move(other.kernel_def)),
        kernel_create_func(other.kernel_create_func) {}
};

using KernelCreateMap = std::multimap<std::string, KernelCreateInfo>;

// Forward declarations for the non-specialized BuildKernel method.
template <typename T>
KernelCreateInfo BuildKernel();

namespace ML {
template <typename T>
KernelCreateInfo BuildKernel();
}  // namespace ML

namespace Cuda {
template <typename T>
KernelCreateInfo BuildKernel();
}  // namespace Cuda

namespace MklDnn {
template <typename T>
KernelCreateInfo BuildKernel();
}  // namespace MklDnn

// Naming convention for operator kernel classes
#define ONNX_OPERATOR_KERNEL_CLASS_NAME(provider, domain, ver, name) \
  provider##_##name##_##domain##_ver##ver

#define ONNX_CPU_OPERATOR_KERNEL(name, ver, builder, ...) \
  ONNX_OPERATOR_KERNEL_EX(name, kOnnxDomain, ver, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_CPU_OPERATOR_ML_KERNEL(name, ver, builder, ...) \
  ONNX_OPERATOR_KERNEL_EX(name, kMLDomain, ver, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_OPERATOR_KERNEL_EX(name, domain, ver, provider, builder, ...)            \
  class ONNX_OPERATOR_KERNEL_CLASS_NAME(provider, domain, ver, name);                 \
  template <>                                                                         \
  KernelCreateInfo                                                                    \
  BuildKernel<ONNX_OPERATOR_KERNEL_CLASS_NAME(provider, domain, ver, name)>() {       \
    return KernelCreateInfo(                                                          \
        builder.SetName(#name)                                                        \
            .SetDomain(domain)                                                        \
            .SinceVersion(ver)                                                        \
            .Provider(provider)                                                       \
            .Build(),                                                                 \
        [](const OpKernelInfo& info) -> OpKernel* { return new __VA_ARGS__(info); }); \
  }

#define ONNX_OPERATOR_VERSIONED_KERNEL_CLASS_NAME(provider, domain, startver, endver, name) \
  provider##_##name##_##domain##_ver##startver##_##endver

#define ONNX_CPU_OPERATOR_VERSIONED_KERNEL(name, startver, endver, builder, ...) \
  ONNX_OPERATOR_VERSIONED_KERNEL_EX(name, kOnnxDomain, startver, endver, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_CPU_OPERATOR_VERSIONED_ML_KERNEL(name, startver, endver, builder, ...) \
  ONNX_OPERATOR_VERSIONED_KERNEL_EX(name, kMLDomain, startver, endver, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_OPERATOR_VERSIONED_KERNEL_EX(name, domain, startver, endver, provider, builder, ...)      \
  class ONNX_OPERATOR_VERSIONED_KERNEL_CLASS_NAME(provider, domain, startver, endver, name);           \
  template <>                                                                                          \
  KernelCreateInfo                                                                                     \
  BuildKernel<ONNX_OPERATOR_VERSIONED_KERNEL_CLASS_NAME(provider, domain, startver, endver, name)>() { \
    return KernelCreateInfo(                                                                           \
        builder.SetName(#name)                                                                         \
            .SetDomain(domain)                                                                         \
            .SinceVersion(startver, endver)                                                            \
            .Provider(provider)                                                                        \
            .Build(),                                                                                  \
        [](const OpKernelInfo& info) -> OpKernel* { return new __VA_ARGS__(info); });                  \
  }

#define ONNX_OPERATOR_TYPED_KERNEL_CLASS_NAME(provider, domain, ver, type, name) \
  provider##_##name##_##domain##_ver##ver##_##type

#define ONNX_CPU_OPERATOR_TYPED_KERNEL(name, ver, type, builder, ...) \
  ONNX_OPERATOR_TYPED_KERNEL_EX(name, kOnnxDomain, ver, type, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_CPU_OPERATOR_TYPED_ML_KERNEL(name, ver, type, builder, ...) \
  ONNX_OPERATOR_TYPED_KERNEL_EX(name, kMLDomain, ver, type, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_OPERATOR_TYPED_KERNEL_EX(name, domain, ver, type, provider, builder, ...)      \
  class ONNX_OPERATOR_TYPED_KERNEL_CLASS_NAME(provider, domain, ver, type, name);           \
  template <>                                                                               \
  KernelCreateInfo                                                                          \
  BuildKernel<ONNX_OPERATOR_TYPED_KERNEL_CLASS_NAME(provider, domain, ver, type, name)>() { \
    return KernelCreateInfo(                                                                \
        builder.SetName(#name)                                                              \
            .SetDomain(domain)                                                              \
            .SinceVersion(ver)                                                              \
            .Provider(provider)                                                             \
            .Build(),                                                                       \
        [](const OpKernelInfo& info) -> OpKernel* { return new __VA_ARGS__(info); });       \
  }

}  // namespace Lotus
