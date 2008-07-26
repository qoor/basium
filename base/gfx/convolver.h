// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef BASE_GFX_CONVOLVER_H__
#define BASE_GFX_CONVOLVER_H__

#include <vector>

#include "base/basictypes.h"

namespace gfx {

// Represents a filter in one dimension. Each output pixel has one entry in this
// object for the filter values contributing to it. You build up the filter
// list by calling AddFilter for each output pixel (in order).
//
// We do 2-dimensional convolusion by first convolving each row by one
// ConvolusionFilter1D, then convolving each column by another one.
//
// Entries are stored in fixed point, shifted left by kShiftBits.
class ConvolusionFilter1D {
 public:
  // The number of bits that fixed point values are shifted by.
  enum { kShiftBits = 14 };

  ConvolusionFilter1D() : max_filter_(0) {
  }

  // Convert between floating point and our fixed point representation.
  static inline int16 FloatToFixed(float f) {
    return static_cast<int16>(f * (1 << kShiftBits));
  }
  static inline unsigned char FixedToChar(int16 x) {
    return static_cast<unsigned char>(x >> kShiftBits);
  }

  // Returns the maximum pixel span of a filter.
  int max_filter() const { return max_filter_; }

  // Returns the number of filters in this filter. This is the dimension of the
  // output image.
  int num_values() const { return static_cast<int>(filters_.size()); }

  // Appends the given list of scaling values for generating a given output
  // pixel. |filter_offset| is the distance from the edge of the image to where
  // the scaling factors start. The scaling factors apply to the source pixels
  // starting from this position, and going for the next |filter_length| pixels.
  //
  // You will probably want to make sure your input is normalized (that is,
  // all entries in |filter_values| sub to one) to prevent affecting the overall
  // brighness of the image.
  //
  // The filter_length must be > 0.
  //
  // This version will automatically convert your input to fixed point.
  void AddFilter(int filter_offset,
                 const float* filter_values,
                 int filter_length);

  // Same as the above version, but the input is already fixed point.
  void AddFilter(int filter_offset,
                 const int16* filter_values,
                 int filter_length);

  // Retrieves a filter for the given |value_offset|, a position in the output
  // image in the direction we're convolving. The offset and length of the
  // filter values are put into the corresponding out arguments (see AddFilter
  // above for what these mean), and a pointer to the first scaling factor is
  // returned. There will be |filter_length| values in this array.
  inline const int16* FilterForValue(int value_offset,
                                     int* filter_offset,
                                     int* filter_length) const {
    const FilterInstance& filter = filters_[value_offset];
    *filter_offset = filter.offset;
    *filter_length = filter.length;
    return &filter_values_[filter.data_location];
  }

 private:
  struct FilterInstance {
    // Offset within filter_values for this instance of the filter.
    int data_location;

    // Distance from the left of the filter to the center. IN PIXELS
    int offset;

    // Number of values in this filter instance.
    int length;
  };

  // Stores the information for each filter added to this class.
  std::vector<FilterInstance> filters_;

  // We store all the filter values in this flat list, indexed by
  // |FilterInstance.data_location| to avoid the mallocs required for storing
  // each one separately.
  std::vector<int16> filter_values_;

  // The maximum size of any filter we've added.
  int max_filter_;
};

// Does a two-dimensional convolusion on the given source image.
//
// It is assumed the source pixel offsets referenced in the input filters
// reference only valid pixels, so the source image size is not required. Each
// row of the source image starts |source_byte_row_stride| after the previous
// one (this allows you to have rows with some padding at the end).
//
// The result will be put into the given output buffer. The destination image
// size will be xfilter.num_values() * yfilter.num_values() pixels. It will be
// in rows of exactly xfilter.num_values() * 4 bytes.
//
// |source_has_alpha| is a hint that allows us to avoid doing computations on
// the alpha channel if the image is opaque. If you don't know, set this to
// true and it will work properly, but setting this to false will be a few
// percent faster if you know the image is opaque.
//
// The layout in memory is assumed to be 4-bytes per pixel in B-G-R-A order
// (this is ARGB when loaded into 32-bit words on a little-endian machine).
void BGRAConvolve2D(const uint8* source_data,
                    int source_byte_row_stride,
                    bool source_has_alpha,
                    const ConvolusionFilter1D& xfilter,
                    const ConvolusionFilter1D& yfilter,
                    uint8* output);

}  // namespace gfx

#endif  // BASE_GFX_CONVOLVER_H__
