#include <compsys/chapter_models.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>

namespace {

constexpr std::int32_t COMPSYS_CHAPTER_MODEL_COUNT = 21;
constexpr std::int32_t COMPSYS_BRANCH_THRESHOLD = 5;
constexpr std::int32_t COMPSYS_CACHE_LINE_BYTES = 64;
constexpr std::int32_t COMPSYS_PAGE_BYTES = 4096;
constexpr std::int32_t COMPSYS_TLB_ENTRIES = 2;
constexpr double COMPSYS_ROOFLINE_FLOPS = 200.0;
constexpr double COMPSYS_ROOFLINE_BYTES = 100.0;
constexpr double COMPSYS_ROOFLINE_PEAK_GFLOPS = 10.0;
constexpr double COMPSYS_ROOFLINE_BANDWIDTH_GBPS = 20.0;
constexpr std::int32_t COMPSYS_LAYOUT_FIELDS = 4;
constexpr std::int32_t COMPSYS_LAYOUT_HOT_FIELDS = 1;
constexpr std::int32_t COMPSYS_FIELD_BYTES = 8;
constexpr std::int32_t COMPSYS_SIMD_ELEMENTS = 10;
constexpr std::int32_t COMPSYS_SIMD_LANES = 4;
constexpr std::int32_t COMPSYS_SCHEDULING_WORKERS = 2;
constexpr std::int32_t COMPSYS_BACKPRESSURE_TICKS = 3;
constexpr std::int32_t COMPSYS_BACKPRESSURE_PRODUCE_RATE = 4;
constexpr std::int32_t COMPSYS_BACKPRESSURE_CONSUME_RATE = 1;
constexpr std::int32_t COMPSYS_BACKPRESSURE_CAPACITY = 5;
constexpr std::int32_t COMPSYS_FINAL_VERSION = 3;
constexpr std::int32_t COMPSYS_RING_CAPACITY = 2;
constexpr std::int32_t COMPSYS_PARTITION_THRESHOLD = 2;
constexpr std::int32_t COMPSYS_IO_REQUESTS = 5;
constexpr std::int32_t COMPSYS_IO_QUEUE_DEPTH = 2;
constexpr std::int32_t COMPSYS_IO_COMPLETIONS_PER_TICK = 1;
constexpr std::int32_t COMPSYS_CURRENT_GENERATION = 2;
constexpr std::int32_t COMPSYS_REPLICA_COUNT = 3;
constexpr std::int32_t COMPSYS_INPUT_RECORDS = 100;
constexpr std::int32_t COMPSYS_CHECKPOINT_INTERVAL = 25;
constexpr std::int32_t COMPSYS_FAILED_AFTER_RECORD = 63;

}  // namespace

int main() {
    try {
        const std::array<std::int32_t, 3> records{4, -1, 7};
        const std::array<std::int32_t, 4> branch_values{1, 9, 2, 10};
        const std::array<std::uint64_t, 5> offsets{0, 64, 4096, 8192, 0};
        const std::array<std::int32_t, 3> worker_nodes{0, 1, 1};
        const std::array<std::int32_t, 3> memory_nodes{0, 0, 1};
        const std::array<std::int32_t, 4> kernel_values{1, 2, 3, 4};
        const std::array<std::int32_t, 4> task_costs{5, 1, 4, 2};
        const std::array<bool, 3> ready_observations{false, false, true};
        const std::array<std::int32_t, 4> observed_versions{1, 2, 2, 3};
        const std::array<std::int32_t, 6> ring_operations{1, 1, 1, -1, -1, -1};
        const std::array<std::int32_t, 3> retired_nodes{1, 2, 3};
        const std::array<std::int32_t, 1> protected_nodes{2};
        const std::array<std::int32_t, 3> scan_values{1, 2, 3};
        const std::array<std::int32_t, 3> worker_queues{6, 0, 0};
        const std::array<std::int32_t, 4> task_ids{1, 1, 2, 3};
        const std::array<std::int32_t, 4> generations{2, 2, 1, 2};
        const std::array<bool, 3> acknowledgements{true, true, false};

        const compsys::Chapter01PipelineReport ch01 =
            compsys::model_ch01_input_pipeline(records);
        const compsys::Chapter02BranchReport ch02 =
            compsys::model_ch02_branch_predictor(branch_values,
                                                 COMPSYS_BRANCH_THRESHOLD);
        const compsys::Chapter03MemoryAccessReport ch03 =
            compsys::model_ch03_cache_tlb(offsets,
                                          COMPSYS_CACHE_LINE_BYTES,
                                          COMPSYS_PAGE_BYTES,
                                          COMPSYS_TLB_ENTRIES);
        const compsys::Chapter04NumaReport ch04 =
            compsys::model_ch04_numa_coherence(worker_nodes, memory_nodes);
        const compsys::Chapter05RooflineReport ch05 =
            compsys::model_ch05_roofline(COMPSYS_ROOFLINE_FLOPS,
                                         COMPSYS_ROOFLINE_BYTES,
                                         COMPSYS_ROOFLINE_PEAK_GFLOPS,
                                         COMPSYS_ROOFLINE_BANDWIDTH_GBPS);
        const compsys::Chapter05bPmuSimdReport ch05b =
            compsys::model_ch05b_pmu_simd_evidence(80, 20, true);
        const compsys::Chapter06LayoutReport ch06 =
            compsys::model_ch06_layout_locality(COMPSYS_SIMD_ELEMENTS,
                                                COMPSYS_LAYOUT_FIELDS,
                                                COMPSYS_LAYOUT_HOT_FIELDS,
                                                COMPSYS_FIELD_BYTES);
        const compsys::Chapter07SimdReport ch07 =
            compsys::model_ch07_simd_ilp(COMPSYS_SIMD_ELEMENTS,
                                         COMPSYS_SIMD_LANES);
        const compsys::Chapter08KernelReport ch08 =
            compsys::model_ch08_map_filter_reduce(kernel_values);
        const compsys::Chapter09SchedulingReport ch09 =
            compsys::model_ch09_thread_scheduling(task_costs,
                                                  COMPSYS_SCHEDULING_WORKERS);
        const compsys::Chapter09bLinuxWaitReport ch09b =
            compsys::model_ch09b_linux_wait_wake(ready_observations);
        const compsys::Chapter10BackpressureReport ch10 =
            compsys::model_ch10_sync_backpressure(
                COMPSYS_BACKPRESSURE_TICKS,
                COMPSYS_BACKPRESSURE_PRODUCE_RATE,
                COMPSYS_BACKPRESSURE_CONSUME_RATE,
                COMPSYS_BACKPRESSURE_CAPACITY);
        const compsys::Chapter11AtomicPublicationReport ch11 =
            compsys::model_ch11_atomic_publication(observed_versions,
                                                   COMPSYS_FINAL_VERSION);
        const compsys::Chapter12LockFreeQueueReport ch12 =
            compsys::model_ch12_spsc_ring(ring_operations, COMPSYS_RING_CAPACITY);
        const compsys::Chapter12bReclamationReport ch12b =
            compsys::model_ch12b_epoch_reclamation(retired_nodes,
                                                   protected_nodes);
        const compsys::Chapter13ReduceScanPartitionReport ch13 =
            compsys::model_ch13_reduce_scan_partition(scan_values,
                                                      COMPSYS_PARTITION_THRESHOLD);
        const compsys::Chapter14WorkStealingReport ch14 =
            compsys::model_ch14_work_stealing(worker_queues);
        const compsys::Chapter15AsyncIoReport ch15 =
            compsys::model_ch15_async_io_backpressure(
                COMPSYS_IO_REQUESTS,
                COMPSYS_IO_QUEUE_DEPTH,
                COMPSYS_IO_COMPLETIONS_PER_TICK);
        const compsys::Chapter16DistributedModelReport ch16 =
            compsys::model_ch16_distributed_attempts(task_ids,
                                                     generations,
                                                     COMPSYS_CURRENT_GENERATION);
        const compsys::Chapter17ReplicationReport ch17 =
            compsys::model_ch17_quorum_commit(COMPSYS_REPLICA_COUNT,
                                              acknowledgements);
        const compsys::Chapter18RecoveryReport ch18 =
            compsys::model_ch18_checkpoint_recovery(
                COMPSYS_INPUT_RECORDS,
                COMPSYS_CHECKPOINT_INTERVAL,
                COMPSYS_FAILED_AFTER_RECORD);

        std::cout << "chapter_models," << COMPSYS_CHAPTER_MODEL_COUNT << '\n';
        std::cout << "ch01_pipeline," << ch01.records_total << ','
                  << ch01.accepted << ',' << ch01.rejected << '\n';
        std::cout << "ch02_branch," << ch02.actual_taken << ','
                  << ch02.mispredictions << '\n';
        std::cout << "ch03_cache_tlb," << ch03.unique_cache_lines << ','
                  << ch03.unique_pages << ',' << ch03.tlb_misses << '\n';
        std::cout << "ch04_numa," << ch04.local_accesses << ','
                  << ch04.remote_accesses << ',' << ch04.coherence_transfers << '\n';
        std::cout << "ch05_roofline," << ch05.arithmetic_intensity << ','
                  << ch05.attainable_gflops << ',' << (ch05.memory_bound ? 1 : 0)
                  << '\n';
        std::cout << "ch05b_pmu_simd," << ch05b.retired_instructions << ','
                  << ch05b.vector_fraction << ','
                  << (ch05b.evidence_complete ? 1 : 0) << '\n';
        std::cout << "ch06_layout," << ch06.aos_bytes_touched << ','
                  << ch06.soa_bytes_touched << ',' << ch06.saved_bytes << '\n';
        std::cout << "ch07_simd," << ch07.vector_iterations << ','
                  << ch07.scalar_tail << '\n';
        std::cout << "ch08_kernel," << ch08.filtered_count << ','
                  << ch08.reduced_sum << '\n';
        std::cout << "ch09_scheduling," << ch09.task_count << ','
                  << ch09.imbalance << '\n';
        std::cout << "ch09b_wait," << ch09b.spurious_wakeups << ','
                  << (ch09b.completed ? 1 : 0) << '\n';
        std::cout << "ch10_backpressure," << ch10.produced << ','
                  << ch10.backpressure_events << ',' << ch10.max_depth << '\n';
        std::cout << "ch11_atomic," << ch11.stale_reads << ','
                  << (ch11.monotonic_observations ? 1 : 0) << '\n';
        std::cout << "ch12_ring," << ch12.pushed << ',' << ch12.popped << ','
                  << ch12.rejected_full << ',' << ch12.empty_pops << '\n';
        std::cout << "ch12b_reclamation," << ch12b.reclaimed_nodes << ','
                  << ch12b.deferred_nodes << '\n';
        std::cout << "ch13_reduce_scan_partition," << ch13.reduce_sum << ','
                  << ch13.last_prefix << ',' << ch13.partition_true_count << '\n';
        std::cout << "ch14_work_stealing," << ch14.stolen_tasks << ','
                  << ch14.completed_tasks << '\n';
        std::cout << "ch15_async_io," << ch15.submitted << ','
                  << ch15.completed << ',' << ch15.backpressure_events << '\n';
        std::cout << "ch16_distributed," << ch16.logical_tasks << ','
                  << ch16.duplicate_attempts << ','
                  << ch16.stale_generation_attempts << '\n';
        std::cout << "ch17_quorum," << ch17.quorum_size << ','
                  << (ch17.committed ? 1 : 0) << '\n';
        std::cout << "ch18_recovery," << ch18.checkpoints << ','
                  << ch18.replay_from_record << ',' << ch18.replay_records << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "compsys chapter models probe failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
