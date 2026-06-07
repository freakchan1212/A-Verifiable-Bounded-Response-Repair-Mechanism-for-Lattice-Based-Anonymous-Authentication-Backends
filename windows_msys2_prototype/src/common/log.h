#pragma once
#include <cstdio>
#include <cstdarg>

namespace common {

	
	enum class LogLevel {
		ERROR = 0,
		WARN = 1,
		INFO = 2,
		DEBUG = 3,
	};

#ifndef COMMON_LOG_LEVEL
	
	
	
	
#define COMMON_LOG_LEVEL 2
#endif

	
	inline const char* level_name(LogLevel lv) {
		switch (lv) {
		case LogLevel::ERROR: return "ERROR";
		case LogLevel::WARN:  return "WARN";
		case LogLevel::INFO:  return "INFO";
		case LogLevel::DEBUG: return "DEBUG";
		default:              return "LOG";
		}
	}
	
	
	
	inline void vlog(LogLevel lv, const char* fmt, va_list ap) {
		std::fprintf(stderr, "[%s] ", level_name(lv));
		std::vfprintf(stderr, fmt, ap);
		std::fprintf(stderr, "\n");
	}
	
	
	
	inline void log(LogLevel lv, const char* fmt, ...) {
#if COMMON_LOG_LEVEL >= 0
		
		
		
		
		
		if ((int)lv > (int)LogLevel::ERROR && COMMON_LOG_LEVEL < 1) return;
		if ((int)lv > (int)LogLevel::WARN  && COMMON_LOG_LEVEL < 2) return;
		if ((int)lv > (int)LogLevel::INFO  && COMMON_LOG_LEVEL < 3) return;
#endif
		va_list ap;
		va_start(ap, fmt);
		vlog(lv, fmt, ap);
		va_end(ap);
	}

} 


#define LOGE(...) ::common::log(::common::LogLevel::ERROR, __VA_ARGS__)
#define LOGW(...) ::common::log(::common::LogLevel::WARN,  __VA_ARGS__)
#define LOGI(...) ::common::log(::common::LogLevel::INFO,  __VA_ARGS__)
#define LOGD(...) ::common::log(::common::LogLevel::DEBUG, __VA_ARGS__)
