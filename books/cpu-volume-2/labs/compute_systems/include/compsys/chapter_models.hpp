#pragma once

#include <cstdint>
#include <span>

namespace compsys {

struct Chapter01PipelineReport {
    std::int32_t records_total = 0;
    std::int32_t accepted = 0;
    std::int32_t rejected = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Chapter01PipelineReport model_ch01_input_pipeline(
    std::span<const std::int32_t> records);

struct Chapter02BranchReport {
    std::int32_t iterations = 0;
    std::int32_t predicted_taken = 0;
    std::int32_t actual_taken = 0;
    std::int32_t mispredictions = 0;
};

[[nodiscard]] Chapter02BranchReport model_ch02_branch_predictor(
    std::span<const std::int32_t> values,
    std::int32_t taken_threshold);

struct Chapter03MemoryAccessReport {
    std::int32_t access_count = 0;
    std::int32_t unique_cache_lines = 0;
    std::int32_t unique_pages = 0;
    std::int32_t tlb_misses = 0;
};

[[nodiscard]] Chapter03MemoryAccessReport model_ch03_cache_tlb(
    std::span<const std::uint64_t> byte_offsets,
    std::int32_t cache_line_bytes,
    std::int32_t page_bytes,
    std::int32_t tlb_entries);

struct Chapter04NumaReport {
    std::int32_t access_count = 0;
    std::int32_t local_accesses = 0;
    std::int32_t remote_accesses = 0;
    std::int32_t coherence_transfers = 0;
};

[[nodiscard]] Chapter04NumaReport model_ch04_numa_coherence(
    std::span<const std::int32_t> worker_nodes,
    std::span<const std::int32_t> memory_nodes);

struct Chapter05RooflineReport {
    double arithmetic_intensity = 0.0;
    double compute_bound_gflops = 0.0;
    double memory_bound_gflops = 0.0;
    double attainable_gflops = 0.0;
    bool memory_bound = false;
};

[[nodiscard]] Chapter05RooflineReport model_ch05_roofline(
    double flops,
    double bytes_read_written,
    double peak_gflops,
    double bandwidth_gbytes_per_second);

struct Chapter05bPmuSimdReport {
    std::int64_t scalar_instructions = 0;
    std::int64_t vector_instructions = 0;
    std::int64_t retired_instructions = 0;
    double vector_fraction = 0.0;
    bool counter_available = false;
    bool evidence_complete = false;
};

[[nodiscard]] Chapter05bPmuSimdReport model_ch05b_pmu_simd_evidence(
    std::int64_t scalar_instructions,
    std::int64_t vector_instructions,
    bool counter_available);

struct Chapter06LayoutReport {
    std::int64_t records = 0;
    std::int64_t aos_bytes_touched = 0;
    std::int64_t soa_bytes_touched = 0;
    std::int64_t saved_bytes = 0;
};

[[nodiscard]] Chapter06LayoutReport model_ch06_layout_locality(
    std::int64_t records,
    std::int32_t field_count,
    std::int32_t hot_field_count,
    std::int32_t field_bytes);

struct Chapter07SimdReport {
    std::int32_t elements = 0;
    std::int32_t lane_width = 0;
    std::int32_t vector_iterations = 0;
    std::int32_t scalar_tail = 0;
    double lane_utilization = 0.0;
};

[[nodiscard]] Chapter07SimdReport model_ch07_simd_ilp(
    std::int32_t elements,
    std::int32_t lane_width);

struct Chapter08KernelReport {
    std::int32_t input_count = 0;
    std::int32_t filtered_count = 0;
    std::int64_t mapped_sum = 0;
    std::int64_t reduced_sum = 0;
};

[[nodiscard]] Chapter08KernelReport model_ch08_map_filter_reduce(
    std::span<const std::int32_t> values);

struct Chapter09SchedulingReport {
    std::int32_t worker_count = 0;
    std::int32_t task_count = 0;
    std::int64_t min_load = 0;
    std::int64_t max_load = 0;
    std::int64_t imbalance = 0;
};

[[nodiscard]] Chapter09SchedulingReport model_ch09_thread_scheduling(
    std::span<const std::int32_t> task_costs,
    std::int32_t worker_count);

struct Chapter09bLinuxWaitReport {
    std::int32_t poll_count = 0;
    std::int32_t wakeups = 0;
    std::int32_t spurious_wakeups = 0;
    bool completed = false;
};

[[nodiscard]] Chapter09bLinuxWaitReport model_ch09b_linux_wait_wake(
    std::span<const bool> ready_observations);

struct Chapter10BackpressureReport {
    std::int32_t ticks = 0;
    std::int32_t capacity = 0;
    std::int32_t produced = 0;
    std::int32_t consumed = 0;
    std::int32_t backpressure_events = 0;
    std::int32_t max_depth = 0;
};

[[nodiscard]] Chapter10BackpressureReport model_ch10_sync_backpressure(
    std::int32_t ticks,
    std::int32_t produce_per_tick,
    std::int32_t consume_per_tick,
    std::int32_t capacity);

struct Chapter11AtomicPublicationReport {
    std::int32_t published_versions = 0;
    std::int32_t observed_versions = 0;
    std::int32_t stale_reads = 0;
    std::int32_t final_version = 0;
    bool monotonic_observations = true;
};

[[nodiscard]] Chapter11AtomicPublicationReport model_ch11_atomic_publication(
    std::span<const std::int32_t> observed_versions,
    std::int32_t final_version);

struct Chapter12LockFreeQueueReport {
    std::int32_t capacity = 0;
    std::int32_t pushed = 0;
    std::int32_t popped = 0;
    std::int32_t rejected_full = 0;
    std::int32_t empty_pops = 0;
};

[[nodiscard]] Chapter12LockFreeQueueReport model_ch12_spsc_ring(
    std::span<const std::int32_t> operations,
    std::int32_t capacity);

struct Chapter12bReclamationReport {
    std::int32_t retired_nodes = 0;
    std::int32_t protected_nodes = 0;
    std::int32_t reclaimed_nodes = 0;
    std::int32_t deferred_nodes = 0;
};

[[nodiscard]] Chapter12bReclamationReport model_ch12b_epoch_reclamation(
    std::span<const std::int32_t> retired_node_ids,
    std::span<const std::int32_t> protected_node_ids);

struct Chapter13ReduceScanPartitionReport {
    std::int32_t input_count = 0;
    std::int64_t reduce_sum = 0;
    std::int64_t last_prefix = 0;
    std::int32_t partition_true_count = 0;
    std::int32_t partition_false_count = 0;
};

[[nodiscard]] Chapter13ReduceScanPartitionReport model_ch13_reduce_scan_partition(
    std::span<const std::int32_t> values,
    std::int32_t partition_threshold);

struct Chapter14WorkStealingReport {
    std::int32_t worker_count = 0;
    std::int32_t initial_tasks = 0;
    std::int32_t stolen_tasks = 0;
    std::int32_t completed_tasks = 0;
    std::int32_t max_queue_depth = 0;
};

[[nodiscard]] Chapter14WorkStealingReport model_ch14_work_stealing(
    std::span<const std::int32_t> initial_worker_queues);

struct Chapter15AsyncIoReport {
    std::int32_t requested = 0;
    std::int32_t submitted = 0;
    std::int32_t completed = 0;
    std::int32_t backpressure_events = 0;
    std::int32_t max_in_flight = 0;
};

[[nodiscard]] Chapter15AsyncIoReport model_ch15_async_io_backpressure(
    std::int32_t request_count,
    std::int32_t queue_depth,
    std::int32_t completions_per_tick);

struct Chapter16DistributedModelReport {
    std::int32_t logical_tasks = 0;
    std::int32_t attempts = 0;
    std::int32_t duplicate_attempts = 0;
    std::int32_t stale_generation_attempts = 0;
};

[[nodiscard]] Chapter16DistributedModelReport model_ch16_distributed_attempts(
    std::span<const std::int32_t> task_ids,
    std::span<const std::int32_t> generations,
    std::int32_t current_generation);

struct Chapter17ReplicationReport {
    std::int32_t replica_count = 0;
    std::int32_t ack_count = 0;
    std::int32_t quorum_size = 0;
    bool committed = false;
};

[[nodiscard]] Chapter17ReplicationReport model_ch17_quorum_commit(
    std::int32_t replica_count,
    std::span<const bool> acknowledgements);

struct Chapter18RecoveryReport {
    std::int32_t input_records = 0;
    std::int32_t checkpoint_interval = 0;
    std::int32_t checkpoints = 0;
    std::int32_t replay_from_record = 0;
    std::int32_t replay_records = 0;
};

[[nodiscard]] Chapter18RecoveryReport model_ch18_checkpoint_recovery(
    std::int32_t input_records,
    std::int32_t checkpoint_interval,
    std::int32_t failed_after_record);

}  // namespace compsys
