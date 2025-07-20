#ifndef JSON_CONFIG_H
#define JSON_CONFIG_H

#include <string>
#include <fstream>
#include <iostream>
#include <map>

// Simple JSON parser for basic key-value pairs
class SimpleJsonConfig {
public:
    SimpleJsonConfig(const std::string& filename) {
        load_config(filename);
    }
    
    bool get_bool(const std::string& key, bool default_value = false) const {
        auto it = config_map.find(key);
        if (it != config_map.end()) {
            return it->second == "true";
        }
        return default_value;
    }
    
    int get_int(const std::string& key, int default_value = 0) const {
        auto it = config_map.find(key);
        if (it != config_map.end()) {
            return std::stoi(it->second);
        }
        return default_value;
    }
    
    std::string get_string(const std::string& key, const std::string& default_value = "") const {
        auto it = config_map.find(key);
        if (it != config_map.end()) {
            // Remove quotes if present
            std::string value = it->second;
            if (value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.length() - 2);
            }
            return value;
        }
        return default_value;
    }

private:
    std::map<std::string, std::string> config_map;
    
    void load_config(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Warning: Could not open config file: " << filename << std::endl;
            return;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            // Skip empty lines and comments
            if (line.empty() || line[0] == '/' || line[0] == '#') continue;
            
            // Find key-value pairs in format "key": value
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                
                // Clean up key and value (remove spaces, quotes, commas)
                key = trim(key);
                value = trim(value);
                if (key.front() == '"' && key.back() == '"') {
                    key = key.substr(1, key.length() - 2);
                }
                if (value.back() == ',') {
                    value = value.substr(0, value.length() - 1);
                }
                
                config_map[key] = value;
            }
        }
    }
    
    std::string trim(const std::string& str) const {
        size_t start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }
};

#endif