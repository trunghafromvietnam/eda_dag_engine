#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <random>
#include "CircuitDAG.hpp"
#include "BenchParser.hpp"
#include "ThreadPool.hpp"
#include "ParallelEngine.hpp"
#include "EventDrivenEngine.hpp"

int main(int argc, char* argv[]) {
    CircuitDAG dag;
    size_t num_threads = std::thread::hardware_concurrency();
    std::mt19937_64 rng(42);

    if (argc > 1) {
        std::string bench_path = argv[1];
        std::cout << "=== PARSING BENCHMARK FILE: " << bench_path << " ===" << std::endl;
        if (!BenchParser::parse(bench_path, dag)) {
            std::cerr << "Loi nap file benchmark!" << std::endl;
            return 1;
        }
    } else {
        std::cout << "=== MACH THU NGHIEM TU SINH (2M Gates) ===" << std::endl;
        dag = CircuitDAG::generate_random_circuit(2000000, 20000);
        dag.levelize();
        dag.reindex_by_level();
    }

    std::cout << "Tong so cong: " << dag.size() << " | So tang: " << dag.levels.size() << std::endl;
    ThreadPool pool(num_threads);

    // 1. CHU KỲ 1 (Giai đoạn Nền): Mô phỏng trạng thái ban đầu của toàn bộ mạch
    for (size_t i = 0; i < dag.levels[0].size(); ++i) {
        uint32_t gate_id = dag.levels[0][i];
        dag.values[gate_id] = rng();
    }
    ParallelEngine::simulate_sequential(dag);

    // 2. CHU KỲ 2: Tạo kịch bản thực tế -> Chỉ 5% cổng Input thay đổi tín hiệu
    std::vector<uint64_t> new_inputs;
    for (uint32_t gate_id : dag.levels[0]) {
        uint64_t current_val = dag.values[gate_id];
        if ((rng() % 100) < 5) { // 5% cơ hội đổi bit
            new_inputs.push_back(rng());
        } else {
            new_inputs.push_back(current_val);
        }
    }

    // A. ĐO MỐC 3 (Level-based: Tính mù mịt 100% cổng)
    CircuitDAG dag_m3 = dag;
    auto start_m3 = std::chrono::high_resolution_clock::now();
    size_t in_idx = 0;
    for (uint32_t g_id : dag_m3.levels[0]) dag_m3.values[g_id] = new_inputs[in_idx++];
    ParallelEngine::simulate_sequential(dag_m3);
    auto end_m3 = std::chrono::high_resolution_clock::now();
    double time_m3 = std::chrono::duration<double, std::milli>(end_m3 - start_m3).count();

    // B. ĐO MỐC 4 (Event-Driven: Chỉ tính các cổng Active)
    CircuitDAG dag_m4 = dag;
    double time_m4 = EventDrivenEngine::simulate_sequential(dag_m4, new_inputs);

    std::cout << "\n=== KET QUA SO SANH HIEU NANG ===" << std::endl;
    std::cout << "MOC 3 (Level-Based Time) : " << time_m3 << " ms" << std::endl;
    std::cout << "MOC 4 (Event-Driven Time): " << time_m4 << " ms" << std::endl;
    std::cout << "Toc do gia tang (Speedup)  : " << (time_m3 / time_m4) << "x" << std::endl;

    return 0;
}