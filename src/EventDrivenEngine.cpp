#include "EventDrivenEngine.hpp"
#include <chrono>
#include <algorithm>

// Hàm helper tính toán logic Bitwise 64-bit cho 1 cổng
static inline uint64_t compute_gate_64bit(GateType type, uint64_t v1, uint64_t v2 = 0ULL) {
    switch (type) {
        case GateType::NOT:  return ~v1;
        case GateType::BUFF: return v1;
        case GateType::AND:  return v1 & v2;
        case GateType::OR:   return v1 | v2;
        case GateType::XOR:  return v1 ^ v2;
        case GateType::NAND: return ~(v1 & v2);
        case GateType::NOR:  return ~(v1 | v2);
        default: return 0ULL;
    }
}

double EventDrivenEngine::simulate_sequential(CircuitDAG& dag, const std::vector<uint64_t>& new_inputs) {
    auto start_time = std::chrono::high_resolution_clock::now();

    size_t num_gates = dag.size();
    
    // mảng đánh dấu cờ Active cho Tầng hiện tại và Tầng kế tiếp
    std::vector<uint8_t> active(num_gates, 0);
    std::vector<uint8_t> next_active(num_gates, 0);

    // -------------------------------------------------------------
    // BƯỚC 1: XỬ LÝ TẦNG 0 (CÁC CỔNG INPUT)
    // -------------------------------------------------------------
    size_t input_idx = 0;
    for (uint32_t gate_id : dag.levels[0]) {
        if (input_idx < new_inputs.size()) {
            uint64_t old_val = dag.values[gate_id];
            uint64_t val = new_inputs[input_idx++];
            dag.values[gate_id] = val;

            // Nếu Input bị thay đổi -> Kích hoạt (Active) các cổng nhận tín hiệu ở Tầng sau (adj)
            if ((old_val ^ val) != 0ULL) {
                for (uint32_t successor : dag.adj[gate_id]) {
                    active[successor] = 1;
                }
            }
        }
    }

    // -------------------------------------------------------------
    // BƯỚC 2: QUAN SÁT & LAN TRUYỀN SỰ KIỆN QUA CÁC TẦNG Tiếp theo
    // -------------------------------------------------------------
    for (size_t lvl = 1; lvl < dag.levels.size(); ++lvl) {
        const auto& level_gates = dag.levels[lvl];
        std::fill(next_active.begin(), next_active.end(), 0);

        for (uint32_t gate_id : level_gates) {
            // BỎ QUA CỔNG NẾU KHÔNG CÓ CỜ ACTIVE (TRUY VẾT CHỌN LỌC - CỐT LÕI MỐC 4)
            if (!active[gate_id]) continue;

            // 1. Lấy tín hiệu đầu vào (Hướng ngược - inputs)
            uint32_t in1 = dag.inputs[gate_id].in1;
            uint64_t v1 = dag.values[in1];
            uint64_t new_val = 0ULL;

            if (dag.types[gate_id] == GateType::NOT || dag.types[gate_id] == GateType::BUFF) {
                new_val = compute_gate_64bit(dag.types[gate_id], v1);
            } else {
                uint32_t in2 = dag.inputs[gate_id].in2;
                uint64_t v2 = dag.values[in2];
                new_val = compute_gate_64bit(dag.types[gate_id], v1, v2);
            }

            // 2. So sánh với giá trị cũ
            uint64_t old_val = dag.values[gate_id];
            if ((old_val ^ new_val) != 0ULL) {
                dag.values[gate_id] = new_val;
                
                // 3. Nếu Output biến đổi -> Bật cờ Active cho các cổng thuộc tầng tiếp theo (adj)
                for (uint32_t successor : dag.adj[gate_id]) {
                    next_active[successor] = 1;
                }
            }
        }
        active = next_active; // Chuyển danh sách Active sang Tầng kế tiếp
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end_time - start_time).count();
}

double EventDrivenEngine::simulate_parallel(CircuitDAG& dag, ThreadPool& pool, size_t num_threads, const std::vector<uint64_t>& new_inputs) {
    auto start_time = std::chrono::high_resolution_clock::now();

    size_t num_gates = dag.size();
    std::vector<uint8_t> active(num_gates, 0);
    std::vector<uint8_t> next_active(num_gates, 0);

    // Tầng 0: Khởi tạo Input
    size_t input_idx = 0;
    for (uint32_t gate_id : dag.levels[0]) {
        if (input_idx < new_inputs.size()) {
            uint64_t old_val = dag.values[gate_id];
            uint64_t val = new_inputs[input_idx++];
            dag.values[gate_id] = val;

            if ((old_val ^ val) != 0ULL) {
                for (uint32_t successor : dag.adj[gate_id]) {
                    active[successor] = 1;
                }
            }
        }
    }

    // Lan truyền đa luồng
    for (size_t lvl = 1; lvl < dag.levels.size(); ++lvl) {
        const auto& level_gates = dag.levels[lvl];
        size_t level_size = level_gates.size();
        std::fill(next_active.begin(), next_active.end(), 0);

        if (level_size < 10000 || num_threads <= 1) {
            for (uint32_t gate_id : level_gates) {
                if (!active[gate_id]) continue;

                uint32_t in1 = dag.inputs[gate_id].in1;
                uint64_t v1 = dag.values[in1];
                uint64_t new_val = 0ULL;

                if (dag.types[gate_id] == GateType::NOT || dag.types[gate_id] == GateType::BUFF) {
                    new_val = compute_gate_64bit(dag.types[gate_id], v1);
                } else {
                    uint32_t in2 = dag.inputs[gate_id].in2;
                    uint64_t v2 = dag.values[in2];
                    new_val = compute_gate_64bit(dag.types[gate_id], v1, v2);
                }

                uint64_t old_val = dag.values[gate_id];
                if ((old_val ^ new_val) != 0ULL) {
                    dag.values[gate_id] = new_val;
                    for (uint32_t successor : dag.adj[gate_id]) {
                        next_active[successor] = 1;
                    }
                }
            }
        } else {
            std::vector<std::future<void>> futures;
            size_t chunk_size = (level_size + num_threads - 1) / num_threads;

            for (size_t t = 0; t < num_threads; ++t) {
                size_t start_idx = t * chunk_size;
                size_t end_idx = std::min(start_idx + chunk_size, level_size);

                if (start_idx >= end_idx) break;

                futures.push_back(pool.enqueue([&dag, &level_gates, &active, &next_active, start_idx, end_idx]() {
                    for (size_t i = start_idx; i < end_idx; ++i) {
                        uint32_t gate_id = level_gates[i];
                        if (!active[gate_id]) continue;

                        uint32_t in1 = dag.inputs[gate_id].in1;
                        uint64_t v1 = dag.values[in1];
                        uint64_t new_val = 0ULL;

                        if (dag.types[gate_id] == GateType::NOT || dag.types[gate_id] == GateType::BUFF) {
                            new_val = compute_gate_64bit(dag.types[gate_id], v1);
                        } else {
                            uint32_t in2 = dag.inputs[gate_id].in2;
                            uint64_t v2 = dag.values[in2];
                            new_val = compute_gate_64bit(dag.types[gate_id], v1, v2);
                        }

                        uint64_t old_val = dag.values[gate_id];
                        if ((old_val ^ new_val) != 0ULL) {
                            dag.values[gate_id] = new_val;
                            for (uint32_t successor : dag.adj[gate_id]) {
                                next_active[successor] = 1;
                            }
                        }
                    }
                }));
            }

            for (auto& fut : futures) {
                fut.get();
            }
        }
        active = next_active;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end_time - start_time).count();
}