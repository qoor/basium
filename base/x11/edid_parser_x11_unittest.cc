// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/x11/edid_parser_x11.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <X11/extensions/Xrandr.h>

namespace base {

namespace {

// Returns the number of characters in the string literal but doesn't count its
// terminator NULL byte.
#define charsize(str) (arraysize(str) - 1)

// Sample EDID data extracted from real devices.
const unsigned char kNormalDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x22\xf0\x6c\x28\x01\x01\x01\x01"
    "\x02\x16\x01\x04\xb5\x40\x28\x78\xe2\x8d\x85\xad\x4f\x35\xb1\x25"
    "\x0e\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xe2\x68\x00\xa0\xa0\x40\x2e\x60\x30\x20"
    "\x36\x00\x81\x90\x21\x00\x00\x1a\xbc\x1b\x00\xa0\x50\x20\x17\x30"
    "\x30\x20\x36\x00\x81\x90\x21\x00\x00\x1a\x00\x00\x00\xfc\x00\x48"
    "\x50\x20\x5a\x52\x33\x30\x77\x0a\x20\x20\x20\x20\x00\x00\x00\xff"
    "\x00\x43\x4e\x34\x32\x30\x32\x31\x33\x37\x51\x0a\x20\x20\x00\x71";

const unsigned char kInternalDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x4c\xa3\x42\x31\x00\x00\x00\x00"
    "\x00\x15\x01\x03\x80\x1a\x10\x78\x0a\xd3\xe5\x95\x5c\x60\x90\x27"
    "\x19\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\x9e\x1b\x00\xa0\x50\x20\x12\x30\x10\x30"
    "\x13\x00\x05\xa3\x10\x00\x00\x19\x00\x00\x00\x0f\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x23\x87\x02\x64\x00\x00\x00\x00\xfe\x00\x53"
    "\x41\x4d\x53\x55\x4e\x47\x0a\x20\x20\x20\x20\x20\x00\x00\x00\xfe"
    "\x00\x31\x32\x31\x41\x54\x31\x31\x2d\x38\x30\x31\x0a\x20\x00\x45";

const unsigned char kOverscanDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x4c\x2d\xfe\x08\x00\x00\x00\x00"
    "\x29\x15\x01\x03\x80\x10\x09\x78\x0a\xee\x91\xa3\x54\x4c\x99\x26"
    "\x0f\x50\x54\xbd\xef\x80\x71\x4f\x81\xc0\x81\x00\x81\x80\x95\x00"
    "\xa9\xc0\xb3\x00\x01\x01\x02\x3a\x80\x18\x71\x38\x2d\x40\x58\x2c"
    "\x45\x00\xa0\x5a\x00\x00\x00\x1e\x66\x21\x56\xaa\x51\x00\x1e\x30"
    "\x46\x8f\x33\x00\xa0\x5a\x00\x00\x00\x1e\x00\x00\x00\xfd\x00\x18"
    "\x4b\x0f\x51\x17\x00\x0a\x20\x20\x20\x20\x20\x20\x00\x00\x00\xfc"
    "\x00\x53\x41\x4d\x53\x55\x4e\x47\x0a\x20\x20\x20\x20\x20\x01\x1d"
    "\x02\x03\x1f\xf1\x47\x90\x04\x05\x03\x20\x22\x07\x23\x09\x07\x07"
    "\x83\x01\x00\x00\xe2\x00\x0f\x67\x03\x0c\x00\x20\x00\xb8\x2d\x01"
    "\x1d\x80\x18\x71\x1c\x16\x20\x58\x2c\x25\x00\xa0\x5a\x00\x00\x00"
    "\x9e\x01\x1d\x00\x72\x51\xd0\x1e\x20\x6e\x28\x55\x00\xa0\x5a\x00"
    "\x00\x00\x1e\x8c\x0a\xd0\x8a\x20\xe0\x2d\x10\x10\x3e\x96\x00\xa0"
    "\x5a\x00\x00\x00\x18\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xc6";

const unsigned char kLP2565A[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x22\xF0\x76\x26\x01\x01\x01\x01"
    "\x02\x12\x01\x03\x80\x34\x21\x78\xEE\xEF\x95\xA3\x54\x4C\x9B\x26"
    "\x0F\x50\x54\xA5\x6B\x80\x81\x40\x81\x80\x81\x99\x71\x00\xA9\x00"
    "\xA9\x40\xB3\x00\xD1\x00\x28\x3C\x80\xA0\x70\xB0\x23\x40\x30\x20"
    "\x36\x00\x07\x44\x21\x00\x00\x1A\x00\x00\x00\xFD\x00\x30\x55\x1E"
    "\x5E\x11\x00\x0A\x20\x20\x20\x20\x20\x20\x00\x00\x00\xFC\x00\x48"
    "\x50\x20\x4C\x50\x32\x34\x36\x35\x0A\x20\x20\x20\x00\x00\x00\xFF"
    "\x00\x43\x4E\x4B\x38\x30\x32\x30\x34\x48\x4D\x0A\x20\x20\x00\xA4";

const unsigned char kLP2565B[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x22\xF0\x75\x26\x01\x01\x01\x01"
    "\x02\x12\x01\x03\x6E\x34\x21\x78\xEE\xEF\x95\xA3\x54\x4C\x9B\x26"
    "\x0F\x50\x54\xA5\x6B\x80\x81\x40\x71\x00\xA9\x00\xA9\x40\xA9\x4F"
    "\xB3\x00\xD1\xC0\xD1\x00\x28\x3C\x80\xA0\x70\xB0\x23\x40\x30\x20"
    "\x36\x00\x07\x44\x21\x00\x00\x1A\x00\x00\x00\xFD\x00\x30\x55\x1E"
    "\x5E\x15\x00\x0A\x20\x20\x20\x20\x20\x20\x00\x00\x00\xFC\x00\x48"
    "\x50\x20\x4C\x50\x32\x34\x36\x35\x0A\x20\x20\x20\x00\x00\x00\xFF"
    "\x00\x43\x4E\x4B\x38\x30\x32\x30\x34\x48\x4D\x0A\x20\x20\x00\x45";

}  // namespace

TEST(EdidParserX11Test, ParseEDID) {
  uint16 manufacturer_id = 0;
  std::string human_readable_name;
  EXPECT_TRUE(ParseOutputDeviceData(
      kNormalDisplay, charsize(kNormalDisplay),
      &manufacturer_id, &human_readable_name));
  EXPECT_EQ(0x22f0u, manufacturer_id);
  EXPECT_EQ("HP ZR30w", human_readable_name);

  manufacturer_id = 0;
  human_readable_name.clear();
  EXPECT_TRUE(ParseOutputDeviceData(
      kInternalDisplay, charsize(kInternalDisplay),
      &manufacturer_id, NULL));
  EXPECT_EQ(0x4ca3u, manufacturer_id);
  EXPECT_EQ("", human_readable_name);

  // Internal display doesn't have name.
  EXPECT_TRUE(ParseOutputDeviceData(
      kInternalDisplay, charsize(kInternalDisplay),
      NULL, &human_readable_name));
  EXPECT_TRUE(human_readable_name.empty());

  manufacturer_id = 0;
  human_readable_name.clear();
  EXPECT_TRUE(ParseOutputDeviceData(
      kOverscanDisplay, charsize(kOverscanDisplay),
      &manufacturer_id, &human_readable_name));
  EXPECT_EQ(0x4c2du, manufacturer_id);
  EXPECT_EQ("SAMSUNG", human_readable_name);
}

TEST(EdidParserX11Test, ParseBrokenEDID) {
  uint16 manufacturer_id = 0;
  std::string human_readable_name;

  // length == 0
  EXPECT_FALSE(ParseOutputDeviceData(
      kNormalDisplay, 0,
      &manufacturer_id, &human_readable_name));

  // name is broken. Copying kNormalDisplay and substitute its name data by
  // some control code.
  std::string display_data(
      reinterpret_cast<const char*>(kNormalDisplay), charsize(kNormalDisplay));

  // display's name data is embedded in byte 95-107 in this specific example.
  // Fix here too when the contents of kNormalDisplay is altered.
  display_data[97] = '\x1b';
  EXPECT_FALSE(ParseOutputDeviceData(
      reinterpret_cast<const unsigned char*>(display_data.data()),
      display_data.size(),
      &manufacturer_id, &human_readable_name));

  // If |human_readable_name| isn't specified, it skips parsing the name.
  manufacturer_id = 0;
  EXPECT_TRUE(ParseOutputDeviceData(
      reinterpret_cast<const unsigned char*>(display_data.data()),
      display_data.size(),
      &manufacturer_id, NULL));
  EXPECT_EQ(0x22f0u, manufacturer_id);
}

TEST(EdidParserX11Test, GetDisplayId) {
  // EDID of kLP2565A and B are slightly different but actually the same device.
  int64 id1 = -1;
  int64 id2 = -1;
  EXPECT_TRUE(GetDisplayIdFromEDID(kLP2565A, charsize(kLP2565A), 0, &id1));
  EXPECT_TRUE(GetDisplayIdFromEDID(kLP2565B, charsize(kLP2565B), 0, &id2));
  EXPECT_EQ(id1, id2);
  EXPECT_NE(-1, id1);
}

TEST(EdidParserX11Test, GetDisplayIdFromInternal) {
  int64 id = -1;
  EXPECT_TRUE(GetDisplayIdFromEDID(
      kInternalDisplay, charsize(kInternalDisplay), 0, &id));
  EXPECT_NE(-1, id);
}

TEST(EdidParserX11Test, GetDisplayIdFailure) {
  int64 id = -1;
  EXPECT_FALSE(GetDisplayIdFromEDID(NULL, 0, 0, &id));
  EXPECT_EQ(-1, id);
}

}   // namespace base
