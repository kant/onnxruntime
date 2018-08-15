#pragma once
#include <iosfwd>
#include <vector>
#include <algorithm>
#include <string>
#include <cstring>

namespace Lotus {

// TODO - Use a custom STL allocator to avoid heap allocations in the common case.
// We use negative numbers for unknown symbolic dimension. Each negative
// number represents a unique symbolic dimension.
// Private inheritance is used to prevent ambiguity of element versus dimension size
class TensorShape : private std::vector<int64_t> {
 public:
  TensorShape();

  TensorShape(const int64_t* dimension_sizes, size_t dimension_count);

  TensorShape(const std::vector<int64_t>& dims);

  TensorShape(const TensorShape& other);

  TensorShape(const std::vector<int64_t>& dims, size_t start, size_t end);

  /**
		Return the dimension specified by <idx>.
		*/
  const int64_t& operator[](size_t idx) const {
    return std::vector<int64_t>::operator[](static_cast<int>(idx));
  }

  int64_t& operator[](size_t idx) {
    return std::vector<int64_t>::operator[](static_cast<int>(idx));
  }

  bool operator==(const TensorShape& other) const noexcept {
    auto thisVector = static_cast<const std::vector<int64_t>*>(this);
    auto otherVector = static_cast<const std::vector<int64_t>*>(&other);
    return *thisVector == *otherVector;
  }

  bool operator!=(const TensorShape& other) const noexcept {
    return !(*this == other);
  }

  size_t NumDimensions() const noexcept {
    return size();
  }

  /**
		Copy dims into an array with given size
		*/
  void CopyDims(int64_t* dims, size_t num_dims) const {
    memcpy(dims, data(), sizeof(value_type) * std::min(num_dims, NumDimensions()));
  }

  /**
		Return underlying vector representation.
		*/
  const std::vector<int64_t>& GetDims() const { return *this; }

  /**
		Return the total number of elements. Returns 1 for an empty (rank 0) TensorShape.
		*/
  int64_t Size() const;

  /**
		Return the total number of elements up to the specified dimension.
		@param dimension Return size up to this dimension. Value must be >= 0 and < this.Size().
		*/
  int64_t SizeToDimension(size_t dimension) const;

  /**
		Return the total number of elements from the specified dimension to the end of the tensor shape.
		@param dimension Return size up to this dimension. 0 <= dimension < this.Size().
		*/
  int64_t SizeFromDimension(size_t dimension) const;

  // Return a new TensorShape of the dimensions from dimstart to dimend.
  TensorShape Slice(size_t dimstart, size_t dimend) const;

  // Return a new TensorShape of the dimensions from dimstart to end.
  TensorShape Slice(size_t dimstart) const;

  // output dimensions nicely formatted
  std::string ToString() const;

  // Calculate size between start and end.
  // Assumes start and end are between 0 and dimensions.size(), inclusive, and that
  // start < end.
  int64_t SizeHelper(size_t start, size_t end) const;

  // empty shape or 1D shape (1) is regarded as scalar tensor
  bool IsScalar() const {
    return size() == 0 || (size() == 1 && at(0) == 1);
  }

  static const TensorShape& ReinterpretBaseType(const std::vector<int64_t>& dimensions) {
    static_assert(sizeof(TensorShape) == sizeof(std::vector<int64_t>), "Size of TensorShape prevents safe casting from vector");
    return *static_cast<const TensorShape*>(&dimensions);
  }
};
// operator<< to nicely output to a stream
std::ostream& operator<<(std::ostream& out, const Lotus::TensorShape& shape);

}  // namespace Lotus