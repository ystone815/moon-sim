#ifndef ERROR_HANDLING_H
#define ERROR_HANDLING_H

#include <systemc.h>
#include <string>

// Centralized error handling utilities for consistent error reporting

namespace soc_sim {
    namespace error {
        
        // Error severity levels
        enum class Severity {
            INFO,
            WARNING,
            ERROR,
            FATAL
        };
        
        // Standardized error reporting function
        inline void report(Severity severity, const std::string& module_name, 
                          const std::string& error_code, const std::string& message) {
            std::string full_message = "[" + error_code + "] " + message;
            
            switch (severity) {
                case Severity::INFO:
                    SC_REPORT_INFO(module_name.c_str(), full_message.c_str());
                    break;
                case Severity::WARNING:
                    SC_REPORT_WARNING(module_name.c_str(), full_message.c_str());
                    break;
                case Severity::ERROR:
                    SC_REPORT_ERROR(module_name.c_str(), full_message.c_str());
                    break;
                case Severity::FATAL:
                    SC_REPORT_FATAL(module_name.c_str(), full_message.c_str());
                    break;
            }
        }
        
        // Common error codes
        namespace codes {
            constexpr const char* INVALID_ATTRIBUTE = "E001";
            constexpr const char* ADDRESS_OUT_OF_BOUNDS = "E002";
            constexpr const char* INVALID_PACKET_TYPE = "E003";
            constexpr const char* CONFIGURATION_ERROR = "E004";
            constexpr const char* RESOURCE_EXHAUSTED = "E005";
        }
        
        // Convenience macros for common error reporting
        #define SOC_SIM_ERROR(module, code, msg) \
            soc_sim::error::report(soc_sim::error::Severity::ERROR, module, code, msg)
            
        #define SOC_SIM_WARNING(module, code, msg) \
            soc_sim::error::report(soc_sim::error::Severity::WARNING, module, code, msg)
            
        #define SOC_SIM_INFO(module, code, msg) \
            soc_sim::error::report(soc_sim::error::Severity::INFO, module, code, msg)
    }
}

#endif