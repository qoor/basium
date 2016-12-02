// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NUMERICS_SAFE_CONVERSIONS_IMPL_H_
#define BASE_NUMERICS_SAFE_CONVERSIONS_IMPL_H_

#include <limits.h>
#include <stdint.h>

#include <climits>
#include <limits>
#include <type_traits>

namespace base {
namespace internal {

// The std library doesn't provide a binary max_exponent for integers, however
// we can compute one by adding one to the number of non-sign bits. This allows
// for accurate range comparisons between floating point and integer types.
template <typename NumericType>
struct MaxExponent {
  static const int value = std::numeric_limits<NumericType>::is_iec559
                               ? std::numeric_limits<NumericType>::max_exponent
                               : (sizeof(NumericType) * CHAR_BIT + 1 -
                                  std::numeric_limits<NumericType>::is_signed);
};

enum IntegerRepresentation {
  INTEGER_REPRESENTATION_UNSIGNED,
  INTEGER_REPRESENTATION_SIGNED
};

// A range for a given nunmeric Src type is contained for a given numeric Dst
// type if both numeric_limits<Src>::max() <= numeric_limits<Dst>::max() and
// numeric_limits<Src>::min() >= numeric_limits<Dst>::min() are true.
// We implement this as template specializations rather than simple static
// comparisons to ensure type correctness in our comparisons.
enum NumericRangeRepresentation {
  NUMERIC_RANGE_NOT_CONTAINED,
  NUMERIC_RANGE_CONTAINED
};

// Helper templates to statically determine if our destination type can contain
// maximum and minimum values represented by the source type.

template <
    typename Dst,
    typename Src,
    IntegerRepresentation DstSign = std::numeric_limits<Dst>::is_signed
                                            ? INTEGER_REPRESENTATION_SIGNED
                                            : INTEGER_REPRESENTATION_UNSIGNED,
    IntegerRepresentation SrcSign =
        std::numeric_limits<Src>::is_signed
            ? INTEGER_REPRESENTATION_SIGNED
            : INTEGER_REPRESENTATION_UNSIGNED >
struct StaticDstRangeRelationToSrcRange;

// Same sign: Dst is guaranteed to contain Src only if its range is equal or
// larger.
template <typename Dst, typename Src, IntegerRepresentation Sign>
struct StaticDstRangeRelationToSrcRange<Dst, Src, Sign, Sign> {
  static const NumericRangeRepresentation value =
      MaxExponent<Dst>::value >= MaxExponent<Src>::value
          ? NUMERIC_RANGE_CONTAINED
          : NUMERIC_RANGE_NOT_CONTAINED;
};

// Unsigned to signed: Dst is guaranteed to contain source only if its range is
// larger.
template <typename Dst, typename Src>
struct StaticDstRangeRelationToSrcRange<Dst,
                                        Src,
                                        INTEGER_REPRESENTATION_SIGNED,
                                        INTEGER_REPRESENTATION_UNSIGNED> {
  static const NumericRangeRepresentation value =
      MaxExponent<Dst>::value > MaxExponent<Src>::value
          ? NUMERIC_RANGE_CONTAINED
          : NUMERIC_RANGE_NOT_CONTAINED;
};

// Signed to unsigned: Dst cannot be statically determined to contain Src.
template <typename Dst, typename Src>
struct StaticDstRangeRelationToSrcRange<Dst,
                                        Src,
                                        INTEGER_REPRESENTATION_UNSIGNED,
                                        INTEGER_REPRESENTATION_SIGNED> {
  static const NumericRangeRepresentation value = NUMERIC_RANGE_NOT_CONTAINED;
};

enum RangeConstraint {
  RANGE_VALID = 0x0,  // Value can be represented by the destination type.
  RANGE_UNDERFLOW = 0x1,  // Value would overflow.
  RANGE_OVERFLOW = 0x2,  // Value would underflow.
  RANGE_INVALID = RANGE_UNDERFLOW | RANGE_OVERFLOW  // Invalid (i.e. NaN).
};

// Helper function for coercing an int back to a RangeContraint.
constexpr RangeConstraint GetRangeConstraint(int integer_range_constraint) {
  // TODO(jschuh): Once we get full C++14 support we want this
  // assert(integer_range_constraint >= RANGE_VALID &&
  //        integer_range_constraint <= RANGE_INVALID)
  return static_cast<RangeConstraint>(integer_range_constraint);
}

// This function creates a RangeConstraint from an upper and lower bound
// check by taking advantage of the fact that only NaN can be out of range in
// both directions at once.
constexpr inline RangeConstraint GetRangeConstraint(bool is_in_upper_bound,
                                                    bool is_in_lower_bound) {
  return GetRangeConstraint((is_in_upper_bound ? 0 : RANGE_OVERFLOW) |
                            (is_in_lower_bound ? 0 : RANGE_UNDERFLOW));
}

// The following helper template addresses a corner case in range checks for
// conversion from a floating-point type to an integral type of smaller range
// but larger precision (e.g. float -> unsigned). The problem is as follows:
//   1. Integral maximum is always one less than a power of two, so it must be
//      truncated to fit the mantissa of the floating point. The direction of
//      rounding is implementation defined, but by default it's always IEEE
//      floats, which round to nearest and thus result in a value of larger
//      magnitude than the integral value.
//      Example: float f = UINT_MAX; // f is 4294967296f but UINT_MAX
//                                   // is 4294967295u.
//   2. If the floating point value is equal to the promoted integral maximum
//      value, a range check will erroneously pass.
//      Example: (4294967296f <= 4294967295u) // This is true due to a precision
//                                            // loss in rounding up to float.
//   3. When the floating point value is then converted to an integral, the
//      resulting value is out of range for the target integral type and
//      thus is implementation defined.
//      Example: unsigned u = (float)INT_MAX; // u will typically overflow to 0.
// To fix this bug we manually truncate the maximum value when the destination
// type is an integral of larger precision than the source floating-point type,
// such that the resulting maximum is represented exactly as a floating point.
template <typename Dst, typename Src>
struct NarrowingRange {
  typedef typename std::numeric_limits<Src> SrcLimits;
  typedef typename std::numeric_limits<Dst> DstLimits;
  // The following logic avoids warnings where the max function is
  // instantiated with invalid values for a bit shift (even though
  // such a function can never be called).
  static const int shift = (MaxExponent<Src>::value > MaxExponent<Dst>::value &&
                            SrcLimits::digits < DstLimits::digits &&
                            SrcLimits::is_iec559 &&
                            DstLimits::is_integer)
                               ? (DstLimits::digits - SrcLimits::digits)
                               : 0;

  static constexpr Dst max() {
    // We use UINTMAX_C below to avoid compiler warnings about shifting floating
    // points. Since it's a compile time calculation, it shouldn't have any
    // performance impact.
    return DstLimits::max() - static_cast<Dst>((UINTMAX_C(1) << shift) - 1);
  }

  static constexpr Dst min() {
    return std::numeric_limits<Dst>::is_iec559 ? -DstLimits::max()
                                               : DstLimits::min();
  }
};

template <
    typename Dst,
    typename Src,
    IntegerRepresentation DstSign = std::numeric_limits<Dst>::is_signed
                                            ? INTEGER_REPRESENTATION_SIGNED
                                            : INTEGER_REPRESENTATION_UNSIGNED,
    IntegerRepresentation SrcSign = std::numeric_limits<Src>::is_signed
                                            ? INTEGER_REPRESENTATION_SIGNED
                                            : INTEGER_REPRESENTATION_UNSIGNED,
    NumericRangeRepresentation DstRange =
        StaticDstRangeRelationToSrcRange<Dst, Src>::value >
struct DstRangeRelationToSrcRangeImpl;

// The following templates are for ranges that must be verified at runtime. We
// split it into checks based on signedness to avoid confusing casts and
// compiler warnings on signed an unsigned comparisons.

// Dst range is statically determined to contain Src: Nothing to check.
template <typename Dst,
          typename Src,
          IntegerRepresentation DstSign,
          IntegerRepresentation SrcSign>
struct DstRangeRelationToSrcRangeImpl<Dst,
                                      Src,
                                      DstSign,
                                      SrcSign,
                                      NUMERIC_RANGE_CONTAINED> {
  static constexpr RangeConstraint Check(Src value) { return RANGE_VALID; }
};

// Signed to signed narrowing: Both the upper and lower boundaries may be
// exceeded.
template <typename Dst, typename Src>
struct DstRangeRelationToSrcRangeImpl<Dst,
                                      Src,
                                      INTEGER_REPRESENTATION_SIGNED,
                                      INTEGER_REPRESENTATION_SIGNED,
                                      NUMERIC_RANGE_NOT_CONTAINED> {
  static constexpr RangeConstraint Check(Src value) {
    return GetRangeConstraint((value <= NarrowingRange<Dst, Src>::max()),
                              (value >= NarrowingRange<Dst, Src>::min()));
  }
};

// Unsigned to unsigned narrowing: Only the upper boundary can be exceeded.
template <typename Dst, typename Src>
struct DstRangeRelationToSrcRangeImpl<Dst,
                                      Src,
                                      INTEGER_REPRESENTATION_UNSIGNED,
                                      INTEGER_REPRESENTATION_UNSIGNED,
                                      NUMERIC_RANGE_NOT_CONTAINED> {
  static constexpr RangeConstraint Check(Src value) {
    return GetRangeConstraint(value <= NarrowingRange<Dst, Src>::max(), true);
  }
};

// Unsigned to signed: The upper boundary may be exceeded.
template <typename Dst, typename Src>
struct DstRangeRelationToSrcRangeImpl<Dst,
                                      Src,
                                      INTEGER_REPRESENTATION_SIGNED,
                                      INTEGER_REPRESENTATION_UNSIGNED,
                                      NUMERIC_RANGE_NOT_CONTAINED> {
  static constexpr RangeConstraint Check(Src value) {
    return sizeof(Dst) > sizeof(Src)
               ? RANGE_VALID
               : GetRangeConstraint(
                     value <= static_cast<Src>(NarrowingRange<Dst, Src>::max()),
                     true);
  }
};

// Signed to unsigned: The upper boundary may be exceeded for a narrower Dst,
// and any negative value exceeds the lower boundary.
template <typename Dst, typename Src>
struct DstRangeRelationToSrcRangeImpl<Dst,
                                      Src,
                                      INTEGER_REPRESENTATION_UNSIGNED,
                                      INTEGER_REPRESENTATION_SIGNED,
                                      NUMERIC_RANGE_NOT_CONTAINED> {
  static constexpr RangeConstraint Check(Src value) {
    return (MaxExponent<Dst>::value >= MaxExponent<Src>::value)
               ? GetRangeConstraint(true, value >= static_cast<Src>(0))
               : GetRangeConstraint(
                     value <= static_cast<Src>(NarrowingRange<Dst, Src>::max()),
                     value >= static_cast<Src>(0));
  }
};

template <typename Dst, typename Src>
constexpr RangeConstraint DstRangeRelationToSrcRange(Src value) {
  static_assert(std::numeric_limits<Src>::is_specialized,
                "Argument must be numeric.");
  static_assert(std::numeric_limits<Dst>::is_specialized,
                "Result must be numeric.");
  return DstRangeRelationToSrcRangeImpl<Dst, Src>::Check(value);
}

// Integer promotion templates used by the portable checked integer arithmetic.
template <size_t Size, bool IsSigned>
struct IntegerForSizeAndSign;
template <>
struct IntegerForSizeAndSign<1, true> {
  typedef int8_t type;
};
template <>
struct IntegerForSizeAndSign<1, false> {
  typedef uint8_t type;
};
template <>
struct IntegerForSizeAndSign<2, true> {
  typedef int16_t type;
};
template <>
struct IntegerForSizeAndSign<2, false> {
  typedef uint16_t type;
};
template <>
struct IntegerForSizeAndSign<4, true> {
  typedef int32_t type;
};
template <>
struct IntegerForSizeAndSign<4, false> {
  typedef uint32_t type;
};
template <>
struct IntegerForSizeAndSign<8, true> {
  typedef int64_t type;
};
template <>
struct IntegerForSizeAndSign<8, false> {
  typedef uint64_t type;
  static_assert(sizeof(uintmax_t) == 8,
                "Max integer size not supported for this toolchain.");
};

// WARNING: We have no IntegerForSizeAndSign<16, *>. If we ever add one to
// support 128-bit math, then the ArithmeticPromotion template below will need
// to be updated (or more likely replaced with a decltype expression).

template <typename Integer>
struct UnsignedIntegerForSize {
  typedef typename std::enable_if<
      std::numeric_limits<Integer>::is_integer,
      typename IntegerForSizeAndSign<sizeof(Integer), false>::type>::type type;
};

template <typename Integer>
struct SignedIntegerForSize {
  typedef typename std::enable_if<
      std::numeric_limits<Integer>::is_integer,
      typename IntegerForSizeAndSign<sizeof(Integer), true>::type>::type type;
};

template <typename Integer>
struct TwiceWiderInteger {
  typedef typename std::enable_if<
      std::numeric_limits<Integer>::is_integer,
      typename IntegerForSizeAndSign<
          sizeof(Integer) * 2,
          std::numeric_limits<Integer>::is_signed>::type>::type type;
};

template <typename Integer>
struct PositionOfSignBit {
  static const typename std::enable_if<std::numeric_limits<Integer>::is_integer,
                                       size_t>::type value =
      CHAR_BIT * sizeof(Integer) - 1;
};

enum ArithmeticPromotionCategory {
  LEFT_PROMOTION,          // Use the type of the left-hand argument.
  RIGHT_PROMOTION,         // Use the type of the right-hand argument.
  MAX_EXPONENT_PROMOTION,  // Use the type supporting the largest exponent.
  BIG_ENOUGH_PROMOTION     // Attempt to find a big enough type.
};

template <ArithmeticPromotionCategory Promotion,
          typename Lhs,
          typename Rhs = Lhs>
struct ArithmeticPromotion;

template <typename Lhs,
          typename Rhs,
          ArithmeticPromotionCategory Promotion =
              (MaxExponent<Lhs>::value > MaxExponent<Rhs>::value)
                  ? LEFT_PROMOTION
                  : RIGHT_PROMOTION>
struct MaxExponentPromotion;

template <typename Lhs, typename Rhs>
struct MaxExponentPromotion<Lhs, Rhs, LEFT_PROMOTION> {
  using type = Lhs;
};

template <typename Lhs, typename Rhs>
struct MaxExponentPromotion<Lhs, Rhs, RIGHT_PROMOTION> {
  using type = Rhs;
};

template <typename Lhs,
          typename Rhs = Lhs,
          bool is_intmax_type =
              std::is_integral<
                  typename MaxExponentPromotion<Lhs, Rhs>::type>::value &&
              sizeof(typename MaxExponentPromotion<Lhs, Rhs>::type) ==
                  sizeof(intmax_t),
          bool is_max_exponent =
              StaticDstRangeRelationToSrcRange<
                  typename MaxExponentPromotion<Lhs, Rhs>::type,
                  Lhs>::value ==
              NUMERIC_RANGE_CONTAINED&& StaticDstRangeRelationToSrcRange<
                  typename MaxExponentPromotion<Lhs, Rhs>::type,
                  Rhs>::value == NUMERIC_RANGE_CONTAINED>
struct BigEnoughPromotion;

// The side with the max exponent is big enough.
template <typename Lhs, typename Rhs, bool is_intmax_type>
struct BigEnoughPromotion<Lhs, Rhs, is_intmax_type, true> {
  using type = typename MaxExponentPromotion<Lhs, Rhs>::type;
  static const bool is_contained = true;
};

// We can use a twice wider type to fit.
template <typename Lhs, typename Rhs>
struct BigEnoughPromotion<Lhs, Rhs, false, false> {
  using type = typename IntegerForSizeAndSign<
      sizeof(typename MaxExponentPromotion<Lhs, Rhs>::type) * 2,
      std::is_signed<Lhs>::value || std::is_signed<Rhs>::value>::type;
  static const bool is_contained = true;
};

// No type is large enough.
template <typename Lhs, typename Rhs>
struct BigEnoughPromotion<Lhs, Rhs, true, false> {
  using type = typename MaxExponentPromotion<Lhs, Rhs>::type;
  static const bool is_contained = false;
};

// These are the supported promotion types.

// Use the type supporting the largest exponent.
template <typename Lhs, typename Rhs>
struct ArithmeticPromotion<MAX_EXPONENT_PROMOTION, Lhs, Rhs> {
  using type = typename MaxExponentPromotion<Lhs, Rhs>::type;
  static const bool is_contained = true;
};

// Attempt to find a big enough type.
template <typename Lhs, typename Rhs>
struct ArithmeticPromotion<BIG_ENOUGH_PROMOTION, Lhs, Rhs> {
  using type = typename BigEnoughPromotion<Lhs, Rhs>::type;
  static const bool is_contained = BigEnoughPromotion<Lhs, Rhs>::is_contained;
};

// We can statically check if operations on the provided types can wrap, so we
// can skip the checked operations if they're not needed. So, for an integer we
// care if the destination type preserves the sign and is twice the width of
// the source.
template <typename T, typename Lhs, typename Rhs>
struct IsIntegerArithmeticSafe {
  static const bool value = !std::numeric_limits<T>::is_iec559 &&
                            StaticDstRangeRelationToSrcRange<T, Lhs>::value ==
                                NUMERIC_RANGE_CONTAINED &&
                            sizeof(T) >= (2 * sizeof(Lhs)) &&
                            StaticDstRangeRelationToSrcRange<T, Rhs>::value !=
                                NUMERIC_RANGE_CONTAINED &&
                            sizeof(T) >= (2 * sizeof(Rhs));
};

// This hacks around libstdc++ 4.6 missing stuff in type_traits.
#if defined(__GLIBCXX__)
#define PRIV_GLIBCXX_4_7_0 20120322
#define PRIV_GLIBCXX_4_5_4 20120702
#define PRIV_GLIBCXX_4_6_4 20121127
#if (__GLIBCXX__ < PRIV_GLIBCXX_4_7_0 || __GLIBCXX__ == PRIV_GLIBCXX_4_5_4 || \
     __GLIBCXX__ == PRIV_GLIBCXX_4_6_4)
#define PRIV_USE_FALLBACKS_FOR_OLD_GLIBCXX
#undef PRIV_GLIBCXX_4_7_0
#undef PRIV_GLIBCXX_4_5_4
#undef PRIV_GLIBCXX_4_6_4
#endif
#endif

// Extracts the underlying type from an enum.
template <typename T, bool is_enum = std::is_enum<T>::value>
struct ArithmeticOrUnderlyingEnum;

template <typename T>
struct ArithmeticOrUnderlyingEnum<T, true> {
#if defined(PRIV_USE_FALLBACKS_FOR_OLD_GLIBCXX)
  using type = __underlying_type(T);
#else
  using type = typename std::underlying_type<T>::type;
#endif
  static const bool value = std::is_arithmetic<type>::value;
};

#if defined(PRIV_USE_FALLBACKS_FOR_OLD_GLIBCXX)
#undef PRIV_USE_FALLBACKS_FOR_OLD_GLIBCXX
#endif

template <typename T>
struct ArithmeticOrUnderlyingEnum<T, false> {
  using type = T;
  static const bool value = std::is_arithmetic<type>::value;
};

// The following are helper templates used in the CheckedNumeric class.
template <typename T>
class CheckedNumeric;

template <typename T>
class StrictNumeric;

// Used to treat CheckedNumeric and arithmetic underlying types the same.
template <typename T>
struct UnderlyingType {
  using type = typename ArithmeticOrUnderlyingEnum<T>::type;
  static const bool is_numeric = std::is_arithmetic<type>::value;
  static const bool is_checked = false;
  static const bool is_strict = false;
};

template <typename T>
struct UnderlyingType<CheckedNumeric<T>> {
  using type = T;
  static const bool is_numeric = true;
  static const bool is_checked = true;
  static const bool is_strict = false;
};

template <typename T>
struct UnderlyingType<StrictNumeric<T>> {
  using type = T;
  static const bool is_numeric = true;
  static const bool is_checked = false;
  static const bool is_strict = true;
};

template <typename L, typename R>
struct IsCheckedOp {
  static const bool value =
      UnderlyingType<L>::is_numeric && UnderlyingType<R>::is_numeric &&
      (UnderlyingType<L>::is_checked || UnderlyingType<R>::is_checked);
};

template <typename L, typename R>
struct IsStrictOp {
  static const bool value =
      UnderlyingType<L>::is_numeric && UnderlyingType<R>::is_numeric &&
      (UnderlyingType<L>::is_strict || UnderlyingType<R>::is_strict);
};

template <typename L, typename R>
constexpr bool IsLessImpl(const L lhs,
                          const R rhs,
                          const RangeConstraint l_range,
                          const RangeConstraint r_range) {
  return l_range == RANGE_UNDERFLOW || r_range == RANGE_OVERFLOW ||
         (l_range == r_range &&
          static_cast<decltype(lhs + rhs)>(lhs) <
              static_cast<decltype(lhs + rhs)>(rhs));
}

template <typename L, typename R>
struct IsLess {
  static_assert(std::is_arithmetic<L>::value && std::is_arithmetic<R>::value,
                "Types must be numeric.");
  static constexpr bool Test(const L lhs, const R rhs) {
    return IsLessImpl(lhs, rhs, DstRangeRelationToSrcRange<R>(lhs),
                      DstRangeRelationToSrcRange<L>(rhs));
  }
};

template <typename L, typename R>
constexpr bool IsLessOrEqualImpl(const L lhs,
                                 const R rhs,
                                 const RangeConstraint l_range,
                                 const RangeConstraint r_range) {
  return l_range == RANGE_UNDERFLOW || r_range == RANGE_OVERFLOW ||
         (l_range == r_range &&
          static_cast<decltype(lhs + rhs)>(lhs) <=
              static_cast<decltype(lhs + rhs)>(rhs));
}

template <typename L, typename R>
struct IsLessOrEqual {
  static_assert(std::is_arithmetic<L>::value && std::is_arithmetic<R>::value,
                "Types must be numeric.");
  static constexpr bool Test(const L lhs, const R rhs) {
    return IsLessOrEqualImpl(lhs, rhs, DstRangeRelationToSrcRange<R>(lhs),
                             DstRangeRelationToSrcRange<L>(rhs));
  }
};

template <typename L, typename R>
constexpr bool IsGreaterImpl(const L lhs,
                             const R rhs,
                             const RangeConstraint l_range,
                             const RangeConstraint r_range) {
  return l_range == RANGE_OVERFLOW || r_range == RANGE_UNDERFLOW ||
         (l_range == r_range &&
          static_cast<decltype(lhs + rhs)>(lhs) >
              static_cast<decltype(lhs + rhs)>(rhs));
}

template <typename L, typename R>
struct IsGreater {
  static_assert(std::is_arithmetic<L>::value && std::is_arithmetic<R>::value,
                "Types must be numeric.");
  static constexpr bool Test(const L lhs, const R rhs) {
    return IsGreaterImpl(lhs, rhs, DstRangeRelationToSrcRange<R>(lhs),
                         DstRangeRelationToSrcRange<L>(rhs));
  }
};

template <typename L, typename R>
constexpr bool IsGreaterOrEqualImpl(const L lhs,
                                    const R rhs,
                                    const RangeConstraint l_range,
                                    const RangeConstraint r_range) {
  return l_range == RANGE_OVERFLOW || r_range == RANGE_UNDERFLOW ||
         (l_range == r_range &&
          static_cast<decltype(lhs + rhs)>(lhs) >=
              static_cast<decltype(lhs + rhs)>(rhs));
}

template <typename L, typename R>
struct IsGreaterOrEqual {
  static_assert(std::is_arithmetic<L>::value && std::is_arithmetic<R>::value,
                "Types must be numeric.");
  static constexpr bool Test(const L lhs, const R rhs) {
    return IsGreaterOrEqualImpl(lhs, rhs, DstRangeRelationToSrcRange<R>(lhs),
                                DstRangeRelationToSrcRange<L>(rhs));
  }
};

template <typename L, typename R>
struct IsEqual {
  static_assert(std::is_arithmetic<L>::value && std::is_arithmetic<R>::value,
                "Types must be numeric.");
  static constexpr bool Test(const L lhs, const R rhs) {
    return DstRangeRelationToSrcRange<R>(lhs) ==
               DstRangeRelationToSrcRange<L>(rhs) &&
           static_cast<decltype(lhs + rhs)>(lhs) ==
               static_cast<decltype(lhs + rhs)>(rhs);
  }
};

template <typename L, typename R>
struct IsNotEqual {
  static_assert(std::is_arithmetic<L>::value && std::is_arithmetic<R>::value,
                "Types must be numeric.");
  static constexpr bool Test(const L lhs, const R rhs) {
    return DstRangeRelationToSrcRange<R>(lhs) !=
               DstRangeRelationToSrcRange<L>(rhs) ||
           static_cast<decltype(lhs + rhs)>(lhs) !=
               static_cast<decltype(lhs + rhs)>(rhs);
  }
};

// These perform the actual math operations on the CheckedNumerics.
// Binary arithmetic operations.
template <template <typename, typename> class C, typename L, typename R>
constexpr bool SafeCompare(const L lhs, const R rhs) {
  static_assert(std::is_arithmetic<L>::value && std::is_arithmetic<R>::value,
                "Types must be numeric.");
  using Promotion = ArithmeticPromotion<BIG_ENOUGH_PROMOTION, L, R>;
  using BigType = typename Promotion::type;
  return Promotion::is_contained
             // Force to a larger type for speed if both are contained.
             ? C<BigType, BigType>::Test(
                   static_cast<BigType>(static_cast<L>(lhs)),
                   static_cast<BigType>(static_cast<R>(rhs)))
             // Let the template functions figure it out for mixed types.
             : C<L, R>::Test(lhs, rhs);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_NUMERICS_SAFE_CONVERSIONS_IMPL_H_
