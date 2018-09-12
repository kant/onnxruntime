#include "image_scaler.h"
#include "image_scaler_impl.h"

namespace onnxruntime {
namespace cuda {

#define REGISTER_KERNEL_TYPED(T)                                  \
  ONNX_OPERATOR_TYPED_KERNEL_EX(                                  \
      ImageScaler,                                                \
      kOnnxDomain,                                                \
      1,                                                          \
      T,                                                          \
      kCudaExecutionProvider,                                     \
      KernelDefBuilder()                                          \
          .TypeConstraint("T", DataTypeImpl::GetTensorType<T>()), \
      ImageScaler<T>);

REGISTER_KERNEL_TYPED(float)
REGISTER_KERNEL_TYPED(double)
REGISTER_KERNEL_TYPED(MLFloat16)

template <typename T>
ImageScaler<T>::ImageScaler(const OpKernelInfo& info) : CudaKernel(info) {
  LOTUS_ENFORCE(info.GetAttr<float>("scale", &scale_).IsOK());
  LOTUS_ENFORCE(info.GetAttrs<float>("bias", bias_).IsOK());

  b_data_ = GetScratchBuffer<float>(bias_.size());
  CUDA_CALL_THROW(cudaMemcpy(b_data_.get(), bias_.data(), sizeof(float) * bias_.size(), cudaMemcpyHostToDevice));
}

template <typename T>
Status ImageScaler<T>::ComputeInternal(OpKernelContext* context) const {
  const Tensor* X = context->Input<Tensor>(0);
  const auto dims = X->Shape().GetDims();

  if (dims.size() != 4) {
    return LOTUS_MAKE_STATUS(LOTUS, INVALID_ARGUMENT,
                             "Input is expected to have four dimensions corresponding to [N,C,H,W], got ", dims.size());
  }

  const int64_t C = dims[1];  // dims are NCHW

  if (!bias_.empty() && bias_.size() != static_cast<size_t>(C)) {
    return LOTUS_MAKE_STATUS(LOTUS, INVALID_ARGUMENT, "Bias size (", bias_.size(), ") does not match the number of channels (", C, ")");
  }

  Tensor* Y = context->Output(0, X->Shape());

  typedef typename ToCudaType<T>::MappedType CudaT;
  ImageScalerImpl<CudaT>(
      reinterpret_cast<const CudaT*>(X->Data<T>()),
      scale_,
      b_data_.get(),
      dims.data(),
      reinterpret_cast<CudaT*>(Y->MutableData<T>()),
      X->Shape().Size());

  return Status::OK();
}

}  // namespace cuda
}  // namespace onnxruntime