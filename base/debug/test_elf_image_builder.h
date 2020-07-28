// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DEBUG_TEST_ELF_IMAGE_BUILDER_H_
#define BASE_DEBUG_TEST_ELF_IMAGE_BUILDER_H_

#include <elf.h>

#include <cstdint>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"

#if __SIZEOF_POINTER__ == 4
using Ehdr = Elf32_Ehdr;
using Half = Elf32_Half;
using Off = Elf32_Off;
using Phdr = Elf32_Phdr;
using Word = Elf32_Word;
#else
using Ehdr = Elf64_Ehdr;
using Half = Elf64_Half;
using Off = Elf64_Off;
using Phdr = Elf64_Phdr;
using Word = Elf64_Word;
#endif

namespace base {

// In-memory ELF image constructed by TestElfImageBuilder.
class TestElfImage {
 public:
  // |buffer| is a memory buffer containing the ELF image. |elf_start| is the
  // start address of the ELF image within the buffer.
  TestElfImage(std::vector<uint8_t> buffer, const void* elf_start);
  ~TestElfImage();

  TestElfImage(TestElfImage&&);
  TestElfImage& operator=(TestElfImage&&);

  // The start address of the ELF image.
  const void* elf_start() const { return elf_start_; }

 private:
  std::vector<uint8_t> buffer_;
  const void* elf_start_;
};

// Builds an in-memory image of an ELF file for testing.
class TestElfImageBuilder {
 public:
  TestElfImageBuilder();
  ~TestElfImageBuilder();

  TestElfImageBuilder(const TestElfImageBuilder&) = delete;
  TestElfImageBuilder& operator=(const TestElfImageBuilder&) = delete;

  // Add a PT_LOAD segment with the specified rwx |flags|. The contents will be
  // filled with |size| bytes of zeros.
  TestElfImageBuilder& AddLoadSegment(Word flags, size_t size);

  // Add a PT_NOTE segment with the specified state.
  TestElfImageBuilder& AddNoteSegment(Word type,
                                      StringPiece name,
                                      span<const uint8_t> desc);

  // Adds a DT_SONAME dynamic section and the necessary state to support it. May
  // be invoked at most once.
  TestElfImageBuilder& AddSoName(StringPiece soname);

  TestElfImage Build();

 private:
  // Properties of a load segment to create.
  struct LoadSegment;

  // Computed sizing state for parts of the ELF image.
  struct ImageMeasures;

  // Measures sizes/start offset of segments in the image.
  ImageMeasures MeasureSizesAndOffsets() const;

  // Appends a header of type |T| at |loc|, a memory address within the ELF
  // image being constructed, and returns the address past the header.
  template <typename T>
  static uint8_t* AppendHdr(const T& hdr, uint8_t* loc);

  Ehdr CreateEhdr(Half phnum);
  Phdr CreatePhdr(Word type, Word flags, size_t align, Off offset, size_t size);

  std::vector<std::vector<uint8_t>> note_contents_;
  std::vector<LoadSegment> load_segments_;
  Optional<std::string> soname_;
};

}  // namespace base

#endif  // BASE_DEBUG_TEST_ELF_IMAGE_BUILDER_H_
