#pragma once
#include "CircuitDAG.hpp"
#include <string>
#include <unordered_map>
#include <vector>

class BenchParser {
public:
    // Read the .bench file and turn it into a Directed Acyclic Graph and return True if it is done
    static bool parse(const std::string& filepath, CircuitDAG& out_dag);
private:
    // Remove all the white spaces at the beginning and the end of the string
    static std::string trim(const std::string& str);
    
    // Exchange a gate in string to a legal type
    static GateType legalize_gate(const std::string& str);
};

