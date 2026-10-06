#pragma once
#include "CircuitDAG.hpp"
#include "ThreadPool.hpp"
#include <vector>
#include <future>
#include <algorithm>

class ParallelEngine {
public:
    // 1. Chạy Đơn luồng (Sequential)
    static void simulate_sequential(CircuitDAG& dag) {
        for (const auto& level : dag.levels) {
            for (uint32_t gate_id : level) {
                evaluate_gate(dag, gate_id);
            }
        }
    }

    // 2. Chạy Đa luồng (Parallel) tích hợp Static Chunking
    static void simulate_parallel(CircuitDAG& dag, ThreadPool& pool, size_t num_threads) {
        for (const auto& level : dag.levels) {
            if (level.empty()) continue;

            size_t total_gates = level.size();

            // Ngưỡng Granularity: Nếu Level quá ít cổng (< 10000), chạy đơn luồng
            if (total_gates < 10000) {
                for (uint32_t gate_id : level) {
                    evaluate_gate(dag, gate_id);
                }
                continue;
            }

            size_t chunk_size = (total_gates + num_threads - 1) / num_threads;
            std::vector<std::future<void>> futures;
            futures.reserve(num_threads);

            for (size_t i = 0; i < total_gates; i += chunk_size) {
                size_t start = i;
                size_t end = std::min(i + chunk_size, total_gates);

                futures.push_back(pool.enqueue([&dag, &level, start, end]() {
                    for (size_t j = start; j < end; ++j) {
                        evaluate_gate(dag, level[j]);
                    }
                }));
            }

            // BARRIER BARRIER: Đồng bộ hoàn toàn tầng này trước khi sang tầng kế tiếp
            for (auto& f : futures) {
                f.get();
            }
        }
    }

private:
    static inline void evaluate_gate(CircuitDAG& dag, uint32_t gate_id) {
        GateType type = dag.types[gate_id];
        if (type == GateType::INPUT) return;

        // Xử lý cổng 1 đầu vào (NOT)
        if (type == GateType::NOT) {
            uint32_t in1 = dag.inputs[gate_id].in1;
            dag.values[gate_id] = ~dag.values[in1];
            return;
        }

        // Xử lý cổng 1 đầu BUFF
        if (type == GateType::BUFF) {
            uint32_t in1 = dag.inputs[gate_id].in1;
            dag.values[gate_id] = dag.values[in1];
            return;
        }

        // Xử lý các cổng 2 đầu vào (AND, OR, XOR, NAND, NOR)
        uint32_t in1 = dag.inputs[gate_id].in1;
        uint32_t in2 = dag.inputs[gate_id].in2;
        uint64_t val1 = dag.values[in1];
        uint64_t val2 = dag.values[in2];

        switch (type) {
            case GateType::AND:  dag.values[gate_id] = val1 & val2; break;
            case GateType::OR:   dag.values[gate_id] = val1 | val2; break;
            case GateType::XOR:  dag.values[gate_id] = val1 ^ val2; break;
            case GateType::NAND: dag.values[gate_id] = ~(val1 & val2); break; 
            case GateType::NOR:  dag.values[gate_id] = ~(val1 | val2); break; 
            default: break;
        }
    }
};