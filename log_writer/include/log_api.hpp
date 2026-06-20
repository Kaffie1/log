#pragma once

#include "log_manager.hpp"
#include "log_types.hpp"

#define LOG_LEVEL_TRACE ::naviai::log::LogLevel::Trace
#define LOG_LEVEL_DEBUG ::naviai::log::LogLevel::Debug
#define LOG_LEVEL_INFO ::naviai::log::LogLevel::Info
#define LOG_LEVEL_WARN ::naviai::log::LogLevel::Warn
#define LOG_LEVEL_ERROR ::naviai::log::LogLevel::Error
#define LOG_LEVEL_CRITICAL ::naviai::log::LogLevel::Critical

#define LOG_TRACE(module, payload) ::naviai::log::LogManager::Trace(module, payload)
#define LOG_DEBUG(module, payload) ::naviai::log::LogManager::Debug(module, payload)
#define LOG_INFO(module, payload) ::naviai::log::LogManager::Info(module, payload)
#define LOG_WARN(module, payload) ::naviai::log::LogManager::Warn(module, payload)
#define LOG_ERROR(module, payload) ::naviai::log::LogManager::Error(module, payload)
#define LOG_CRITICAL(module, payload) ::naviai::log::LogManager::Critical(module, payload)
