# Copyright 2022 MaBling <akck0918@gmail.com>. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include(${PROJECT_SOURCE_DIR}/base/allocator/allocator.cmake)
include(${PROJECT_SOURCE_DIR}/base/trace_event/tracing.cmake)
include(${PROJECT_SOURCE_DIR}/build/config/chromecast_build.cmake)
include(${PROJECT_SOURCE_DIR}/build/config/buildconfig.cmake)
include(${PROJECT_SOURCE_DIR}/build/BuildflagHeader.cmake)
include(${PROJECT_SOURCE_DIR}/build/config/chromeos/ui_mode.cmake)
include(${PROJECT_SOURCE_DIR}/build/config/compiler/compiler.cmake)
include(${PROJECT_SOURCE_DIR}/build/config/dcheck_always_on.cmake)
include(${PROJECT_SOURCE_DIR}/build/config/ios/config.cmake)
include(${PROJECT_SOURCE_DIR}/build/config/logging.cmake)
include(${PROJECT_SOURCE_DIR}/build/config/ui.cmake)
include(${PROJECT_SOURCE_DIR}/build_overrides/build.cmake)

set(ENABLE_LOCATION_SOURCE ON)
set(FROM_HERE_USES_LOCATION_BUILTINS ON)
set(IOS_STACK_PROFILER_ENABLED ON)

add_subdirectory(allocator)
add_subdirectory(numerics)

add_subdirectory(third_party/double_conversion)
add_subdirectory(third_party/dynamic_annotations)

set(BUILD_RUST_JSON_PARSER OFF)

buildflag_header(parsing_buildflags
  HEADER "parsing_buildflags.h"
  FLAGS BUILD_RUST_JSON_PARSER=${BUILD_RUST_JSON_PARSER}
)

buildflag_header(ios_cronet_buildflags
  HEADER "ios_cronet_buildflags.h"
  HEADER_DIR "base/message_loop"
  FLAGS CRONET_BUILD=${CRONET_BUILD})

if(NOT FUCHSIA AND NOT WIN32 AND NOT MAC AND NOT NACL)
  set(DEP_LIBEVENT ON)
else()
  set(DEP_LIBEVENT OFF)
endif()

if(DEP_LIBEVENT AND NOT IOS)
  set(USE_LIBEVENT ON)
else()
  set(USE_LIBEVENT OFF)
endif()

set(SOURCES
  allocator/allocator_check.cc
  allocator/allocator_check.h
  allocator/allocator_extension.cc
  allocator/allocator_extension.h
  as_const.h
  at_exit.cc
  at_exit.h
  atomic_ref_count.h
  atomic_sequence_num.h
  atomicops.h
  atomicops_internals_atomicword_compat.h
  atomicops_internals_portable.h
  auto_reset.h
  barrier_callback.h
  barrier_closure.cc
  barrier_closure.h
  base64.cc
  base64.h
  base64url.cc
  base64url.h
  base_export.h
  base_switches.h
  big_endian.cc
  big_endian.h
  bind.h
  bind_internal.h
  bit_cast.h
  bits.h
  # build_time.cc
  # build_time.h
  callback.h
  callback_forward.h
  callback_helpers.cc
  callback_helpers.h
  callback_internal.cc
  callback_internal.h
  callback_list.cc
  callback_list.h
  cancelable_callback.h
  check.cc
  check.h
  check_op.cc
  check_op.h
  command_line.cc
  command_line.h
  compiler_specific.h
  component_export.h
  containers/adapters.h
  containers/buffer_iterator.h
  containers/checked_iterators.h
  containers/checked_range.h
  containers/circular_deque.h
  containers/contains.h
  containers/contiguous_iterator.h
  containers/cxx20_erase.h
  containers/cxx20_erase_deque.h
  containers/cxx20_erase_forward_list.h
  containers/cxx20_erase_internal.h
  containers/cxx20_erase_list.h
  containers/cxx20_erase_map.h
  containers/cxx20_erase_set.h
  containers/cxx20_erase_string.h
  containers/cxx20_erase_unordered_map.h
  containers/cxx20_erase_unordered_set.h
  containers/cxx20_erase_vector.h
  containers/enum_set.h
  containers/extend.h
  containers/fixed_flat_map.h
  containers/fixed_flat_set.h
  containers/flat_map.h
  containers/flat_set.h
  containers/flat_tree.cc
  containers/flat_tree.h
  containers/id_map.h
  containers/intrusive_heap.cc
  containers/intrusive_heap.h
  containers/linked_list.cc
  containers/linked_list.h
  containers/lru_cache.h
  containers/small_map.h
  containers/span.h
  containers/stack.h
  containers/stack_container.h
  containers/unique_ptr_adapters.h
  containers/util.h
  containers/vector_buffer.h
  cpu.cc
  cpu.h
  cpu_reduction_experiment.cc
  cpu_reduction_experiment.h
  critical_closure.h
  cxx17_backports.h
  cxx20_to_address.h
  dcheck_is_on.h
  debug/activity_analyzer.cc
  debug/activity_analyzer.h
  debug/activity_tracker.cc
  debug/activity_tracker.h
  debug/alias.cc
  debug/alias.h
  debug/asan_invalid_access.cc
  debug/asan_invalid_access.h
  debug/buffered_dwarf_reader.cc
  debug/buffered_dwarf_reader.h
  debug/crash_logging.cc
  debug/crash_logging.h
  debug/debugger.cc
  debug/debugger.h
  debug/dump_without_crashing.cc
  debug/dump_without_crashing.h
  debug/dwarf_line_no.cc
  debug/dwarf_line_no.h
  debug/leak_annotations.h
  debug/profiler.cc
  debug/profiler.h
  debug/stack_trace.cc
  debug/stack_trace.h
  debug/task_trace.cc
  debug/task_trace.h
  enterprise_util.h
  environment.cc
  environment.h
  export_template.h
  feature_list.cc
  feature_list.h
  file_descriptor_store.cc
  file_descriptor_store.h
  file_version_info.h
  files/dir_reader_fallback.h
  files/file.cc
  files/file.h
  files/file_enumerator.cc
  files/file_enumerator.h
  files/file_error_or.h
  files/file_path.cc
  files/file_path.h
  files/file_path_constants.cc
  files/file_path_watcher.cc
  files/file_path_watcher.h
  files/file_proxy.cc
  files/file_proxy.h
  files/file_tracing.cc
  files/file_tracing.h
  files/file_util.cc
  files/file_util.h
  files/important_file_writer.cc
  files/important_file_writer.h
  files/important_file_writer_cleaner.cc
  files/important_file_writer_cleaner.h
  files/memory_mapped_file.cc
  files/memory_mapped_file.h
  files/platform_file.h
  files/safe_base_name.cc
  files/safe_base_name.h
  files/scoped_file.cc
  files/scoped_file.h
  files/scoped_temp_dir.cc
  files/scoped_temp_dir.h
  format_macros.h
  functional/identity.h
  functional/invoke.h
  functional/not_fn.h
  gtest_prod_util.h
  guid.cc
  guid.h
  hash/hash.cc
  hash/hash.h
  hash/legacy_hash.cc
  hash/legacy_hash.h
  immediate_crash.h
  json/json_common.h
  json/json_file_value_serializer.cc
  json/json_file_value_serializer.h
  json/json_parser.cc
  json/json_parser.h
  json/json_reader.cc
  json/json_reader.h
  json/json_string_value_serializer.cc
  json/json_string_value_serializer.h
  json/json_value_converter.cc
  json/json_value_converter.h
  json/json_writer.cc
  json/json_writer.h
  json/string_escape.cc
  json/string_escape.h
  json/values_util.cc
  json/values_util.h
  lazy_instance.h
  lazy_instance_helpers.cc
  lazy_instance_helpers.h
  linux_util.cc
  linux_util.h
  location.cc
  location.h
  logging.cc
  logging.h
  memory/aligned_memory.cc
  memory/aligned_memory.h
  memory/discardable_memory.cc
  memory/discardable_memory.h
  memory/discardable_memory_allocator.cc
  memory/discardable_memory_allocator.h
  memory/discardable_memory_internal.h
  memory/discardable_shared_memory.cc
  memory/discardable_shared_memory.h
  memory/free_deleter.h
  memory/memory_pressure_listener.cc
  memory/memory_pressure_listener.h
  memory/memory_pressure_monitor.cc
  memory/memory_pressure_monitor.h
  memory/nonscannable_memory.cc
  memory/nonscannable_memory.h
  memory/page_size.h
  memory/platform_shared_memory_region.cc
  memory/platform_shared_memory_region.h
  memory/ptr_util.h
  memory/raw_ptr.cc
  memory/raw_ptr.h
  memory/raw_scoped_refptr_mismatch_checker.h
  memory/read_only_shared_memory_region.cc
  memory/read_only_shared_memory_region.h
  memory/ref_counted.cc
  memory/ref_counted.h
  memory/ref_counted_delete_on_sequence.h
  memory/ref_counted_memory.cc
  memory/ref_counted_memory.h
  memory/safe_ref.h
  memory/scoped_policy.h
  memory/scoped_refptr.h
  memory/shared_memory_hooks.h
  memory/shared_memory_mapping.cc
  memory/shared_memory_mapping.h
  memory/shared_memory_security_policy.cc
  memory/shared_memory_security_policy.h
  memory/shared_memory_tracker.cc
  memory/shared_memory_tracker.h
  memory/singleton.h
  memory/unsafe_shared_memory_pool.cc
  memory/unsafe_shared_memory_pool.h
  memory/unsafe_shared_memory_region.cc
  memory/unsafe_shared_memory_region.h
  memory/values_equivalent.h
  memory/weak_ptr.cc
  memory/weak_ptr.h
  memory/writable_shared_memory_region.cc
  memory/writable_shared_memory_region.h
  message_loop/message_pump.cc
  message_loop/message_pump.h
  message_loop/message_pump_default.cc
  message_loop/message_pump_default.h
  message_loop/message_pump_for_io.h
  message_loop/message_pump_for_ui.h
  message_loop/message_pump_glib.cc
  message_loop/message_pump_glib.h
  message_loop/message_pump_type.h
  message_loop/timer_slack.cc
  message_loop/timer_slack.h
  message_loop/work_id_provider.cc
  message_loop/work_id_provider.h
  metrics/bucket_ranges.cc
  metrics/bucket_ranges.h
  metrics/crc32.cc
  metrics/crc32.h
  metrics/dummy_histogram.cc
  metrics/dummy_histogram.h
  metrics/field_trial.cc
  metrics/field_trial.h
  metrics/field_trial_param_associator.cc
  metrics/field_trial_param_associator.h
  metrics/field_trial_params.cc
  metrics/field_trial_params.h
  metrics/histogram.cc
  metrics/histogram.h
  metrics/histogram_base.cc
  metrics/histogram_base.h
  metrics/histogram_delta_serialization.cc
  metrics/histogram_delta_serialization.h
  metrics/histogram_flattener.h
  metrics/histogram_functions.cc
  metrics/histogram_functions.h
  metrics/histogram_macros.h
  metrics/histogram_macros_internal.h
  metrics/histogram_macros_local.h
  metrics/histogram_samples.cc
  metrics/histogram_samples.h
  metrics/histogram_snapshot_manager.cc
  metrics/histogram_snapshot_manager.h
  metrics/metrics_hashes.cc
  metrics/metrics_hashes.h
  metrics/persistent_histogram_allocator.cc
  metrics/persistent_histogram_allocator.h
  metrics/persistent_memory_allocator.cc
  metrics/persistent_memory_allocator.h
  metrics/persistent_sample_map.cc
  metrics/persistent_sample_map.h
  metrics/record_histogram_checker.h
  metrics/sample_map.cc
  metrics/sample_map.h
  metrics/sample_vector.cc
  metrics/sample_vector.h
  metrics/single_sample_metrics.cc
  metrics/single_sample_metrics.h
  metrics/sparse_histogram.cc
  metrics/sparse_histogram.h
  metrics/statistics_recorder.cc
  metrics/statistics_recorder.h
  metrics/user_metrics.cc
  metrics/user_metrics.h
  metrics/user_metrics_action.h
  native_library.cc
  native_library.h
  no_destructor.h
  notreached.cc
  notreached.h
  observer_list.h
  observer_list_internal.cc
  observer_list_internal.h
  observer_list_threadsafe.cc
  observer_list_threadsafe.h
  observer_list_types.cc
  observer_list_types.h
  one_shot_event.cc
  one_shot_event.h
  os_compat_nacl.cc
  os_compat_nacl.h
  parameter_pack.h
  path_service.cc
  path_service.h
  pending_task.cc
  pending_task.h
  pickle.cc
  pickle.h
  power_monitor/moving_average.cc
  power_monitor/moving_average.h
  power_monitor/power_monitor.cc
  power_monitor/power_monitor.h
  power_monitor/power_monitor_device_source.cc
  power_monitor/power_monitor_device_source.h
  power_monitor/power_monitor_source.cc
  power_monitor/power_monitor_source.h
  power_monitor/power_observer.h
  process/environment_internal.cc
  process/environment_internal.h
  process/kill.cc
  process/kill.h
  process/launch.cc
  process/launch.h
  process/memory.cc
  process/memory.h
  process/process.h
  process/process_handle.cc
  process/process_handle.h
  process/process_info.h
  process/process_iterator.cc
  process/process_iterator.h
  process/process_metrics.cc
  process/process_metrics.h
  process/process_metrics_iocounters.h
  profiler/arm_cfi_table.cc
  profiler/arm_cfi_table.h
  profiler/frame.cc
  profiler/frame.h
  profiler/metadata_recorder.cc
  profiler/metadata_recorder.h
  profiler/module_cache.cc
  profiler/module_cache.h
  profiler/native_unwinder.h
  profiler/profile_builder.h
  profiler/register_context.h
  profiler/sample_metadata.cc
  profiler/sample_metadata.h
  profiler/sampling_profiler_thread_token.cc
  profiler/sampling_profiler_thread_token.h
  profiler/stack_buffer.cc
  profiler/stack_buffer.h
  profiler/stack_copier.cc
  profiler/stack_copier.h
  profiler/stack_copier_suspend.cc
  profiler/stack_copier_suspend.h
  profiler/stack_sampler.cc
  profiler/stack_sampler.h
  profiler/stack_sampler_impl.cc
  profiler/stack_sampler_impl.h
  profiler/stack_sampling_profiler.cc
  profiler/stack_sampling_profiler.h
  profiler/suspendable_thread_delegate.h
  profiler/thread_delegate.h
  profiler/unwinder.cc
  profiler/unwinder.h
  rand_util.cc
  rand_util.h
  rand_util_nacl.cc
  ranges/algorithm.h
  ranges/functional.h
  ranges/ranges.h
  run_loop.cc
  run_loop.h
  sampling_heap_profiler/lock_free_address_hash_set.cc
  sampling_heap_profiler/lock_free_address_hash_set.h
  sampling_heap_profiler/poisson_allocation_sampler.cc
  sampling_heap_profiler/poisson_allocation_sampler.h
  sampling_heap_profiler/sampling_heap_profiler.cc
  sampling_heap_profiler/sampling_heap_profiler.h
  scoped_clear_last_error.h
  scoped_environment_variable_override.cc
  scoped_environment_variable_override.h
  scoped_generic.h
  scoped_multi_source_observation.h
  scoped_native_library.cc
  scoped_native_library.h
  scoped_observation.h
  sequence_checker.cc
  sequence_checker.h
  sequence_checker_impl.cc
  sequence_checker_impl.h
  sequence_token.cc
  sequence_token.h
  stl_util.h
  strings/abseil_string_conversions.cc
  strings/abseil_string_conversions.h
  strings/abseil_string_number_conversions.cc
  strings/abseil_string_number_conversions.h
  strings/char_traits.h
  strings/escape.cc
  strings/escape.h
  strings/latin1_string_conversions.cc
  strings/latin1_string_conversions.h
  strings/pattern.cc
  strings/pattern.h
  strings/safe_sprintf.cc
  strings/safe_sprintf.h
  strings/strcat.cc
  strings/strcat.h
  strings/strcat_internal.h
  strings/string_number_conversions.cc
  strings/string_number_conversions.h
  strings/string_number_conversions_internal.h
  strings/string_piece.cc
  strings/string_piece.h
  strings/string_piece_forward.h
  strings/string_split.cc
  strings/string_split.h
  strings/string_split_internal.h
  strings/string_tokenizer.h
  strings/string_util.cc
  strings/string_util.h
  strings/string_util_constants.cc
  strings/string_util_internal.h
  strings/stringize_macros.h
  strings/stringprintf.cc
  strings/stringprintf.h
  strings/sys_string_conversions.h
  strings/utf_offset_string_conversions.cc
  strings/utf_offset_string_conversions.h
  strings/utf_string_conversion_utils.cc
  strings/utf_string_conversion_utils.h
  strings/utf_string_conversions.cc
  strings/utf_string_conversions.h
  supports_user_data.cc
  supports_user_data.h
  sync_socket.cc
  sync_socket.h
  synchronization/atomic_flag.cc
  synchronization/atomic_flag.h
  synchronization/condition_variable.h
  synchronization/lock.cc
  synchronization/lock.h
  synchronization/lock_impl.h
  synchronization/waitable_event.h
  synchronization/waitable_event_watcher.h
  sys_byteorder.h
  syslog_logging.cc
  syslog_logging.h
  system/sys_info.cc
  system/sys_info.h
  system/sys_info_internal.h
  system/system_monitor.cc
  system/system_monitor.h
  task/bind_post_task.h
  task/bind_post_task_internal.h
  task/cancelable_task_tracker.cc
  task/cancelable_task_tracker.h
  task/common/checked_lock.h
  task/common/checked_lock_impl.cc
  task/common/checked_lock_impl.h
  task/common/operations_controller.cc
  task/common/operations_controller.h
  task/common/scoped_defer_task_posting.cc
  task/common/scoped_defer_task_posting.h
  task/common/task_annotator.cc
  task/common/task_annotator.h
  task/current_thread.cc
  task/current_thread.h
  task/default_delayed_task_handle_delegate.cc
  task/default_delayed_task_handle_delegate.h
  task/deferred_sequenced_task_runner.cc
  task/deferred_sequenced_task_runner.h
  task/delay_policy.h
  task/delayed_task_handle.cc
  task/delayed_task_handle.h
  task/lazy_thread_pool_task_runner.cc
  task/lazy_thread_pool_task_runner.h
  task/post_job.cc
  task/post_job.h
  task/post_task.cc
  task/post_task.h
  task/post_task_and_reply_with_result_internal.h
  task/scoped_set_task_priority_for_current_thread.cc
  task/scoped_set_task_priority_for_current_thread.h
  task/sequence_manager/associated_thread_id.cc
  task/sequence_manager/associated_thread_id.h
  task/sequence_manager/atomic_flag_set.cc
  task/sequence_manager/atomic_flag_set.h
  task/sequence_manager/delayed_task_handle_delegate.cc
  task/sequence_manager/delayed_task_handle_delegate.h
  task/sequence_manager/enqueue_order.h
  task/sequence_manager/enqueue_order_generator.cc
  task/sequence_manager/enqueue_order_generator.h
  task/sequence_manager/fence.cc
  task/sequence_manager/fence.h
  task/sequence_manager/lazily_deallocated_deque.h
  task/sequence_manager/lazy_now.cc
  task/sequence_manager/lazy_now.h
  task/sequence_manager/sequence_manager.cc
  task/sequence_manager/sequence_manager.h
  task/sequence_manager/sequence_manager_impl.cc
  task/sequence_manager/sequence_manager_impl.h
  task/sequence_manager/sequenced_task_source.cc
  task/sequence_manager/sequenced_task_source.h
  task/sequence_manager/task_order.cc
  task/sequence_manager/task_order.h
  task/sequence_manager/task_queue.cc
  task/sequence_manager/task_queue.h
  task/sequence_manager/task_queue_impl.cc
  task/sequence_manager/task_queue_impl.h
  task/sequence_manager/task_queue_selector.cc
  task/sequence_manager/task_queue_selector.h
  task/sequence_manager/task_queue_selector_logic.h
  task/sequence_manager/task_time_observer.h
  task/sequence_manager/tasks.cc
  task/sequence_manager/tasks.h
  task/sequence_manager/thread_controller.cc
  task/sequence_manager/thread_controller.h
  task/sequence_manager/thread_controller_impl.cc
  task/sequence_manager/thread_controller_impl.h
  task/sequence_manager/thread_controller_power_monitor.cc
  task/sequence_manager/thread_controller_power_monitor.h
  task/sequence_manager/thread_controller_with_message_pump_impl.cc
  task/sequence_manager/thread_controller_with_message_pump_impl.h
  task/sequence_manager/time_domain.cc
  task/sequence_manager/time_domain.h
  task/sequence_manager/wake_up_queue.cc
  task/sequence_manager/wake_up_queue.h
  task/sequence_manager/work_deduplicator.cc
  task/sequence_manager/work_deduplicator.h
  task/sequence_manager/work_queue.cc
  task/sequence_manager/work_queue.h
  task/sequence_manager/work_queue_sets.cc
  task/sequence_manager/work_queue_sets.h
  task/sequenced_task_runner.cc
  task/sequenced_task_runner.h
  task/sequenced_task_runner_helpers.h
  task/simple_task_executor.cc
  task/simple_task_executor.h
  task/single_thread_task_executor.cc
  task/single_thread_task_executor.h
  task/single_thread_task_runner.h
  task/single_thread_task_runner_thread_mode.h
  task/task_executor.cc
  task/task_executor.h
  task/task_features.cc
  task/task_features.h
  task/task_observer.h
  task/task_runner.cc
  task/task_runner.h
  task/task_runner_util.h
  task/task_traits.cc
  task/task_traits.h
  task/task_traits_extension.h
  task/thread_pool.cc
  task/thread_pool.h
  task/thread_pool/delayed_task_manager.cc
  task/thread_pool/delayed_task_manager.h
  task/thread_pool/environment_config.cc
  task/thread_pool/environment_config.h
  task/thread_pool/initialization_util.cc
  task/thread_pool/initialization_util.h
  task/thread_pool/job_task_source.cc
  task/thread_pool/job_task_source.h
  task/thread_pool/pooled_parallel_task_runner.cc
  task/thread_pool/pooled_parallel_task_runner.h
  task/thread_pool/pooled_sequenced_task_runner.cc
  task/thread_pool/pooled_sequenced_task_runner.h
  task/thread_pool/pooled_single_thread_task_runner_manager.cc
  task/thread_pool/pooled_single_thread_task_runner_manager.h
  task/thread_pool/pooled_task_runner_delegate.cc
  task/thread_pool/pooled_task_runner_delegate.h
  task/thread_pool/priority_queue.cc
  task/thread_pool/priority_queue.h
  task/thread_pool/sequence.cc
  task/thread_pool/sequence.h
  task/thread_pool/service_thread.cc
  task/thread_pool/service_thread.h
  task/thread_pool/task.cc
  task/thread_pool/task.h
  task/thread_pool/task_source.cc
  task/thread_pool/task_source.h
  task/thread_pool/task_source_sort_key.cc
  task/thread_pool/task_source_sort_key.h
  task/thread_pool/task_tracker.cc
  task/thread_pool/task_tracker.h
  task/thread_pool/thread_group.cc
  task/thread_pool/thread_group.h
  task/thread_pool/thread_group_impl.cc
  task/thread_pool/thread_group_impl.h
  task/thread_pool/thread_group_native.cc
  task/thread_pool/thread_group_native.h
  task/thread_pool/thread_pool_impl.cc
  task/thread_pool/thread_pool_impl.h
  task/thread_pool/thread_pool_instance.cc
  task/thread_pool/thread_pool_instance.h
  task/thread_pool/tracked_ref.h
  task/thread_pool/worker_thread.cc
  task/thread_pool/worker_thread.h
  task/thread_pool/worker_thread_observer.h
  task/thread_pool/worker_thread_stack.cc
  task/thread_pool/worker_thread_stack.h
  task/updateable_sequenced_task_runner.h
  template_util.h
  test/malloc_wrapper.h
  test/scoped_logging_settings.h
  test/spin_wait.h
  third_party/cityhash/city.cc
  third_party/cityhash/city.h
  third_party/cityhash_v103/src/city_v103.cc
  third_party/cityhash_v103/src/city_v103.h
  third_party/icu/icu_utf.h
  third_party/nspr/prtime.cc
  third_party/nspr/prtime.h
  third_party/superfasthash/superfasthash.c
  thread_annotations.h
  threading/hang_watcher.cc
  threading/hang_watcher.h
  threading/platform_thread.cc
  threading/platform_thread.h
  threading/platform_thread_ref.cc
  threading/platform_thread_ref.h
  threading/post_task_and_reply_impl.cc
  threading/post_task_and_reply_impl.h
  threading/scoped_blocking_call.cc
  threading/scoped_blocking_call.h
  threading/scoped_blocking_call_internal.cc
  threading/scoped_blocking_call_internal.h
  threading/scoped_thread_priority.cc
  threading/scoped_thread_priority.h
  threading/sequence_bound.h
  threading/sequence_local_storage_map.cc
  threading/sequence_local_storage_map.h
  threading/sequence_local_storage_slot.cc
  threading/sequence_local_storage_slot.h
  threading/sequenced_task_runner_handle.cc
  threading/sequenced_task_runner_handle.h
  threading/simple_thread.cc
  threading/simple_thread.h
  threading/thread.cc
  threading/thread.h
  threading/thread_checker.cc
  threading/thread_checker.h
  threading/thread_checker_impl.cc
  threading/thread_checker_impl.h
  threading/thread_collision_warner.cc
  threading/thread_collision_warner.h
  threading/thread_id_name_manager.cc
  threading/thread_id_name_manager.h
  threading/thread_local.h
  threading/thread_local_internal.h
  threading/thread_local_storage.cc
  threading/thread_local_storage.h
  threading/thread_restrictions.cc
  threading/thread_restrictions.h
  threading/thread_task_runner_handle.cc
  threading/thread_task_runner_handle.h
  threading/threading_features.h
  threading/watchdog.cc
  threading/watchdog.h
  time/clock.cc
  time/clock.h
  time/default_clock.cc
  time/default_clock.h
  time/default_tick_clock.cc
  time/default_tick_clock.h
  time/tick_clock.cc
  time/tick_clock.h
  time/time.cc
  time/time.h
  time/time_delta_from_string.cc
  time/time_delta_from_string.h
  time/time_override.cc
  time/time_override.h
  time/time_to_iso8601.cc
  time/time_to_iso8601.h
  timer/elapsed_timer.cc
  timer/elapsed_timer.h
  timer/hi_res_timer_manager.h
  timer/lap_timer.cc
  timer/lap_timer.h
  timer/timer.cc
  timer/timer.h
  timer/wall_clock_timer.cc
  timer/wall_clock_timer.h
  token.cc
  token.h
  trace_event/base_tracing.h
  trace_event/base_tracing_forward.h
  trace_event/common/trace_event_common.h
  trace_event/heap_profiler_allocation_context.cc
  trace_event/heap_profiler_allocation_context.h
  trace_event/heap_profiler_allocation_context_tracker.cc
  trace_event/heap_profiler_allocation_context_tracker.h
  trace_event/memory_allocator_dump_guid.cc
  trace_event/memory_allocator_dump_guid.h
  trace_event/trace_id_helper.cc
  trace_event/trace_id_helper.h
  traits_bag.h
  tuple.h
  types/id_type.h
  types/pass_key.h
  types/strong_alias.h
  types/token_type.h
  unguessable_token.cc
  unguessable_token.h
  value_iterators.cc
  value_iterators.h
  values.cc
  values.h
  version.cc
  version.h
  vlog.cc
  vlog.h)

if(UNIX)
  list(APPEND SOURCES
    debug/debugger_posix.cc
    debug/stack_trace_posix.cc
    file_descriptor_posix.cc
    file_descriptor_posix.h
    files/dir_reader_posix.h
    files/file_descriptor_watcher_posix.cc
    files/file_descriptor_watcher_posix.h
    files/file_enumerator_posix.cc
    files/file_posix.cc
    files/file_util_posix.cc
    files/memory_mapped_file_posix.cc
    memory/madv_free_discardable_memory_allocator_posix.cc
    memory/madv_free_discardable_memory_allocator_posix.h
    memory/madv_free_discardable_memory_posix.cc
    memory/madv_free_discardable_memory_posix.h
    memory/page_size_posix.cc
    message_loop/watchable_io_message_pump_posix.cc
    message_loop/watchable_io_message_pump_posix.h
    native_library_posix.cc
    posix/eintr_wrapper.h
    posix/file_descriptor_shuffle.cc
    posix/file_descriptor_shuffle.h
    posix/global_descriptors.cc
    posix/global_descriptors.h
    posix/safe_strerror.cc
    posix/safe_strerror.h
    posix/unix_domain_socket.cc
    posix/unix_domain_socket.h
    process/kill_posix.cc
    process/launch_posix.cc
    process/process_handle_posix.cc
    process/process_metrics_posix.cc
    process/process_posix.cc
    profiler/module_cache_posix.cc
    rand_util_posix.cc
    strings/string_util_posix.h
    strings/sys_string_conversions_posix.cc
    sync_socket_posix.cc
    synchronization/condition_variable_posix.cc
    synchronization/lock_impl_posix.cc
    synchronization/waitable_event_posix.cc
    synchronization/waitable_event_watcher_posix.cc
    system/sys_info_posix.cc
    task/thread_pool/task_tracker_posix.cc
    task/thread_pool/task_tracker_posix.h
    threading/platform_thread_internal_posix.cc
    threading/platform_thread_internal_posix.h
    threading/platform_thread_posix.cc
    threading/thread_local_storage_posix.cc
    timer/hi_res_timer_manager_posix.cc)

  if(NOT APPLE)
    list(APPEND SOURCES
      cpu_affinity_posix.cc
      cpu_affinity_posix.h
      profiler/stack_copier_signal.cc
      profiler/stack_copier_signal.h
      profiler/stack_sampler_posix.cc
      profiler/thread_delegate_posix.cc
      profiler/thread_delegate_posix.h)
  endif()
endif()

if(WIN32)
  list(APPEND SOURCES
    debug/close_handle_hook_win.cc
    debug/close_handle_hook_win.h
    debug/debugger_win.cc
    debug/gdi_debug_util_win.cc
    debug/gdi_debug_util_win.h
    debug/invalid_access_win.cc
    debug/invalid_access_win.h
    debug/stack_trace_win.cc
    enterprise_util_win.cc
    file_version_info_win.cc
    file_version_info_win.h
    files/file_path_watcher_win.cc
    files/file_util_win.cc
    files/file_win.cc
    files/memory_mapped_file_win.cc
    logging_win.cc
    logging_win.h
    memory/page_size_win.cc
    message_loop/message_pump_win.cc
    message_loop/message_pump_win.h
    native_library_win.cc
    process/kill_win.cc
    process/launch_win.cc
    process/memory_win.cc
    process/process_handle_win.cc
    process/process_info_win.cc
    process/process_iterator_win.cc
    process/process_metrics_win.cc
    process/process_win.cc
    profiler/module_cache_win.cc
    profiler/native_unwinder_win.cc
    profiler/native_unwinder_win.h
    profiler/stack_sampler_win.cc
    profiler/suspendable_thread_delegate_win.cc
    profiler/suspendable_thread_delegate_win.h
    scoped_clear_last_error_win.cc
    strings/strcat_win.cc
    strings/strcat_win.h
    strings/string_number_conversions_win.cc
    strings/string_number_conversions_win.h
    strings/string_split_win.cc
    strings/string_split_win.h
    strings/string_util_win.cc
    strings/string_util_win.h
    strings/sys_string_conversions_win.cc
    sync_socket_win.cc
    synchronization/condition_variable_win.cc
    synchronization/lock_impl_win.cc
    synchronization/waitable_event_watcher_win.cc
    synchronization/waitable_event_win.cc
    task/thread_pool/thread_group_native_win.cc
    task/thread_pool/thread_group_native_win.h
    threading/platform_thread_win.cc
    threading/platform_thread_win.h
    threading/thread_local_storage_win.cc
    timer/hi_res_timer_manager_win.cc
    win/access_token.cc
    win/access_token.h
    win/async_operation.h
    win/atl.h
    win/atl_throw.cc
    win/atl_throw.h
    win/com_init_balancer.cc
    win/com_init_balancer.h
    win/com_init_check_hook.cc
    win/com_init_check_hook.h
    win/com_init_util.cc
    win/com_init_util.h
    win/core_winrt_util.cc
    win/core_winrt_util.h
    win/current_module.h
    win/embedded_i18n/language_selector.cc
    win/embedded_i18n/language_selector.h
    win/enum_variant.cc
    win/enum_variant.h
    win/event_trace_consumer.h
    win/event_trace_controller.cc
    win/event_trace_controller.h
    win/event_trace_provider.cc
    win/event_trace_provider.h
    win/hstring_compare.cc
    win/hstring_compare.h
    win/hstring_reference.cc
    win/hstring_reference.h
    win/i18n.cc
    win/i18n.h
    win/iat_patch_function.cc
    win/iat_patch_function.h
    win/map.h
    win/message_window.cc
    win/message_window.h
    win/nt_status.cc
    win/nt_status.h
    win/object_watcher.cc
    win/object_watcher.h
    win/patch_util.cc
    win/patch_util.h
    win/pe_image_reader.cc
    win/pe_image_reader.h
    win/post_async_results.h
    win/process_startup_helper.cc
    win/process_startup_helper.h
    win/propvarutil.h
    win/reference.h
    win/registry.cc
    win/registry.h
    win/resource_util.cc
    win/resource_util.h
    win/scoped_bstr.cc
    win/scoped_bstr.h
    win/scoped_co_mem.h
    win/scoped_com_initializer.cc
    win/scoped_com_initializer.h
    win/scoped_gdi_object.h
    win/scoped_handle.cc
    win/scoped_handle.h
    win/scoped_handle_verifier.cc
    win/scoped_handle_verifier.h
    win/scoped_hdc.h
    win/scoped_hglobal.h
    win/scoped_hstring.cc
    win/scoped_hstring.h
    win/scoped_localalloc.h
    win/scoped_process_information.cc
    win/scoped_process_information.h
    win/scoped_propvariant.h
    win/scoped_safearray.h
    win/scoped_select_object.h
    win/scoped_variant.cc
    win/scoped_variant.h
    win/scoped_windows_thread_environment.h
    win/scoped_winrt_initializer.cc
    win/scoped_winrt_initializer.h
    win/security_util.cc
    win/security_util.h
    win/shlwapi.h
    win/shortcut.cc
    win/shortcut.h
    win/sid.cc
    win/sid.h
    win/sphelper.h
    win/startup_information.cc
    win/startup_information.h
    win/variant_util.h
    win/variant_vector.cc
    win/variant_vector.h
    win/vector.cc
    win/vector.h
    win/win_handle_types_list.inc
    win/win_util.cc
    win/win_util.h
    win/wincrypt_shim.h
    win/windows_defines.inc
    win/windows_h_disallowed.h
    win/windows_types.h
    win/windows_undefines.inc
    win/windows_version.cc
    win/windows_version.h
    win/windowsx_shim.h
    win/winrt_foundation_helpers.h
    win/winrt_storage_util.cc
    win/winrt_storage_util.h
    win/wmi.cc
    win/wmi.h
    win/wrapped_window_proc.cc
    win/wrapped_window_proc.h)
endif()

if(MAC)
  list(APPEND SOURCES
    allocator/allocator_interception_mac.h
    allocator/allocator_interception_mac.mm
    allocator/malloc_zone_functions_mac.cc
    allocator/malloc_zone_functions_mac.h
    enterprise_util_mac.mm
    file_version_info_mac.h
    file_version_info_mac.mm
    files/file_path_watcher_mac.cc
    files/file_util_mac.mm
    mac/authorization_util.h
    mac/authorization_util.mm
    mac/backup_util.h
    mac/backup_util.mm
    mac/bundle_locations.h
    mac/bundle_locations.mm
    mac/call_with_eh_frame.cc
    mac/call_with_eh_frame.h
    mac/call_with_eh_frame_asm.S
    mac/close_nocancel.cc
    mac/dispatch_source_mach.cc
    mac/dispatch_source_mach.h
    mac/foundation_util.h
    mac/foundation_util.mm
    mac/launch_services_util.h
    mac/launch_services_util.mm
    mac/launchd.cc
    mac/launchd.h
    mac/mac_logging.h
    mac/mac_logging.mm
    mac/mac_util.h
    mac/mac_util.mm
    mac/mach_logging.cc
    mac/mach_logging.h
    mac/mach_port_rendezvous.cc
    mac/mach_port_rendezvous.h
    mac/objc_release_properties.h
    mac/objc_release_properties.mm
    mac/os_crash_dumps.cc
    mac/os_crash_dumps.h
    mac/scoped_aedesc.h
    mac/scoped_authorizationref.h
    mac/scoped_authorizationref.mm
    mac/scoped_block.h
    mac/scoped_cffiledescriptorref.h
    mac/scoped_cftyperef.h
    mac/scoped_dispatch_object.h
    mac/scoped_ionotificationportref.h
    mac/scoped_ioobject.h
    mac/scoped_ioplugininterface.h
    mac/scoped_launch_data.h
    mac/scoped_mach_msg_destroy.h
    mac/scoped_mach_port.cc
    mac/scoped_mach_port.h
    mac/scoped_mach_vm.cc
    mac/scoped_mach_vm.h
    mac/scoped_nsautorelease_pool.h
    mac/scoped_nsautorelease_pool.mm
    mac/scoped_nsobject.h
    mac/scoped_objc_class_swizzler.h
    mac/scoped_objc_class_swizzler.mm
    mac/scoped_sending_event.h
    mac/scoped_sending_event.mm
    mac/sdk_forward_declarations.h
    mac/sdk_forward_declarations.mm
    message_loop/message_pump_mac.h
    message_loop/message_pump_mac.mm
    native_library_mac.mm
    process/kill_mac.cc
    process/launch_mac.cc
    process/memory_mac.mm
    process/port_provider_mac.cc
    process/port_provider_mac.h
    process/process_handle_mac.cc
    process/process_info_mac.cc
    process/process_iterator_mac.cc
    process/process_mac.cc
    process/process_metrics_mac.cc
    profiler/module_cache_mac.cc
    profiler/native_unwinder_mac.cc
    profiler/native_unwinder_mac.h
    profiler/stack_sampler_mac.cc
    profiler/suspendable_thread_delegate_mac.cc
    profiler/suspendable_thread_delegate_mac.h
    strings/sys_string_conversions_mac.mm
    synchronization/waitable_event_mac.cc
    synchronization/waitable_event_watcher_mac.cc
    task/thread_pool/thread_group_native_mac.h
    task/thread_pool/thread_group_native_mac.mm
    threading/platform_thread_mac.mm)
endif()

if(IOS)
  list(APPEND SOURCES
    critical_closure_internal_ios.mm
    ios/block_types.h
    ios/device_util.h
    ios/device_util.mm
    ios/ios_util.h
    ios/ios_util.mm
    ios/ns_error_util.h
    ios/ns_error_util.mm
    ios/ns_range.h
    ios/scoped_critical_action.h
    ios/scoped_critical_action.mm
    native_library_ios.mm
    process/launch_ios.cc
    process/process_metrics_ios.cc
    profiler/module_cache_mac.cc
    profiler/stack_sampler_ios.cc)
  if(CRONET_BUILD)
    list(APPEND SOURCES
      message_loop/message_pump_io_ios.cc
      message_loop/message_pump_io_ios.h)
  else()
    list(APPEND SOURCES
      message_loop/message_pump_kqueue.cc
      message_loop/message_pump_kqueue.h)
  endif()

  if(IOS_STACK_PROFILER_ENABLED)
    list(APPEND SOURCES
      profiler/native_unwinder_apple.cc
      profiler/native_unwinder_apple.h
      profiler/suspendable_thread_delegate_mac.cc
      profiler/suspendable_thread_delegate_mac.h)
  endif()
endif()

if(ANDROID)
  list(APPEND SOURCES
    debug/stack_trace_android.cc
    files/file_util_android.cc
    files/scoped_file_android.cc
    message_loop/message_pump_android.cc
    message_loop/message_pump_android.h
    os_compat_android.cc
    os_compat_android.h
    process/process_android.cc
    profiler/stack_sampler_android.cc
    threading/platform_thread_android.cc)
endif()

if(LINUX OR CHROMEOS)
  list(APPEND SOURCES
    debug/proc_maps_linux.cc
    debug/proc_maps_linux.h
    files/dir_reader_linux.h
    files/file_path_watcher_linux.cc
    files/file_path_watcher_linux.h
    files/file_util_linux.cc
    files/scoped_file_linux.cc
    process/internal_linux.cc
    process/internal_linux.h
    process/memory_linux.cc
    process/process_handle_linux.cc
    process/process_iterator_linux.cc
    process/process_linux.cc
    process/process_metrics_linux.cc
    threading/platform_thread_linux.cc)
endif()

if(TRUE) # if (!is_nacl)
  list(APPEND SOURCES
    base_paths.cc
    base_paths.h
    metrics/persistent_histogram_storage.cc
    metrics/persistent_histogram_storage.h)

  if(WIN32)
    list(APPEND SOURCES
      base_paths_win.cc
      base_paths_win.h)
  endif()

  if(MAC)
    list(APPEND SOURCES
      base_paths_mac.h
      base_paths_mac.mm)
  endif()

  if(ANDROID)
    list(APPEND SOURCES
      base_paths_android.cc
      base_paths_android.h)
  endif()

  if(UNIX)
    list(APPEND SOURCES base_paths_posix.h)
  endif()

  if(LINUX OR CHROMEOS)
    list(APPEND SOURCES
      base_paths_posix.cc
      debug/elf_reader.cc
      debug/elf_reader.h
      stack_canary_linux.cc
      stack_canary_linux.h)
  endif()
endif()

set(DEFINES "")
set(LIBS "")
set(FRAMEWORKS "")

set(DEPS
  allocator_buildflags
  chromecast_buildflags
  chromeos_buildflags)
set(LIBS basium_double_conversion basium_dynamic_annotations modp_b64)
set(PUBLIC_LIBS basium_base_static basium_base_numerics absl::base absl::optional)
set(PUBLIC_DEPS
  cfi_buildflags
  clang_profiling_buildflags
  debugging_buildflags
  feature_list_buildflags
  ios_cronet_buildflags
  logging_buildflags
  sanitizer_buildflags
  tracing_buildflags)

if(USE_ALLOCATOR_SHIM)
  list(APPEND SOURCES
    allocator/allocator_shim.cc
    allocator/allocator_shim.h
    allocator/allocator_shim_default_dispatch_to_partition_alloc.cc
    allocator/allocator_shim_default_dispatch_to_partition_alloc.h
    allocator/allocator_shim_internals.h)
  if(ANDROID)
    list(APPEND SOURCES
      allocator/allocator_shim_override_cpp_symbols.h
      allocator/allocator_shim_override_linker_wrapped_symbols.h)
  endif()
  if(APPLE)
    list(APPEND SOURCES
      allocator/allocator_shim_override_mac_default_zone.h
      allocator/allocator_shim_override_mac_symbols.h
      allocator/early_zone_registration_mac.h)
  endif()
  if(CHROMEOS OR LINUX)
    list(APPEND SOURCES
      allocator/allocator_shim_override_cpp_symbols.h
      allocator/allocator_shim_override_glibc_weak_symbols.h
      allocator/allocator_shim_override_libc_symbols.h)
  endif()
  if(WIN32)
    list(APPEND SOURCES
      allocator/allocator_shim_override_ucrt_symbols_win.h
      allocator/winheap_stubs_win.cc
      allocator/winheap_stubs_win.h)
  endif()

  if(USE_ALLOCATOR STREQUAL "none")
    if(ANDROID)
      list(APPEND SOURCES allocator/allocator_shim_default_dispatch_to_linker_wrapped_symbols.cc)
    endif()
    if(APPLE)
      list(APPEND SOURCES
        allocator/allocator_shim_default_dispatch_to_mac_zoned_malloc.cc)
    endif()
    if(CHROMEOS OR LINUX)
      list(APPEND SOURCES allocator/allocator_shim_default_dispatch_to_glibc.cc)
    endif()
    if(WIN32)
      list(APPEND SOURCES allocator/allocator_shim_default_dispatch_to_winheap.cc)
    endif()
  endif()
endif()

if(APPLE OR CHROMEOS OR CHROMECAST OR FUCHSIA)
  set(DEFINES SYSTEM_NATIVE_UTF8)
endif()

if(ANDROID)
  list(REMOVE_ITEM SOURCES
    debug/stack_trace_posix.cc
    profiler/stack_sampler_posix.cc)
  list(APPEND SOURCES
    android/android_hardware_buffer_compat.cc
    android/android_hardware_buffer_compat.h
    android/android_image_reader_abi.h
    android/android_image_reader_compat.cc
    android/android_image_reader_compat.h
    android/apk_assets.cc
    android/apk_assets.h
    android/application_status_listener.cc
    android/application_status_listener.h
    android/base_jni_onload.cc
    android/base_jni_onload.h
    android/build_info.cc
    android/build_info.h
    android/bundle_utils.cc
    android/bundle_utils.h
    android/callback_android.cc
    android/callback_android.h
    android/child_process_binding_types.h
    android/child_process_service.cc
    android/command_line_android.cc
    android/content_uri_utils.cc
    android/content_uri_utils.h
    android/cpu_features.cc
    android/early_trace_event_binding.cc
    android/early_trace_event_binding.h
    android/event_log.cc
    android/event_log.h
    android/feature_list_jni.cc
    android/features_jni.cc
    android/field_trial_list.cc
    android/important_file_writer_android.cc
    android/int_string_callback.cc
    android/int_string_callback.h
    android/jank_metric_uma_recorder.cc
    android/jank_metric_uma_recorder.h
    android/java_exception_reporter.cc
    android/java_exception_reporter.h
    android/java_handler_thread.cc
    android/java_handler_thread.h
    android/java_heap_dump_generator.cc
    android/java_heap_dump_generator.h
    android/java_runtime.cc
    android/java_runtime.h
    android/jni_android.cc
    android/jni_android.h
    android/jni_array.cc
    android/jni_array.h
    android/jni_generator/jni_generator_helper.h
    android/jni_int_wrapper.h
    android/jni_registrar.cc
    android/jni_registrar.h
    android/jni_string.cc
    android/jni_string.h
    android/jni_utils.cc
    android/jni_utils.h
    android/jni_weak_ref.cc
    android/jni_weak_ref.h
    android/library_loader/anchor_functions.cc
    android/library_loader/anchor_functions.h
    android/library_loader/library_loader_hooks.cc
    android/library_loader/library_loader_hooks.h
    android/library_loader/library_prefetcher.cc
    android/library_loader/library_prefetcher.h
    android/library_loader/library_prefetcher_hooks.cc
    android/locale_utils.cc
    android/locale_utils.h
    android/memory_pressure_listener_android.cc
    android/memory_pressure_listener_android.h
    android/native_uma_recorder.cc
    android/path_service_android.cc
    android/path_utils.cc
    android/path_utils.h
    android/radio_utils.cc
    android/radio_utils.h
    android/reached_addresses_bitset.cc
    android/reached_addresses_bitset.h
    android/reached_code_profiler.cc
    android/reached_code_profiler.h
    android/record_histogram.cc
    android/record_user_action.cc
    android/scoped_hardware_buffer_fence_sync.cc
    android/scoped_hardware_buffer_fence_sync.h
    android/scoped_hardware_buffer_handle.cc
    android/scoped_hardware_buffer_handle.h
    android/scoped_java_ref.cc
    android/scoped_java_ref.h
    android/statistics_recorder_android.cc
    android/sys_utils.cc
    android/sys_utils.h
    android/task_scheduler/post_task_android.cc
    android/task_scheduler/post_task_android.h
    android/task_scheduler/task_runner_android.cc
    android/task_scheduler/task_runner_android.h
    android/time_utils.cc
    android/timezone_utils.cc
    android/timezone_utils.h
    android/trace_event_binding.cc
    android/trace_event_binding.h
    android/unguessable_token_android.cc
    android/unguessable_token_android.h
    memory/platform_shared_memory_region_android.cc
    system/sys_info_android.cc

    # Android uses some Linux sources.
    debug/elf_reader.cc
    debug/elf_reader.h
    debug/proc_maps_linux.cc
    debug/proc_maps_linux.h
    files/file_path_watcher_linux.cc
    files/file_path_watcher_linux.h
    power_monitor/power_monitor_device_source_android.cc
    process/internal_linux.cc
    process/internal_linux.h
    process/memory_linux.cc
    process/process_handle_linux.cc
    process/process_iterator_linux.cc
    process/process_metrics_linux.cc
    system/sys_info_linux.cc)
endif()

if(CHROMEOS)
  list(APPEND SOURCES
    logging_chromeos.cc
    system/sys_info_chromeos.cc)
  if(CHROMEOS_ASH)
    list(APPEND SOURCES power_monitor/power_monitor_device_sourcde_chromeos.cc)
  endif()
endif()

if(FALSE) # if (is_fuchsia)
  list(APPEND SOURCES
    base_paths_fuchsia.cc
    debug/debugger_fuchsia.cc
    debug/elf_reader.cc
    debug/elf_reader.h
    debug/stack_trace_fuchsia.cc
    file_descriptor_posix.cc
    file_descriptor_posix.h
    files/dir_reader_posix.h
    files/file_descriptor_watcher_posix.cc
    files/file_descriptor_watcher_posix.h
    files/file_enumerator_posix.cc
    files/file_path_watcher_fuchsia.cc
    files/file_posix.cc
    files/file_util_fuchsia.cc
    files/file_util_posix.cc
    files/memory_mapped_file_posix.cc
    fuchsia/build_info.cc
    fuchsia/build_info.h
    fuchsia/default_job.cc
    fuchsia/default_job.h
    fuchsia/file_utils.cc
    fuchsia/file_utils.h
    fuchsia/filtered_service_directory.cc
    fuchsia/filtered_service_directory.h
    fuchsia/fuchsia_logging.cc
    fuchsia/fuchsia_logging.h
    fuchsia/intl_profile_watcher.cc
    fuchsia/intl_profile_watcher.h
    fuchsia/koid.cc
    fuchsia/koid.h
    fuchsia/mem_buffer_util.cc
    fuchsia/mem_buffer_util.h
    fuchsia/process_context.cc
    fuchsia/process_context.h
    fuchsia/process_lifecycle.cc
    fuchsia/process_lifecycle.h
    fuchsia/scheduler.h
    fuchsia/scoped_fx_logger.cc
    fuchsia/scoped_fx_logger.h
    fuchsia/scoped_service_binding.h
    fuchsia/scoped_service_publisher.h
    fuchsia/service_provider_impl.cc
    fuchsia/service_provider_impl.h
    fuchsia/startup_context.cc
    fuchsia/startup_context.h
    memory/page_size_posix.cc
    memory/platform_shared_memory_region_fuchsia.cc
    message_loop/message_pump_fuchsia.cc
    message_loop/message_pump_fuchsia.h
    message_loop/watchable_io_message_pump_posix.cc
    message_loop/watchable_io_message_pump_posix.h
    native_library_fuchsia.cc
    posix/eintr_wrapper.h
    posix/file_descriptor_shuffle.cc
    posix/file_descriptor_shuffle.h
    posix/global_descriptors.cc
    posix/global_descriptors.h
    posix/safe_strerror.cc
    posix/safe_strerror.h
    process/kill_fuchsia.cc
    process/launch_fuchsia.cc
    process/memory_fuchsia.cc
    process/process_fuchsia.cc
    process/process_handle_fuchsia.cc
    process/process_iterator_fuchsia.cc
    process/process_metrics_fuchsia.cc
    process/process_metrics_posix.cc
    profiler/module_cache_posix.cc
    profiler/stack_sampler_posix.cc
    rand_util_fuchsia.cc
    strings/string_util_posix.h
    strings/sys_string_conversions_posix.cc
    sync_socket_posix.cc
    synchronization/condition_variable_posix.cc
    synchronization/lock_impl_posix.cc
    synchronization/waitable_event_posix.cc
    synchronization/waitable_event_watcher_posix.cc
    system/sys_info_fuchsia.cc
    task/thread_pool/task_tracker_posix.cc
    task/thread_pool/task_tracker_posix.h
    threading/platform_thread_fuchsia.cc
    threading/platform_thread_posix.cc
    threading/thread_local_storage_posix.cc
    time/time_conversion_posix.cc
    time/time_exploded_icu.cc
    time/time_fuchsia.cc
    timer/hi_res_timer_manager_posix.cc)
endif()

list(APPEND SOURCES
  hash/md5.h
  hash/md5_constexpr.h
  hash/md5_constexpr_internal.h
  hash/sha1.h)
if(FALSE) # if (is_nacl)
  list(APPEND SOURCES
    hash/md5_nacl.cc
    hash/md5_nacl.h
    hash/sha1_nacl.cc
    hash/sha1_nacl.h)
else()
  list(APPEND SOURCES
    hash/md5_boringssl.cc
    hash/md5_boringssl.h
    hash/sha1_boringssl.cc
    hash/sha1_boringssl.h)
  list(APPEND PUBLIC_LIBS ssl crypto)
endif()

if(FALSE) # if (is_nacl)
  list(APPEND SOURCES
    debug/stack_trace_nacl.cc
    files/file_path_watcher_stub.cc
    memory/page_size_nacl.cc
    process/process_stubs.cc
    sync_socket_nacl.cc
    threading/platform_thread_linux.cc)

  list(REMOVE_ITEM SOURCES
    cpu.cc
    debug/crash_logging.cc
    debug/crash_logging.h
    debug/stack_trace.cc
    debug/stack_trace_posix.cc
    files/file_descriptor_watcher_posix.cc
    files/file_descriptor_watcher_posix.h
    files/file_enumerator.cc
    files/file_enumerator_posix.cc
    files/file_proxy.cc
    files/file_util.cc
    files/file_util.h
    files/file_util_posix.cc
    files/important_file_writer.cc
    files/important_file_writer.h
    files/important_file_writer_cleaner.cc
    files/important_file_writer_cleaner.h
    files/scoped_temp_dir.cc
    json/json_file_value_serializer.cc
    json/json_file_value_serializer.h
    memory/discardable_memory.cc
    memory/discardable_memory.h
    memory/discardable_memory_allocator.cc
    memory/discardable_memory_allocator.h
    memory/discardable_shared_memory.cc
    memory/discardable_shared_memory.h
    memory/madv_free_discardable_memory_allocator_posix.cc
    memory/madv_free_discardable_memory_allocator_posix.h
    memory/madv_free_discardable_memory_posix.cc
    memory/madv_free_discardable_memory_posix.h
    memory/page_size_posix.cc
    native_library.cc
    native_library_posix.cc
    path_service.cc
    posix/unix_domain_socket.cc
    process/kill.cc
    process/kill.h
    process/kill_posix.cc
    process/launch.cc
    process/launch.h
    process/launch_posix.cc
    process/memory.cc
    process/memory.h
    process/process_iterator.cc
    process/process_iterator.h
    process/process_metrics.cc
    process/process_metrics_posix.cc
    process/process_posix.cc
    profiler/module_cache_posix.cc
    rand_util_posix.cc
    scoped_native_library.cc
    sync_socket_posix.cc
    system/sys_info.cc
    system/sys_info_posix.cc
    task/thread_pool/initialization_util.cc
    task/thread_pool/initialization_util.h
    task/thread_pool/task_tracker_posix.cc
    task/thread_pool/task_tracker_posix.h)
else()
  list(REMOVE_ITEM SOURCES
    os_compat_nacl.cc
    os_compat_nacl.h
    rand_util_nacl.cc)

  if(USE_PARTITION_ALLOC)
    list(APPEND SOURCES
      # PartitionAlloc uses SpinLock, which doesn't work in NaCl (see below).
      allocator/partition_alloc_features.cc
      allocator/partition_alloc_features.h
      allocator/partition_alloc_support.cc
      allocator/partition_alloc_support.h
      allocator/partition_allocator/address_pool_manager.cc
      allocator/partition_allocator/address_pool_manager.h
      allocator/partition_allocator/address_pool_manager_bitmap.cc
      allocator/partition_allocator/address_pool_manager_bitmap.h
      allocator/partition_allocator/address_pool_manager_types.h
      allocator/partition_allocator/address_space_randomization.cc
      allocator/partition_allocator/address_space_randomization.h
      allocator/partition_allocator/address_space_stats.h
      allocator/partition_allocator/allocation_guard.cc
      allocator/partition_allocator/allocation_guard.h
      allocator/partition_allocator/dangling_raw_ptr_checks.cc
      allocator/partition_allocator/dangling_raw_ptr_checks.h
      allocator/partition_allocator/extended_api.cc
      allocator/partition_allocator/extended_api.h
      allocator/partition_allocator/memory_reclaimer.cc
      allocator/partition_allocator/memory_reclaimer.h
      allocator/partition_allocator/oom.cc
      allocator/partition_allocator/oom.h
      allocator/partition_allocator/oom_callback.cc
      allocator/partition_allocator/oom_callback.h
      allocator/partition_allocator/page_allocator.cc
      allocator/partition_allocator/page_allocator.h
      allocator/partition_allocator/page_allocator_constants.h
      allocator/partition_allocator/page_allocator_internal.h
      allocator/partition_allocator/partition_address_space.cc
      allocator/partition_allocator/partition_address_space.h
      allocator/partition_allocator/partition_alloc-inl.h
      allocator/partition_allocator/partition_alloc.cc
      allocator/partition_allocator/partition_alloc.h
      allocator/partition_allocator/partition_alloc_check.h
      allocator/partition_allocator/partition_alloc_config.h
      allocator/partition_allocator/partition_alloc_constants.h
      allocator/partition_allocator/partition_alloc_forward.h
      allocator/partition_allocator/partition_alloc_hooks.cc
      allocator/partition_allocator/partition_alloc_hooks.h
      allocator/partition_allocator/partition_alloc_notreached.h
      allocator/partition_allocator/partition_bucket.cc
      allocator/partition_allocator/partition_bucket.h
      allocator/partition_allocator/partition_bucket_lookup.h
      allocator/partition_allocator/partition_cookie.h
      allocator/partition_allocator/partition_direct_map_extent.h
      allocator/partition_allocator/partition_freelist_entry.h
      allocator/partition_allocator/partition_lock.h
      allocator/partition_allocator/partition_oom.cc
      allocator/partition_allocator/partition_oom.h
      allocator/partition_allocator/partition_page.cc
      allocator/partition_allocator/partition_page.h
      allocator/partition_allocator/partition_ref_count.h
      allocator/partition_allocator/partition_root.cc
      allocator/partition_allocator/partition_root.h
      allocator/partition_allocator/partition_stats.cc
      allocator/partition_allocator/partition_stats.h
      allocator/partition_allocator/partition_tls.h
      allocator/partition_allocator/random.cc
      allocator/partition_allocator/random.h
      allocator/partition_allocator/reservation_offset_table.cc
      allocator/partition_allocator/reservation_offset_table.h
      allocator/partition_allocator/spinning_mutex.cc
      allocator/partition_allocator/spinning_mutex.h
      allocator/partition_allocator/starscan/logging.h
      allocator/partition_allocator/starscan/metadata_allocator.cc
      allocator/partition_allocator/starscan/metadata_allocator.h
      allocator/partition_allocator/starscan/pcscan.cc
      allocator/partition_allocator/starscan/pcscan.h
      allocator/partition_allocator/starscan/pcscan_internal.cc
      allocator/partition_allocator/starscan/pcscan_internal.h
      allocator/partition_allocator/starscan/pcscan_scheduling.cc
      allocator/partition_allocator/starscan/pcscan_scheduling.h
      allocator/partition_allocator/starscan/raceful_worklist.h
      allocator/partition_allocator/starscan/scan_loop.h
      allocator/partition_allocator/starscan/snapshot.cc
      allocator/partition_allocator/starscan/snapshot.h
      allocator/partition_allocator/starscan/stack/stack.cc
      allocator/partition_allocator/starscan/stack/stack.h
      allocator/partition_allocator/starscan/starscan_fwd.h
      allocator/partition_allocator/starscan/state_bitmap.h
      allocator/partition_allocator/starscan/stats_collector.cc
      allocator/partition_allocator/starscan/stats_collector.h
      allocator/partition_allocator/starscan/stats_reporter.h
      allocator/partition_allocator/starscan/write_protector.cc
      allocator/partition_allocator/starscan/write_protector.h
      allocator/partition_allocator/tagging.cc
      allocator/partition_allocator/tagging.h
      allocator/partition_allocator/thread_cache.cc
      allocator/partition_allocator/thread_cache.h
      allocator/partition_allocator/yield_processor.h)
    if(WIN32)
      list(APPEND SOURCES
        allocator/partition_allocator/page_allocator_internals_win.h
        allocator/partition_allocator/partition_tls_win.cc)
    elseif(UNIX)
      list(APPEND SOURCES
        allocator/partition_allocator/page_allocator_internals_posix.cc
        allocator/partition_allocator/page_allocator_internals_posix.h)
    elseif(FALSE)
      list(APPEND SOURCES
        allocator/partition_allocator/page_allocator_internals_fuchsia.h)
    endif()
  endif()
endif()

if(WIN32)
  list(APPEND SOURCES
    files/file_enumerator_win.cc
    memory/platform_shared_memory_region_win.cc
    power_monitor/power_monitor_device_source_win.cc
    power_monitor/speed_limit_observer_win.cc
    power_monitor/speed_limit_observer_win.h
    profiler/win32_stack_frame_unwinder.cc
    profiler/win32_stack_frame_unwinder.h
    rand_util_win.cc
    system/sys_info_win.cc
    time/time_win.cc)

  list(REMOVE_ITEM SOURCES
    file_descriptor_store.cc
    file_descriptor_store.h)
endif()

if(MAC)
  list(REMOVE_ITEM SOURCES process/launch_posix.cc)
  list(APPEND SOURCES
    files/file_path_watcher_fsevents.cc
    files/file_path_watcher_fsevents.h
    files/file_path_watcher_kqueue.cc
    files/file_path_watcher_kqueue.h
    mac/scoped_typeref.h
    memory/platform_shared_memory_region_mac.cc
    message_loop/message_pump_kqueue.cc
    message_loop/message_pump_kqueue.h
    power_monitor/power_monitor_device_source_mac.mm
    power_monitor/thermal_state_observer_mac.h
    power_monitor/thermal_state_observer_mac.mm
    system/sys_info_mac.mm
    time/time_conversion_posix.cc
    time/time_exploded_posix.cc
    time/time_mac.mm)

  list(APPEND LIBS bsm pmenergy pmsample)

  find_library(APPLICATION_SERVICES ApplicationServices)
  find_library(APP_KIT AppKit)
  find_library(CORE_FOUNDATION CoreFoundation)
  find_library(IO_KIT IOKit)
  find_library(OPEN_DIRECTORY OpenDirectory)
  find_library(SECURITY Security)
  list(APPEND FRAMEWORKS
    ${APPLICATION_SERVICES}
    ${APP_KIT}
    ${CORE_FOUNDATION}
    ${IO_KIT}
    ${OPEN_DIRECTORY}
    ${SECURITY})
endif()

if(APPLE)
  list(REMOVE_ITEM SOURCES
    native_library_posix.cc
    profiler/module_cache_posix.cc
    strings/sys_string_conversions_posix.cc
    synchronization/waitable_event_posix.cc
    synchronization/waitable_event_watcher_posix.cc
    threading/platform_thread_internal_posix.cc)
endif()

if(LINUX OR CHROMEOS)
  list(APPEND SOURCES
    nix/mime_util_xdg.cc
    nix/mime_util_xdg.h
    nix/xdg_util.cc
    nix/xdg_util.h
    system/sys_info_linux.cc)

  list(APPEND DEFINES USE_SYMBOLIZE)
elseif(NOT ANDROID)
  list(REMOVE_ITEM SOURCES
    linux_util.cc
    linux_util.h)
endif()

if(IOS)
  list(REMOVE_ITEM SOURCES
    files/file_path_watcher.cc
    files/file_path_watcher.h
    memory/discardable_shared_memory.cc
    memory/discardable_shared_memory.h
    process/kill.cc
    process/kill.h
    process/kill_posix.cc
    process/launch.cc
    process/launch.h
    process/launch_posix.cc
    process/memory.cc
    process/memory.h
    process/process_iterator.cc
    process/process_iterator.h
    process/process_metrics_posix.cc
    process/process_posix.cc
    sync_socket.h
    sync_socket_posix.cc
    synchronization/waitable_event_watcher.h)
  list(APPEND SOURCES
    base_paths_mac.h
    base_paths_mac.mm
    file_version_info_mac.h
    file_version_info_mac.mm
    files/file_util_mac.mm
    mac/backup_util.h
    mac/backup_util.mm
    mac/bridging.h
    mac/bundle_locations.h
    mac/bundle_locations.mm
    mac/call_with_eh_frame.cc
    mac/call_with_eh_frame.h
    mac/call_with_eh_frame_asm.S
    mac/foundation_util.h
    mac/foundation_util.mm
    mac/mac_logging.h
    mac/mac_logging.mm
    mac/mach_logging.cc
    mac/mach_logging.h
    mac/objc_release_properties.h
    mac/objc_release_properties.mm
    mac/scoped_block.h
    mac/scoped_mach_port.cc
    mac/scoped_mach_port.h
    mac/scoped_mach_vm.cc
    mac/scoped_mach_vm.h
    mac/scoped_nsautorelease_pool.h
    mac/scoped_nsautorelease_pool.mm
    mac/scoped_nsobject.h
    mac/scoped_objc_class_swizzler.h
    mac/scoped_objc_class_swizzler.mm
    mac/scoped_typeref.h
    message_loop/message_pump_mac.h
    message_loop/message_pump_mac.mm
    power_monitor/power_monitor_device_source_ios.mm
    process/memory_stubs.cc
    process/process_stubs.cc
    strings/sys_string_conversions_mac.mm
    synchronization/waitable_event_mac.cc
    system/sys_info_ios.mm
    task/thread_pool/thread_group_native_mac.h
    task/thread_pool/thread_group_native_mac.mm
    threading/platform_thread_mac.mm
    time/time_conversion_posix.cc
    time/time_mac.mm)

  if(USE_ALLOCATOR_SHIM)
    list(APPEND SOURCES
      allocator/allocator_interception_mac.h
      allocator/allocator_interception_mac.mm
      allocator/malloc_zone_functions_mac.cc
      allocator/malloc_zone_functions_mac.h)
  else()
    list(REMOVE_ITEM SOURCES
      sampling_heap_profiler/poisson_allocation_sampler.cc
      sampling_heap_profiler/poisson_allocation_sampler.h
      sampling_heap_profiler/sampling_heap_profiler.cc
      sampling_heap_profiler/sampling_heap_profiler.h)
  endif()

  find_library(UI_KIT UIKit)
  list(APPEND FRAMEWORKS ${UI_KIT})
endif()

if(DEP_LIBEVENT)
  list(APPEND LIBS libevent)
endif()

if(USE_LIBEVENT)
  list(APPEND SOURCES
    message_loop/message_pump_libevent.cc
    message_loop/message_pump_libevent.h)
endif()

if(UNIX AND NOT ANDROID AND NOT MAC)
  list(APPEND SOURCES memory/platform_shared_memory_region_posix.cc)
endif()

if(UNIX AND NOT APPLE)
  list(APPEND SOURCES
    time/time_conversion_posix.cc
    time/time_exploded_posix.cc
    time/time_now_posix.cc)

  if(CHROMECAST)
    list(APPEND SOURCES
      time/time_exploded_icu.cc)

    list(APPEND LIBS icui18n icuuc)
  endif()
endif()

if(UNIX AND NOT APPLE AND TRUE) # if (is_posix && !is_apple && !is_nacl)
  list(APPEND SOURCES
    posix/can_lower_nice_to.cc
    posix/can_lower_nice_to.h)
endif()

if((UNIX AND NOT APPLE AND NOT ANDROID AND NOT CHROMEOS_ASH) OR FALSE) # if ((is_posix && !is_apple && !is_android && !is_chromeos_ash) || is_fuchsia)
  list(APPEND SOURCES power_monitor/power_monitor_device_source_stub.cc)
endif()

if(ANDROID OR CHROMEOS_ASH)
  list(APPEND SOURCES time/time_android.cc)
endif()

if(NOT USE_GLIB)
  list(REMOVE_ITEM SOURCES
    message_loop/message_pump_glib.cc
    message_loop/message_pump_glib.h)
endif()

if(ENABLE_BASE_TRACING)
  list(APPEND SOURCES
    trace_event/auto_open_close_event.h
    trace_event/blame_context.cc
    trace_event/blame_context.h
    trace_event/builtin_categories.cc
    trace_event/builtin_categories.h
    trace_event/category_registry.cc
    trace_event/category_registry.h
    trace_event/event_name_filter.cc
    trace_event/event_name_filter.h
    trace_event/heap_profiler.h
    trace_event/interned_args_helper.cc
    trace_event/interned_args_helper.h
    trace_event/log_message.cc
    trace_event/log_message.h
    trace_event/malloc_dump_provider.cc
    trace_event/malloc_dump_provider.h
    trace_event/memory_allocator_dump.cc
    trace_event/memory_allocator_dump.h
    trace_event/memory_dump_manager.cc
    trace_event/memory_dump_manager.h
    trace_event/memory_dump_manager_test_utils.h
    trace_event/memory_dump_provider.h
    trace_event/memory_dump_provider_info.cc
    trace_event/memory_dump_provider_info.h
    trace_event/memory_dump_request_args.cc
    trace_event/memory_dump_request_args.h
    trace_event/memory_dump_scheduler.cc
    trace_event/memory_dump_scheduler.h
    trace_event/memory_infra_background_allowlist.cc
    trace_event/memory_infra_background_allowlist.h
    trace_event/memory_pressure_level_proto.cc
    trace_event/memory_pressure_level_proto.h
    trace_event/memory_usage_estimator.cc
    trace_event/memory_usage_estimator.h
    trace_event/optional_trace_event.h
    trace_event/process_memory_dump.cc
    trace_event/process_memory_dump.h
    trace_event/task_execution_macros.h
    trace_event/thread_instruction_count.cc
    trace_event/thread_instruction_count.h
    trace_event/trace_arguments.cc
    trace_event/trace_arguments.h
    trace_event/trace_buffer.cc
    trace_event/trace_buffer.h
    trace_event/trace_category.h
    trace_event/trace_config.cc
    trace_event/trace_config.h
    trace_event/trace_config_category_filter.cc
    trace_event/trace_config_category_filter.h
    trace_event/trace_conversion_helper.h
    trace_event/trace_event.h
    trace_event/trace_event_filter.cc
    trace_event/trace_event_filter.h
    trace_event/trace_event_impl.cc
    trace_event/trace_event_impl.h
    trace_event/trace_event_memory_overhead.cc
    trace_event/trace_event_memory_overhead.h
    trace_event/trace_log.cc
    trace_event/trace_log.h
    trace_event/trace_log_constants.cc
    trace_event/traced_value.cc
    trace_event/traced_value.h
    trace_event/traced_value_support.h
    trace_event/tracing_agent.cc
    trace_event/tracing_agent.h
    trace_event/typed_macros.h
    trace_event/typed_macros_embedder_support.h
    trace_event/typed_macros_internal.cc
    trace_event/typed_macros_internal.h
    tracing/perfetto_platform.cc
    tracing/perfetto_platform.h
    tracing/perfetto_task_runner.cc
    tracing/perfetto_task_runner.h
    tracing/trace_time.cc
    tracing/trace_time.h
    tracing/tracing_tls.cc
    tracing/tracing_tls.h)

  if(WIN32)
    list(APPEND SOURCES
      trace_event/trace_event_etw_export_win.cc
      trace_event/trace_event_etw_export_win.h
      trace_event/trace_logging_minimal_win.cc
      trace_event/trace_logging_minimal_win.h)
  endif()

  if(ANDROID)
    list(APPEND SOURCES
      trace_event/application_state_proto_android.cc
      trace_event/application_state_proto_android.h
      trace_event/cpufreq_monitor_android.cc
      trace_event/cpufreq_monitor_android.h
      trace_event/java_heap_dump_provider_android.cc
      trace_event/java_heap_dump_provider_android.h
      trace_event/trace_event_android.cc)
  endif()

  if(USE_ALLOCATOR STREQUAL "partition")
    list(APPEND SOURCES
      trace_event/address_space_dump_provider.cc
      trace_event/address_space_dump_provider.h)
  endif()
else()
  list(APPEND SOURCES
    trace_event/trace_event_stub.cc
    trace_event/trace_event_stub.h)
endif()

add_library(basium_base ${SOURCES})
target_link_libraries(basium_base PRIVATE ${LIBS})
target_link_libraries(basium_base PUBLIC ${PUBLIC_LIBS})
target_link_libraries(basium_base PRIVATE ${FRAMEWORKS})
target_compile_definitions(basium_base PUBLIC ${DEFINES})
target_compile_options(basium_base PUBLIC -fno-exceptions)
target_include_directories(basium_base PUBLIC ${BASIUM_COMMON_INCLUDE_DIR})
add_dependencies(basium_base ${DEPS} ${PUBLIC_DEPS})

set(CFI_CAST_CHECK OFF)
set(CFI_ICALL_CHECK OFF)
set(CFI_ENFORCEMENT_TRAP OFF)
set(CFI_ENFORCEMENT_DIAGNOSTIC OFF)
if(CFI)
  if(CFI_CAST)
    set(CFI_CAST_CHECK ON)
  endif()
  if(CFI_ICALL)
    set(CFI_ICALL_CHECK ON)
  endif()
  if(NOT CFI_DIAG)
    set(CFI_ENFORCEMENT_TRAP ON)
  endif()
  if(CFI_DIAG AND NOT CFI_RECOVER)
    set(CFI_ENFORCEMENT_DIAGNOSTIC ON)
  endif()
endif()

buildflag_header(cfi_buildflags
  HEADER "cfi_buildflags.h"

  FLAGS
  CFI_CAST_CHECK=${CFI_CAST_CHECK}
  CFI_ICALL_CHECK=${CFI_ICALL_CHECK}
  CFI_ENFORCEMENT_TRAP=${CFI_ENFORCEMENT_TRAP}
  CFI_ENFORCEMENT_DIAGNOSTIC=${CFI_ENFORCEMENT_DIAGNOSTIC})

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND (STRIP_ABSOLUTE_PATHS_FROM_DEBUG_SYMBOLS OR USE_CUSTOM_LIBCXX))
  set(ENABLE_GDBINIT_WARNING ON)
else()
  set(ENABLE_GDBINIT_WARNING OFF)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND STRIP_ABSOLUTE_PATHS_FROM_DEBUG_SYMBOLS AND MAC)
  set(ENABLE_LLDBINIT_WARNING ON)
else()
  set(ENABLE_LLDBINIT_WARNING OFF)
endif()
buildflag_header(debugging_buildflags
  HEADER "debugging_buildflags.h"
  HEADER_DIR "base/debug"

  FLAGS
  ENABLE_LOCATION_SOURCE=${ENABLE_LOCATION_SOURCE}
  FROM_HERE_USES_LOCATION_BUILTINS=${FROM_HERE_USES_LOCATION_BUILTINS}
  ENABLE_PROFILING=${ENABLE_PROFILING}
  CAN_UNWIND_WITH_FRAME_POINTERS=${CAN_UNWIND_WITH_FRAME_POINTERS}
  CAN_UNWIND_WITH_CFI_TABLE=${CAN_UNWIND_WITH_CFI_TABLE}
  ENABLE_GDBINIT_WARNING=${ENABLE_GDBINIT_WARNING}
  ENABLE_LLDBINIT_WARNING=${ENABLE_LLDBINIT_WARNING}
  EXPENSIVE_DCHECKS_ARE_ON=${ENABLE_EXPENSIVE_DCHECKS}
  ENABLE_STACK_TRACE_LINE_NUMBERS=${ENABLE_STACK_TRACE_LINE_NUMBERS})

if(CHROMEOS_ASH)
  set(FEATURE_LIST_FLAGS
    ENABLE_BANNED_BASE_FEATURE_PREFIX=true
    BANNED_BASE_FEATURE_PREFIX=\"CrOSLateBoot\")
else()
  set(FEATURE_LIST_FLAGS ENABLE_BANNED_BASE_FEATURE_PREFIX=false)
endif()
buildflag_header(feature_list_buildflags
  HEADER "feature_list_buildflags.h"
  FLAGS ${FEATURE_LIST_FLAGS})

buildflag_header(synchronization_buildflags
  HEADER "synchronization_buildflags.h"
  HEADER_DIR "base/synchronization"

  FLAGS
  ENABLE_MUTEX_PRIORITY_INHERITANCE=$ENABLE_MUTEX_PRIORITY_INHERITANCE)

buildflag_header(logging_buildflags
  HEADER "logging_buildflags.h"
  FLAGS ENABLE_LOG_ERROR_NOT_REACHED=${ENABLE_LOG_ERROR_NOT_REACHED})

buildflag_header(clang_profiling_buildflags
  HEADER "clang_profiling_buildflags.h"
  HEADER_DIR "base"
  
  FLAGS
  CLANG_PROFILING=OFF
  CLANG_PROFILING_INSIDE_SANDBOX=OFF
  USE_CLANG_COVERAGE=OFF)

buildflag_header(sanitizer_buildflags
  HEADER "sanitizer_buildflags.h"
  HEADER_DIR "base"

  FLAGS IS_HWASAN=${HWASAN})

buildflag_header(tracing_buildflags
  HEADER "tracing_buildflags.h"

  FLAGS
  ENABLE_BASE_TRACING=${ENABLE_BASE_TRACING}
  USE_PERFETTO_CLIENT_LIBRARY=${USE_PERFETTO_CLIENT_LIBRARY}
  OPTIONAL_TRACE_EVENTS_ENABLED=${OPTIONAL_TRACE_EVENTS_ENABLED})

buildflag_header(profiler_buildflags
  HEADER "profiler_buildflags.h"
  HEADER_DIR "base/profiler"

  FLAGS
  ENABLE_ARM_CFI_TABLE=${ENABLE_ARM_CFI_TABLE}
  IOS_STACK_PROFILER_ENABLED=${IOS_STACK_PROFILER_ENABLED}
  USE_ANDROID_UNWINDER_V2=${USE_ANDROID_UNWINDER_V2})

set(SOURCES
  base_switches.cc
  base_switches.h)
set(DEPS chromeos_buildflags)

if(WIN32)
  list(APPEND SOURCES
    win/static_constants.cc
    win/static_constants.h)

  set(PUBLIC_DEPS basium_win_pe_image)
endif()
add_library(basium_base_static STATIC ${SOURCES})
target_include_directories(basium_base_static PUBLIC ${BASIUM_COMMON_INCLUDE_DIR})
target_compile_options(basium_base_static PRIVATE -fno-exceptions)

set(SOURCES
  i18n/base_i18n_export.h
  i18n/base_i18n_switches.cc
  i18n/base_i18n_switches.h
  i18n/break_iterator.cc
  i18n/break_iterator.h
  i18n/case_conversion.cc
  i18n/case_conversion.h
  i18n/char_iterator.cc
  i18n/char_iterator.h
  i18n/character_encoding.cc
  i18n/character_encoding.h
  i18n/encoding_detection.cc
  i18n/encoding_detection.h
  i18n/file_util_icu.cc
  i18n/file_util_icu.h
  i18n/i18n_constants.cc
  i18n/i18n_constants.h
  i18n/icu_string_conversions.cc
  i18n/icu_string_conversions.h
  i18n/icu_util.cc
  i18n/icu_util.h
  i18n/message_formatter.cc
  i18n/message_formatter.h
  i18n/number_formatting.cc
  i18n/number_formatting.h
  i18n/rtl.cc
  i18n/rtl.h
  i18n/streaming_utf8_validator.cc
  i18n/streaming_utf8_validator.h
  i18n/string_compare.cc
  i18n/string_compare.h
  i18n/string_search.cc
  i18n/string_search.h
  i18n/time_formatting.cc
  i18n/time_formatting.h
  i18n/timezone.cc
  i18n/timezone.h
  i18n/unicodestring.h
  i18n/utf8_validator_tables.cc
  i18n/utf8_validator_tables.h)
set(DEFINES BASE_I18N_IMPLEMENTATION)
set(PUBLIC_DEPS ced icu)
set(DEPS basium_base basium_dynamic_annotations)
if(MAC)
  find_library(CORE_FOUNDATION CoreFoundation)
  list(APPEND PUBLIC_DEPS ${CORE_FOUNDATION})
endif()
add_library(basium_base_i18n ${SOURCES})
target_link_libraries(basium_base_i18n PUBLIC ${PUBLIC_DEPS})
target_link_libraries(basium_base_i18n PRIVATE ${DEPS})
target_compile_definitions(basium_base_i18n PUBLIC ${DEFINES})
add_dependencies(basium_base_i18n chromeos_buildflags)

if(NOT IOS)
  add_executable(basium_build_utf8_validator_tables i18n/build_utf8_validator_tables.cc)
  target_link_libraries(basium_build_utf8_validator_tables PRIVATE basium_base icuuc)

  add_executable(basium_check_example check_example.cc)
  target_link_libraries(basium_check_example basium_base)

  add_executable(basium_json_perftest_decodebench json/json_perftest_decodebench.cc)
  target_link_libraries(basium_json_perftest_decodebench basium_base)
endif()
