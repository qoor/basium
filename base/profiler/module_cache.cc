// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/module_cache.h"

#include <utility>

namespace base {

ModuleCache::ModuleCache() = default;
ModuleCache::~ModuleCache() = default;

const ModuleCache::Module* ModuleCache::GetModuleForAddress(uintptr_t address) {
  const Module* module = FindModuleForAddress(non_native_modules_, address);
  if (module)
    return module;

  module = FindModuleForAddress(native_modules_, address);
  if (module)
    return module;

  std::unique_ptr<const Module> new_module = CreateModuleForAddress(address);
  if (!new_module)
    return nullptr;
  native_modules_.push_back(std::move(new_module));
  return native_modules_.back().get();
}

std::vector<const ModuleCache::Module*> ModuleCache::GetModules() const {
  std::vector<const Module*> result;
  result.reserve(native_modules_.size());
  for (const std::unique_ptr<const Module>& module : native_modules_)
    result.push_back(module.get());
  for (const std::unique_ptr<const Module>& module : non_native_modules_)
    result.push_back(module.get());
  return result;
}

void ModuleCache::AddNonNativeModule(std::unique_ptr<const Module> module) {
  DCHECK(!module->IsNative());
  non_native_modules_.push_back(std::move(module));
}

void ModuleCache::InjectNativeModuleForTesting(
    std::unique_ptr<const Module> module) {
  DCHECK(module->IsNative());
  native_modules_.push_back(std::move(module));
}

// static
const ModuleCache::Module* ModuleCache::FindModuleForAddress(
    const std::vector<std::unique_ptr<const Module>>& modules,
    uintptr_t address) {
  auto it = std::find_if(modules.begin(), modules.end(),
                         [address](
                             const std::unique_ptr<const Module>& module) {
                           return address >= module->GetBaseAddress() &&
                                  address < module->GetBaseAddress() +
                                                module->GetSize();
                         });
  return it != modules.end() ? it->get() : nullptr;
}

}  // namespace base
