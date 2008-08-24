// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GFX_PLATFORM_DEVICE_MAC_H__
#define BASE_GFX_PLATFORM_DEVICE_MAC_H__

#import <ApplicationServices/ApplicationServices.h>
#include "SkDevice.h"

class SkMatrix;
class SkPath;
class SkRegion;

namespace gfx {

// A device is basically a wrapper around SkBitmap that provides a surface for
// SkCanvas to draw into. Our device provides a surface CoreGraphics can also
// write to. It also provides functionality to play well with CG drawing
// functions.
// This class is abstract and must be subclassed. It provides the basic
// interface to implement it either with or without a bitmap backend.
class PlatformDeviceMac : public SkDevice {
 public:
  // The CGContext that corresponds to the bitmap, used for CoreGraphics
  // operations drawing into the bitmap. This is possibly heavyweight, so it
  // should exist only during one pass of rendering.
  virtual CGContextRef GetBitmapContext() = 0;

  // Translate the device's coordinate system by the given amount; this will
  // override any previous calls to this function.
  virtual void SetTransform(const SkMatrix& matrix) = 0;

  // Devices may be in a layer and offset from the root device. In this case,
  // the transform (set by setTransform) will corrspond to the root device, and
  // this device will actually be offset from there.
  //
  // This is called after a layered device is created to tell us the location.
  // This location will be factored into any transforms applied via
  // setTransform.
  //
  // If this function is not called, the offset defaults to (0, 0);
  virtual void SetDeviceOffset(int x, int y) = 0;

  // Sets the clipping region, overriding any previous calls.
  virtual void SetClipRegion(const SkRegion& region) = 0;

  // Draws to the given graphics context. If the bitmap context doesn't exist,
  // this will temporarily create it. However, if you have created the bitmap
  // context, it will be more efficient if you don't free it until after this
  // call so it doesn't have to be created twice.  If src_rect is null, then
  // the entirety of the source device will be copied.
  virtual void DrawToContext(CGContextRef context, int x, int y,
                             const CGRect* src_rect) = 0;

  // Sets the opacity of each pixel in the specified region to be opaque.
  void makeOpaque(int x, int y, int width, int height);

  // Returns if the preferred rendering engine is vectorial or bitmap based.
  virtual bool IsVectorial() = 0;

  // Initializes the default settings and colors in a device context.
  static void InitializeCGContext(CGContextRef context);

  // Loads a SkPath into the CG context. The path can there after be used for
  // clipping or as a stroke.
  static void LoadPathToCGContext(CGContextRef context, const SkPath& path);

 protected:
  // Forwards |bitmap| to SkDevice's constructor.
  PlatformDeviceMac(const SkBitmap& bitmap);

  // Loads the specified Skia transform into the device context
  static void LoadTransformToCGContext(CGContextRef context,
                                       const SkMatrix& matrix);

  // Function pointer used by the processPixels method for setting the alpha
  // value of a particular pixel.
  typedef void (*adjustAlpha)(uint32_t* pixel);
  
  // Loops through each of the pixels in the specified range, invoking
  // adjustor for the alpha value of each pixel.
  virtual void processPixels(int x,
                             int y,
                             int width,
                             int height,
                             adjustAlpha adjustor) = 0;
};

}  // namespace gfx

#endif  // BASE_GFX_PLATFORM_DEVICE_MAC_H__

