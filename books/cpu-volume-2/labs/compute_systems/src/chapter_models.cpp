#include <compsys/chapter_models.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

namespace compsys {
namespace {

constexpr std::int32_t COMPSYS_MODEL_RECORD_ACCEPT_THRESHOLD = 0;
constexpr std::int32_t COMPSYS_MODEL_HISTORY_INITIAL_PREDICTION = 0;
constexpr std::int32_t COMPSYS_MODEL_PUSH_OPERATION = 1;
constexpr std::int32_t COMPSYS_MODEL_POP_OPERATION = -1;

void require_positive(std::int64_t value, const char* message) {
    if (value <= 0) {
        throw std::runtime_error(message);
    }
}

[[nodiscard]] std::int32_t ceil_div(std::int32_t numerator,
                                    std::int32_t denominator) {
    require_positive(denominator, "denominator must be positive");
    if (numerator <= 0) {
        return 0;
    }
    return (numerator + denominator - 1) / denominator;
}

[[nodiscard]] bool contains_value(std::span<const std::int32_t> values,
                                  std::int32_t needle) {
    return std::find(values.begin(), values.end(), needle) != values.end();
}

}  // namespace

Chapter01PipelineReport model_ch01_input_pipeline(
    std::span<const std::int32_t> records) {
    Chapter01PipelineReport report;
    report.records_total = static_cast<std::int32_t>(records.size());
    for (const std::int32_t record : records) {
        if (record >= COMPSYS_MODEL_RECORD_ACCEPT_THRESHOLD) {
            ++report.accepted;
            report.checksum = (report.checksum * 131) + record + 17;
        } else {
            ++report.rejected;
            report.checksum = (report.checksum * 131) - record + 19;
        }
    }
    return report;
}

Chapter02BranchReport model_ch02_branch_predictor(
    std::span<const std::int32_t> values,
    std::int32_t taken_threshold) {
    Chapter02BranchReport report;
    report.iterations = static_cast<std::int32_t>(values.size());
    bool predicted_taken = false;
    for (const std::int32_t value : values) {
        const bool actual_taken = value >= taken_threshold;
        if (predicted_taken) {
            ++report.predicted_taken;
        }
        if (actual_taken) {
            ++report.actual_taken;
        }
        if (predicted_taken != actual_taken) {
            ++report.mispredictions;
        }
        predicted_taken = actual_taken;
    }
    return report;
}

Chapter03MemoryAccessReport model_ch03_cache_tlb(
    std::span<const std::uint64_t> byte_offsets,
    std::int32_t cache_line_bytes,
    std::int32_t page_bytes,
    std::int32_t tlb_entries) {
    require_positive(cache_line_bytes, "cache line bytes must be positive");
    require_positive(page_bytes, "page bytes must be positive");
    require_positive(tlb_entries, "TLB entries must be positive");

    std::set<std::uint64_t> cache_lines;
    std::vector<std::uint64_t> tlb_lru;
    std::set<std::uint64_t> unique_pages;
    std::int32_t tlb_misses = 0;

    for (const std::uint64_t offset : byte_offsets) {
        cache_lines.insert(offset / static_cast<std::uint64_t>(cache_line_bytes));
        const std::uint64_t page = offset / static_cast<std::uint64_t>(page_bytes);
        unique_pages.insert(page);

        const auto it = std::find(tlb_lru.begin(), tlb_lru.end(), page);
        if (it == tlb_lru.end()) {
            ++tlb_misses;
            if (static_cast<std::int32_t>(tlb_lru.size()) == tlb_entries) {
                tlb_lru.erase(tlb_lru.begin());
            }
        } else {
            tlb_lru.erase(it);
        }
        tlb_lru.push_back(page);
    }

    Chapter03MemoryAccessReport report;
    report.access_count = static_cast<std::int32_t>(byte_offsets.size());
    report.unique_cache_lines = static_cast<std::int32_t>(cache_lines.size());
    report.unique_pages = static_cast<std::int32_t>(unique_pages.size());
    report.tlb_misses = tlb_misses;
    return report;
}

Chapter04NumaReport model_ch04_numa_coherence(
    std::span<const std::int32_t> worker_nodes,
    std::span<const std::int32_t> memory_nodes) {
    if (worker_nodes.size() != memory_nodes.size()) {
        throw std::runtime_error("worker and memory node spans must have equal size");
    }

    Chapter04NumaReport report;
    report.access_count = static_cast<std::int32_t>(worker_nodes.size());
    std::int32_t previous_writer_node = -1;
    for (std::size_t i = 0; i < worker_nodes.size(); ++i) {
        const std::int32_t worker_node = worker_nodes[i];
        const std::int32_t memory_node = memory_nodes[i];
        if (worker_node < 0 || memory_node < 0) {
            throw std::runtime_error("NUMA node ids must not be negative");
        }
        if (worker_node == memory_node) {
            ++report.local_accesses;
        } else {
            ++report.remote_accesses;
        }
        if (previous_writer_node != -1 && previous_writer_node != worker_node) {
            ++report.coherence_transfers;
        }
        previous_writer_node = worker_node;
    }
    return report;
}

Chapter05RooflineReport model_ch05_roofline(
    double flops,
    double bytes_read_written,
    double peak_gflops,
    double bandwidth_gbytes_per_second) {
    if (flops <= 0.0 || bytes_read_written <= 0.0 || peak_gflops <= 0.0 ||
        bandwidth_gbytes_per_second <= 0.0) {
        throw std::runtime_error("roofline inputs must be positive");
    }

    Chapter05RooflineReport report;
    report.arithmetic_intensity = flops / bytes_read_written;
    report.compute_bound_gflops = peak_gflops;
    report.memory_bound_gflops =
        report.arithmetic_intensity * bandwidth_gbytes_per_second;
    report.attainable_gflops =
        std::min(report.compute_bound_gflops, report.memory_bound_gflops);
    report.memory_bound = report.memory_bound_gflops < report.compute_bound_gflops;
    return report;
}

Chapter05bPmuSimdReport model_ch05b_pmu_simd_evidence(
    std::int64_t scalar_instructions,
    std::int64_t vector_instructions,
    bool counter_available) {
    if (scalar_instructions < 0 || vector_instructions < 0) {
        throw std::runtime_error("instruction counts must not be negative");
    }
    Chapter05bPmuSimdReport report;
    report.scalar_instructions = scalar_instructions;
    report.vector_instructions = vector_instructions;
    report.retired_instructions = scalar_instructions + vector_instructions;
    report.counter_available = counter_available;
    if (report.retired_instructions > 0) {
        report.vector_fraction =
            static_cast<double>(vector_instructions) /
            static_cast<double>(report.retired_instructions);
    }
    report.evidence_complete = counter_available && report.retired_instructions > 0;
    return report;
}

Chapter06LayoutReport model_ch06_layout_locality(
    std::int64_t records,
    std::int32_t field_count,
    std::int32_t hot_field_count,
    std::int32_t field_bytes) {
    require_positive(records, "records must be positive");
    require_positive(field_count, "field count must be positive");
    require_positive(hot_field_count, "hot field count must be positive");
    require_positive(field_bytes, "field bytes must be positive");
    if (hot_field_count > field_count) {
        throw std::runtime_error("hot field count must not exceed field count");
    }

    Chapter06LayoutReport report;
    report.records = records;
    report.aos_bytes_touched =
        records * static_cast<std::int64_t>(field_count) * field_bytes;
    report.soa_bytes_touched =
        records * static_cast<std::int64_t>(hot_field_count) * field_bytes;
    report.saved_bytes = report.aos_bytes_touched - report.soa_bytes_touched;
    return report;
}

Chapter07SimdReport model_ch07_simd_ilp(std::int32_t elements,
                                        std::int32_t lane_width) {
    require_positive(elements, "elements must be positive");
    require_positive(lane_width, "lane width must be positive");
    Chapter07SimdReport report;
    report.elements = elements;
    report.lane_width = lane_width;
    report.vector_iterations = elements / lane_width;
    report.scalar_tail = elements % lane_width;
    const std::int32_t vector_slots =
        ceil_div(elements, lane_width) * lane_width;
    report.lane_utilization =
        static_cast<double>(elements) / static_cast<double>(vector_slots);
    return report;
}

Chapter08KernelReport model_ch08_map_filter_reduce(
    std::span<const std::int32_t> values) {
    Chapter08KernelReport report;
    report.input_count = static_cast<std::int32_t>(values.size());
    for (const std::int32_t value : values) {
        if ((value % 2) == 0) {
            ++report.filtered_count;
            const std::int32_t mapped = value * value;
            report.mapped_sum += mapped;
            report.reduced_sum += mapped;
        }
    }
    return report;
}

Chapter09SchedulingReport model_ch09_thread_scheduling(
    std::span<const std::int32_t> task_costs,
    std::int32_t worker_count) {
    require_positive(worker_count, "worker count must be positive");
    std::vector<std::int64_t> loads(static_cast<std::size_t>(worker_count), 0);
    for (const std::int32_t cost : task_costs) {
        if (cost < 0) {
            throw std::runtime_error("task cost must not be negative");
        }
        auto it = std::min_element(loads.begin(), loads.end());
        *it += cost;
    }

    Chapter09SchedulingReport report;
    report.worker_count = worker_count;
    report.task_count = static_cast<std::int32_t>(task_costs.size());
    report.min_load = *std::min_element(loads.begin(), loads.end());
    report.max_load = *std::max_element(loads.begin(), loads.end());
    report.imbalance = report.max_load - report.min_load;
    return report;
}

Chapter09bLinuxWaitReport model_ch09b_linux_wait_wake(
    std::span<const bool> ready_observations) {
    Chapter09bLinuxWaitReport report;
    report.poll_count = static_cast<std::int32_t>(ready_observations.size());
    for (const bool ready : ready_observations) {
        ++report.wakeups;
        if (ready) {
            report.completed = true;
            return report;
        }
        ++report.spurious_wakeups;
    }
    return report;
}

Chapter10BackpressureReport model_ch10_sync_backpressure(
    std::int32_t ticks,
    std::int32_t produce_per_tick,
    std::int32_t consume_per_tick,
    std::int32_t capacity) {
    require_positive(ticks, "ticks must be positive");
    require_positive(produce_per_tick, "produce rate must be positive");
    require_positive(consume_per_tick, "consume rate must be positive");
    require_positive(capacity, "capacity must be positive");

    Chapter10BackpressureReport report;
    report.ticks = ticks;
    report.capacity = capacity;
    std::int32_t depth = 0;
    for (std::int32_t tick = 0; tick < ticks; ++tick) {
        const std::int32_t available_space = capacity - depth;
        const std::int32_t accepted = std::min(produce_per_tick, available_space);
        const std::int32_t rejected = produce_per_tick - accepted;
        report.produced += accepted;
        if (rejected > 0) {
            ++report.backpressure_events;
        }
        depth += accepted;
        const std::int32_t consumed = std::min(depth, consume_per_tick);
        depth -= consumed;
        report.consumed += consumed;
        report.max_depth = std::max(report.max_depth, depth);
    }
    return report;
}

Chapter11AtomicPublicationReport model_ch11_atomic_publication(
    std::span<const std::int32_t> observed_versions,
    std::int32_t final_version) {
    require_positive(final_version, "final version must be positive");
    Chapter11AtomicPublicationReport report;
    report.published_versions = final_version;
    report.observed_versions = static_cast<std::int32_t>(observed_versions.size());
    report.final_version = final_version;
    std::int32_t previous = COMPSYS_MODEL_HISTORY_INITIAL_PREDICTION;
    for (const std::int32_t observed : observed_versions) {
        if (observed < 0) {
            throw std::runtime_error("observed version must not be negative");
        }
        if (observed < previous) {
            report.monotonic_observations = false;
        }
        if (observed < final_version) {
            ++report.stale_reads;
        }
        previous = observed;
    }
    return report;
}

Chapter12LockFreeQueueReport model_ch12_spsc_ring(
    std::span<const std::int32_t> operations,
    std::int32_t capacity) {
    require_positive(capacity, "capacity must be positive");
    Chapter12LockFreeQueueReport report;
    report.capacity = capacity;
    std::int32_t depth = 0;
    for (const std::int32_t operation : operations) {
        if (operation == COMPSYS_MODEL_PUSH_OPERATION) {
            if (depth == capacity) {
                ++report.rejected_full;
            } else {
                ++depth;
                ++report.pushed;
            }
        } else if (operation == COMPSYS_MODEL_POP_OPERATION) {
            if (depth == 0) {
                ++report.empty_pops;
            } else {
                --depth;
                ++report.popped;
            }
        } else {
            throw std::runtime_error("queue operation must be 1 for push or -1 for pop");
        }
    }
    return report;
}

Chapter12bReclamationReport model_ch12b_epoch_reclamation(
    std::span<const std::int32_t> retired_node_ids,
    std::span<const std::int32_t> protected_node_ids) {
    Chapter12bReclamationReport report;
    report.retired_nodes = static_cast<std::int32_t>(retired_node_ids.size());
    report.protected_nodes = static_cast<std::int32_t>(protected_node_ids.size());
    for (const std::int32_t node : retired_node_ids) {
        if (contains_value(protected_node_ids, node)) {
            ++report.deferred_nodes;
        } else {
            ++report.reclaimed_nodes;
        }
    }
    return report;
}

Chapter13ReduceScanPartitionReport model_ch13_reduce_scan_partition(
    std::span<const std::int32_t> values,
    std::int32_t partition_threshold) {
    Chapter13ReduceScanPartitionReport report;
    report.input_count = static_cast<std::int32_t>(values.size());
    std::int64_t prefix = 0;
    for (const std::int32_t value : values) {
        report.reduce_sum += value;
        prefix += value;
        report.last_prefix = prefix;
        if (value >= partition_threshold) {
            ++report.partition_true_count;
        } else {
            ++report.partition_false_count;
        }
    }
    return report;
}

Chapter14WorkStealingReport model_ch14_work_stealing(
    std::span<const std::int32_t> initial_worker_queues) {
    if (initial_worker_queues.empty()) {
        throw std::runtime_error("at least one worker queue is required");
    }
    Chapter14WorkStealingReport report;
    report.worker_count = static_cast<std::int32_t>(initial_worker_queues.size());
    for (const std::int32_t queue_depth : initial_worker_queues) {
        if (queue_depth < 0) {
            throw std::runtime_error("queue depth must not be negative");
        }
        report.initial_tasks += queue_depth;
        report.max_queue_depth = std::max(report.max_queue_depth, queue_depth);
    }
    const std::int32_t target_load =
        ceil_div(report.initial_tasks, report.worker_count);
    for (const std::int32_t queue_depth : initial_worker_queues) {
        if (queue_depth > target_load) {
            report.stolen_tasks += queue_depth - target_load;
        }
    }
    report.completed_tasks = report.initial_tasks;
    return report;
}

Chapter15AsyncIoReport model_ch15_async_io_backpressure(
    std::int32_t request_count,
    std::int32_t queue_depth,
    std::int32_t completions_per_tick) {
    require_positive(request_count, "request count must be positive");
    require_positive(queue_depth, "queue depth must be positive");
    require_positive(completions_per_tick, "completions per tick must be positive");

    Chapter15AsyncIoReport report;
    report.requested = request_count;
    std::int32_t remaining = request_count;
    std::int32_t in_flight = 0;
    while (remaining > 0 || in_flight > 0) {
        while (remaining > 0 && in_flight < queue_depth) {
            --remaining;
            ++in_flight;
            ++report.submitted;
            report.max_in_flight = std::max(report.max_in_flight, in_flight);
        }
        if (remaining > 0 && in_flight == queue_depth) {
            ++report.backpressure_events;
        }
        const std::int32_t completed = std::min(in_flight, completions_per_tick);
        in_flight -= completed;
        report.completed += completed;
    }
    return report;
}

Chapter16DistributedModelReport model_ch16_distributed_attempts(
    std::span<const std::int32_t> task_ids,
    std::span<const std::int32_t> generations,
    std::int32_t current_generation) {
    if (task_ids.size() != generations.size()) {
        throw std::runtime_error("task id and generation spans must have equal size");
    }
    Chapter16DistributedModelReport report;
    report.attempts = static_cast<std::int32_t>(task_ids.size());
    std::map<std::int32_t, std::int32_t> current_attempt_counts;
    for (std::size_t i = 0; i < task_ids.size(); ++i) {
        if (task_ids[i] < 0 || generations[i] < 0) {
            throw std::runtime_error("task ids and generations must not be negative");
        }
        if (generations[i] != current_generation) {
            ++report.stale_generation_attempts;
            continue;
        }
        ++current_attempt_counts[task_ids[i]];
    }
    report.logical_tasks = static_cast<std::int32_t>(current_attempt_counts.size());
    for (const auto& [_, count] : current_attempt_counts) {
        if (count > 1) {
            report.duplicate_attempts += count - 1;
        }
    }
    return report;
}

Chapter17ReplicationReport model_ch17_quorum_commit(
    std::int32_t replica_count,
    std::span<const bool> acknowledgements) {
    require_positive(replica_count, "replica count must be positive");
    if (static_cast<std::int32_t>(acknowledgements.size()) != replica_count) {
        throw std::runtime_error("acknowledgement count must equal replica count");
    }
    Chapter17ReplicationReport report;
    report.replica_count = replica_count;
    report.quorum_size = (replica_count / 2) + 1;
    for (const bool acknowledged : acknowledgements) {
        if (acknowledged) {
            ++report.ack_count;
        }
    }
    report.committed = report.ack_count >= report.quorum_size;
    return report;
}

Chapter18RecoveryReport model_ch18_checkpoint_recovery(
    std::int32_t input_records,
    std::int32_t checkpoint_interval,
    std::int32_t failed_after_record) {
    require_positive(input_records, "input records must be positive");
    require_positive(checkpoint_interval, "checkpoint interval must be positive");
    if (failed_after_record < 0 || failed_after_record > input_records) {
        throw std::runtime_error("failed record must be inside input range");
    }

    Chapter18RecoveryReport report;
    report.input_records = input_records;
    report.checkpoint_interval = checkpoint_interval;
    report.checkpoints = input_records / checkpoint_interval;
    report.replay_from_record =
        (failed_after_record / checkpoint_interval) * checkpoint_interval;
    report.replay_records = input_records - report.replay_from_record;
    return report;
}

}  // namespace compsys
