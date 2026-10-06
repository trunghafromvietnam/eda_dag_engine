#include "CircuitDAG.hpp"
#include <queue>
#include <utility> // Cung cấp std::move
#include <random> // Sinh số ngẫu nhiên
#include <algorithm>

// Xác định hàm phân tầng từ class CircuitDAG
const std::vector<std::vector<uint32_t>>& CircuitDAG::levelize() {
    // Xóa dữ liệu cũ nếu đã từng phân tầng
    levels.clear();

    // Tạo một bản sao của in_degrees vì thuật toán sẽ giảm dần in_degrees
    std::vector<uint32_t> temp_in_degrees = in_degrees;
    std::vector<uint32_t> current_level;

    // Tầng 0, tầng đầu tiên, chạy qua rồi coi ai = 0 thì bỏ vào trước
    for (uint32_t i = 0; i < size(); ++i) {
        if (temp_in_degrees[i] == 0) {
            current_level.push_back(i);
        }
    }

    // Vòng lặp phân tầng theo chiều rộng (BFS-like)
    while (!current_level.empty()) {
        // Lưu tầng hiện tại lại
        levels.push_back(current_level);
        std::vector<uint32_t> next_level;

        for (uint32_t gate_id : current_level) {
            // Xóa các cạnh đi ra từ cổng hiện tại
            for (uint32_t child_id : adj[gate_id]) {
                temp_in_degrees[child_id]--;
                if (temp_in_degrees[child_id] == 0) {
                    next_level.push_back(child_id);
                }
            }
        }

        // Chuyển giao quyền sở hữu cho current_level rồi nó lại tiếp tục ko trống
        current_level = std::move(next_level);
    }

    return levels;
}

void CircuitDAG::reindex_by_level() {
    if (levels.empty()) return;
    size_t total_gates = types.size();

    // Mảng ánh xạ từ ID cũ -> ID mới
    std::vector<uint32_t> old_to_new(total_gates);
    uint32_t new_id_counter = 0;

    for (const auto& level : levels) {
        for (uint32_t old_id : level) {
            old_to_new[old_id] = new_id_counter++;
        }
    }

    // Tạo các mảng dữ liệu mới sắp xếp liên tiếp theo ID mới
    std::vector<GateType> new_types(total_gates);
    std::vector<uint64_t> new_values(total_gates, 0ULL);
    std::vector<GateInputPair> new_inputs(total_gates);
    std::vector<std::vector<uint32_t>> new_levels(levels.size());

    for (size_t lvl_idx = 0; lvl_idx < levels.size(); ++lvl_idx) {
        for (uint32_t old_id : levels[lvl_idx]) {
            uint32_t nid = old_to_new[old_id];
            new_types[nid] = types[old_id];
            new_values[nid] = values[old_id];

            if (types[old_id] != GateType::INPUT) {
                new_inputs[nid].in1 = old_to_new[inputs[old_id].in1];
                new_inputs[nid].in2 = old_to_new[inputs[old_id].in2];
            }
            new_levels[lvl_idx].push_back(nid);
        }
    }

    // Thay thế toàn bộ dữ liệu cũ bằng dữ liệu đã tối ưu vị trí bộ nhớ
    types = std::move(new_types);
    values = std::move(new_values);
    inputs = std::move(new_inputs);
    levels = std::move(new_levels);
}

CircuitDAG CircuitDAG::generate_random_circuit(size_t total_gates, size_t num_inputs) {
    CircuitDAG dag;

    if (total_gates <= num_inputs) return dag;
    
    // std::mt19937: Thuật toán sinh số ngẫu nhiên Mersenne Twister với seed cố định là 42 (để kết quả có thể tái lập)
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint8_t> bit_dist(0, 1); // Phân phối đều 0 hoặc 1

    // Sinh các cổng INPUT đầu tiên
    for (size_t i = 0; i < num_inputs; ++i) {
        uint32_t id = dag.add_gate(GateType::INPUT);
        dag.values[id] = rng(); 
    }

    GateType non_input_types[] = {
        GateType::AND, GateType::OR, GateType::XOR, GateType::NOT, GateType::NAND, GateType::NOR, GateType::BUFF
    };
    std::uniform_int_distribution<size_t> type_dist(0, 6);

    // Sinh các cổng logic còn lại
    for (size_t i = num_inputs; i < total_gates; ++i) {
        GateType gtype = non_input_types[type_dist(rng)];
        uint32_t current_id = dag.add_gate(gtype);

        // QUY TẮC ĐẢM BẢO KHÔNG CÓ CHU TRÌNH (DAG):
        // Cổng hiện tại (current_id) CHỈ ĐƯỢC LẤY đầu vào từ các cổng đã sinh trước đó (ID từ 0 đến current_id - 1)
        std::uniform_int_distribution<uint32_t> parent_dist(0, current_id - 1);

        if (gtype == GateType::NOT) { 
            uint32_t parent = parent_dist(rng);
            dag.add_wire(parent, current_id); // Cổng NOT chỉ có 1 đầu vào
        } else {
            uint32_t p1 = parent_dist(rng);
            uint32_t p2 = parent_dist(rng);

            // Đảm bảo p2 khác p1 để không tự nối 2 đầu vào của cổng vào cùng 1 nguồn
            while (p2 == p1 && current_id > 1) {
                p2 = parent_dist(rng);
            }
            dag.add_wire(p1, current_id);
            dag.add_wire(p2, current_id);
        }
    }

    return dag;
}