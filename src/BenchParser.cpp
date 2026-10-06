#include "BenchParser.hpp" 
#include <fstream> 
#include <sstream> 
#include <iostream>
#include <algorithm>


// Remove all the white spaces at the beginning and the end of the string
std::string BenchParser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Exchange a gate in string to a legal type
GateType BenchParser::legalize_gate(const std::string& str) {
    if (str == "NOT") return GateType::NOT;
    if (str == "AND") return GateType::AND;
    if (str == "OR")  return GateType::OR;
    if (str == "XOR") return GateType::XOR;
    if (str == "NAND") return GateType::NAND;
    if (str == "NOR") return GateType::NOR;
    if (str == "BUFF" || str == "BUF") return GateType::BUFF;
    return GateType::BUFF; 
}

// Returns the intermediate gate type for the N-input gate decomposition process
static GateType get_base_type(GateType type) {
    if (type == GateType::NAND) return GateType::AND;
    if (type == GateType::NOR)  return GateType::OR;
    return type;
}

// Read the .bench file and turn it into a Directed Acyclic Graph and return True if it is done
bool BenchParser::parse(const std::string& filepath, CircuitDAG& out_dag) {
    std::ifstream inFile(filepath);

    if (!inFile.is_open()) {
        std::cout << "Cannot open the file: " << filepath << std::endl;
        return false;
    }

    std::string line;
    std::unordered_map<std::string, uint32_t> name_to_id;

    // ==========================================
    // PASS 1: READ FILE AND GATE'S SIZE
    // ==========================================
    while (std::getline(inFile, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (line.find("INPUT(") == 0) {
            size_t open_p = line.find('(');
            size_t close_p = line.find(')');
            std::string gate_name = trim(line.substr(open_p + 1, close_p - open_p - 1));
            
            uint32_t id = out_dag.add_gate(GateType::INPUT);
            name_to_id[gate_name] = id;
        }
        else if (line.find('=') != std::string::npos) {
            size_t equal_pos = line.find('=');
            std::string des = trim(line.substr(0, equal_pos));
            std::string rest = trim(line.substr(equal_pos + 1));

            size_t open_p = rest.find('(');
            size_t close_p = rest.find(')');

            std::string gate_str = trim(rest.substr(0, open_p));
            GateType gate_type = legalize_gate(gate_str);

            std::string input_gates = rest.substr(open_p + 1, close_p - open_p - 1);
            std::stringstream ss(input_gates);
            std::string in_name;
            std::vector<std::string> inputs_list;
            while (std::getline(ss, in_name, ',')) {
                inputs_list.push_back(trim(in_name));
            }

            if (inputs_list.size() > 2) {
                GateType base_type = get_base_type(gate_type);
                std::string prev_temp_name = "";

                for (size_t i = 0; i < inputs_list.size() - 2; ++i) {
                    std::string temp_name = des + "_sub_" + std::to_string(i);
                    uint32_t sub_id = out_dag.add_gate(base_type);
                    name_to_id[temp_name] = sub_id;
                }
            }

            uint32_t id = out_dag.add_gate(gate_type);
            name_to_id[des] = id;
        }
    }

    // ==========================================
    // PASS 2: COME BACK TO THE HEAD OF THE FILE AND WIRE THE GATES
    // ==========================================
    inFile.clear();
    inFile.seekg(0);

    while (std::getline(inFile, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (line.find('=') != std::string::npos) {
            size_t equal_pos = line.find('=');
            std::string des = trim(line.substr(0, equal_pos));
            std::string rest = trim(line.substr(equal_pos + 1));

            size_t open_p = rest.find('(');
            size_t close_p = rest.find(')');

            std::string input_gates = rest.substr(open_p + 1, close_p - open_p - 1);
            std::stringstream ss(input_gates);
            std::string in_name;
            std::vector<std::string> inputs_list;
            while (std::getline(ss, in_name, ',')) {
                inputs_list.push_back(trim(in_name));
            }

            if (inputs_list.size() <= 2) {
                for (const auto& in_g : inputs_list) {
                    out_dag.add_wire(name_to_id[in_g], name_to_id[des]);
                }
            } else {
                std::string prev_name = des + "_sub_0";
                out_dag.add_wire(name_to_id[inputs_list[0]], name_to_id[prev_name]);
                out_dag.add_wire(name_to_id[inputs_list[1]], name_to_id[prev_name]);

                for (size_t i = 2; i < inputs_list.size() - 1; ++i) {
                    std::string curr_name = des + "_sub_" + std::to_string(i - 1);
                    out_dag.add_wire(name_to_id[prev_name], name_to_id[curr_name]);
                    out_dag.add_wire(name_to_id[inputs_list[i]], name_to_id[curr_name]);
                    prev_name = curr_name;
                }

                out_dag.add_wire(name_to_id[prev_name], name_to_id[des]);
                out_dag.add_wire(name_to_id[inputs_list.back()], name_to_id[des]);
            }
        }
    }

    out_dag.levelize();

    out_dag.reindex_by_level();

    return true;
}

