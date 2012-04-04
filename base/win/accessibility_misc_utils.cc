// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/win/accessibility_misc_utils.h"

#include "base/logging.h"

namespace base {
namespace win {

// UIA TextProvider implementation.
UIATextProvider::UIATextProvider()
    : editable_(false) {}

// static
bool UIATextProvider::CreateTextProvider(bool editable, IUnknown** provider) {
  CComObject<UIATextProvider>* text_provider = NULL;
  HRESULT hr = CComObject<UIATextProvider>::CreateInstance(&text_provider);
  if (SUCCEEDED(hr)) {
    DCHECK(text_provider);
    text_provider->set_editable(editable);
    text_provider->AddRef();
    *provider = static_cast<ITextProvider*>(text_provider);
    return true;
  }
  return false;
}

STDMETHODIMP UIATextProvider::get_IsReadOnly(BOOL* read_only) {
  *read_only = !editable_;
  return S_OK;
}

}  // namespace win
}  // namespace base
