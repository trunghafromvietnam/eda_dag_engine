#pragma once // Đảm bảo file header này chỉ được biên dịch đúng 1 lần trong quá trình biên dịch
#include <vector>
#include <cstdint> // Cung cấp các kiểu dữ liệu có fixed size như uint8_t và uint32_t
#include <cstddef> // Cung cấp kiểu size_t

// Enum: Định nghĩa tập hợp các loại cổng logic
// unit8_t chỉ định enum này chỉ tốn 1 byte (8 bits) thay vì 4 bytes (32 bits) như default
enum class GateType : uint8_t {
    INPUT,
    AND,
    OR,
    XOR,
    NOT,
    NAND,
    NOR, 
    BUFF
};

// Flat Array 
struct GateInputPair {
    uint32_t in1{0};
    uint32_t in2{0};
};

class CircuitDAG {
public:
    // SoA Pattern: Tách các thuộc tính ra riêng thành mảng riêng biệt để tăng Cache Locality
    std::vector<GateType> types;            // Lưu loại của từng cổng
    std::vector<uint64_t> values;            // Lưu giá trị logic output hiện tại (0 hoặc 1)
    std::vector<uint32_t> in_degrees;       // Bán bậc vào (số dây tín hiệu đi vào cổng)
    std::vector<std::vector<uint32_t>> adj; // Danh sách kề đầu ra (Cổng này cấp tín hiệu cho những cổng nào)
    std::vector<GateInputPair> inputs; // Danh sách kề đầu vào (Cổng này nhận tín hiệu từ những cổng nào)
    std::vector<std::vector<uint32_t>> levels; // Kết quả phân tầng: levels[i] chứa danh sách ID cổng ở tầng i

    // Hàm thêm 1 cổng mới vào đồ thị
    uint32_t add_gate(GateType type) {
        // ID của 1 cổng chính là chỉ số tiếp theo trong vector
        uint32_t id = static_cast<uint32_t>(types.size());
        types.push_back(type);
        values.push_back(0ULL); // Khởi tạo giá trị ban đầu là 0
        in_degrees.push_back(0); // Khởi tạo in-degree ban đầu là 0
        adj.push_back({}); // Tạo vector rỗng cho danh sách đầu ra
        inputs.push_back({0, 0}); // Tạo vector rỗng cho danh sách đầu vào
        return id;
    }

    // Hàm phát tín hiệu từ cổng from tới cổng to
    void add_wire(uint32_t from, uint32_t to) {
        adj[from].push_back(to);    
        
        if (in_degrees[to] == 0) {
            inputs[to].in1 = from;
        } else {
            inputs[to].in2 = from;
        }
        
        in_degrees[to]++;
    }

    // Trả về tổng số lượng cổng hiện tại, const cam kết không thay đổi dữ liệu nào
    size_t size() const { return types.size(); }

    // Declare hàm phân tầng Kahn, trả về tham chiếu hàm số (const reference) để chỉ đọc và tránh copy
    const std::vector<std::vector<uint32_t>>& levelize();

    // Hàm đánh lại ID theo thứ tự phân tầng để tối ưu Cache
    void reindex_by_level();

    // Static function: Hàm tiện ích sinh mạch ngẫu nhiên, có thể gọi trực tiếp mà không cần tạo đối tượng trước
    static CircuitDAG generate_random_circuit(size_t total_gates, size_t num_inputs);
};