#pragma once

#include <string>
#include "core/common/common.h"
#include "core/common/exceptions.h"
#include "core/framework/allocator.h"
#include "core/framework/data_types.h"
#include "core/framework/tensor.h"

namespace Lotus {
class MLValue {
 public:
  MLValue() : data_(nullptr), type_(nullptr) {}
  virtual ~MLValue() = default;

  void Init(void* pData, MLDataType type, DeleteFunc deleter) {
    data_.reset(pData, deleter);
    type_ = type;
  }

  bool IsAllocated() {
    return data_ && type_;
  }

  template <typename T>
  const T& Get() const {
    LOTUS_ENFORCE(DataTypeImpl::GetType<T>() == type_, DataTypeImpl::GetType<T>(), " != ", type_);
    return *static_cast<T*>(data_.get());
  }

  template <typename T>
  T* GetMutable() {
    LOTUS_ENFORCE(DataTypeImpl::GetType<T>() == type_, DataTypeImpl::GetType<T>(), " != ", type_);
    return static_cast<T*>(data_.get());
  }

  bool IsTensor() {
    return DataTypeImpl::GetType<Tensor>() == type_;
  }

 private:
  std::shared_ptr<void> data_;
  MLDataType type_;
};
}  // namespace Lotus
