#pragma once    
#include "CircuitDAG.hpp"
#include "ThreadPool.hpp"
#include <vector>

class EventDrivenEngine {
public:
    static double simulate_sequential(CircuitDAG& dag, const std::vector<uint64_t>& new_inputs);
    static double simulate_parallel(CircuitDAG& dag, ThreadPool& pool, size_t num_threads, const std::vector<uint64_t>& new_inputs);
};