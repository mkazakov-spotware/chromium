#include <tchar.h>
#include <shlwapi.h>
#include <aclapi.h>
#include <iostream>
#include <string>
#include <ctime>

#include "algo/win/broker/algobroker.h"
#include "base/logging.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/app_container_profile.h"
#include "sandbox/win/src/app_container_profile_base.h"
#include "sandbox/win/src/restricted_token_utils.h"

using namespace sandbox;

// Get current datetime as a string in format YYYYMMDD_HHMMSS
std::wstring GetCurrentDateTimeString() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_s(&timeinfo, &now);
  
  wchar_t buffer[20];
  wcsftime(buffer, 20, L"%Y%m%d_%H%M%S", &timeinfo);
  
  return std::wstring(buffer);
}

// Initialize logging to file for the broker process
// Define environment variable name for file logging control
#define ENV_ENABLE_FILE_LOGGING L"ALGO_ENABLE_FILE_LOGGING"

bool InitializeLogging() {
  // Check environment variable to determine if file logging is enabled
  wchar_t buffer[MAX_PATH];
  DWORD result = GetEnvironmentVariable(ENV_ENABLE_FILE_LOGGING, buffer, MAX_PATH);
  bool enable_file_logging = false;
  
  if (result > 0 && result < MAX_PATH) {
    std::wstring value(buffer);
    enable_file_logging = (value == L"True");
  }
  
  // Generate log filename with timestamp
  std::wstring log_filename = L"broker_" + GetCurrentDateTimeString() + L".log";
  
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_STDERR; // Always log to stderr

  LOG(INFO) << L"enable_file_logging: " << enable_file_logging;
  LOG(INFO) << L"log_filename: " << log_filename;

  if (enable_file_logging) {
    settings.logging_dest |= logging::LOG_TO_FILE; // Add file logging if enabled
    settings.log_file_path = log_filename.c_str();
  }
  
  return logging::InitLogging(settings);
}

// Define environment variable name for target log file
#define ENV_TARGET_LOG_FILE L"ALGO_TARGET_LOG_FILE"

// Define environment variable name for .NET desktop log file
#define ENV_DESKTOP_LOG_FILE L"ALGO_DESKTOP_LOG_FILE"

// Generate log filename for a child target process
std::wstring GenerateTargetLogFilename() {
  return L"target_" + GetCurrentDateTimeString() + L".log";
}

// Generate log filename for managed .NET desktop app
std::wstring GenerateDesktopLogFilename() {
  return L"desktop_" + GetCurrentDateTimeString() + L".log";
}

// Initialize logging for a child target process - reads log filename from environment variable
bool InitializeChildProcessLogging() {
  // Check environment variable to determine if file logging is enabled
  wchar_t buffer[MAX_PATH];
  DWORD result = GetEnvironmentVariable(ENV_ENABLE_FILE_LOGGING, buffer, MAX_PATH);
  bool enable_file_logging = false;
  
  if (result > 0 && result < MAX_PATH) {
    std::wstring value(buffer);
    enable_file_logging = (value == L"True");
  }
  
  if (!enable_file_logging) {
    // File logging disabled, only use stderr
    logging::LoggingSettings settings;
    settings.logging_dest = logging::LOG_TO_STDERR;
    return logging::InitLogging(settings);
  }
  
  // Read log filename from environment variable
  result = GetEnvironmentVariable(ENV_TARGET_LOG_FILE, buffer, MAX_PATH);
  if (result == 0 || result >= MAX_PATH) {
    // Environment variable not set or too long, use default naming
    std::wstring log_filename = GenerateTargetLogFilename();
    
    logging::LoggingSettings settings;
    settings.logging_dest = logging::LOG_TO_FILE | logging::LOG_TO_STDERR;
    settings.log_file_path = log_filename.c_str();

    LOG(INFO) << L"Initializing child process logging to (default): " << log_filename.c_str();
    
    return logging::InitLogging(settings);
  }
  
  // Use log filename from environment variable
  std::wstring log_filename(buffer);
  
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_FILE | logging::LOG_TO_STDERR;
  settings.log_file_path = log_filename.c_str();
  
  LOG(INFO) << L"Initializing child process logging to (from env): " << log_filename.c_str();
  
  return logging::InitLogging(settings);
}

ResultCode SetupProtectedMode(
  const scoped_refptr<TargetPolicy>& target_policy,
  const wchar_t* package_name,
  const wchar_t* python_dll_path = nullptr) {
  ResultCode result;

  // If stdout/stderr point to a Windows console, these calls will
  // have no effect. These calls can fail with SBOX_ERROR_BAD_PARAMS.
  target_policy->SetStdoutHandle(GetStdHandle(STD_OUTPUT_HANDLE));
  target_policy->SetStderrHandle(GetStdHandle(STD_ERROR_HANDLE));

  do {
    result = target_policy->SetTokenLevel(
      USER_RESTRICTED_SAME_ACCESS, TokenLevel::USER_LOCKDOWN);
    if (result != SBOX_ALL_OK)
      break;

    result = target_policy->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_UNTRUSTED);
    if (result != SBOX_ALL_OK)
      break;

    result = target_policy->SetAlternateDesktop(true);
    if (result != SBOX_ALL_OK)
      break;

    result = target_policy->SetJobLevel(JOB_LOCKDOWN, 0);
    if (result != SBOX_ALL_OK)
      break;

  //result = target_policy->AddAppContainerProfile(
  //  package_name, true);

  //if (result == SBOX_ERROR_UNSUPPORTED)
  //{
  //  LOG(INFO) << L"AppContainer profile is not supported" << std::endl;
  //  result = SBOX_ALL_OK;
  //}

    if (result != SBOX_ALL_OK)
      break;
  }
  while (false);

  return result;
}

ResultCode SpawnTarget(const wchar_t* path,
                       const wchar_t* arguments,
                       const wchar_t* current_directory,
                       BrokerServices* broker_services,
                       scoped_refptr<TargetPolicy> target_policy,
                       PROCESS_INFORMATION* process_information) {
  LOG(INFO) << L"Target path: " << path << std::endl;
  LOG(INFO) << L"Target arguments: " << arguments << std::endl;

  ResultCode last_warning = SBOX_ALL_OK;
  DWORD last_error = 0;

  const ResultCode result = broker_services->SpawnTarget(path,
                                                         arguments,
                                                         current_directory,
                                                         target_policy,
                                                         &last_warning,
                                                         &last_error,
                                                         process_information);
  if (result != SBOX_ALL_OK) {
    if (last_warning != SBOX_ALL_OK) {
      LOG(INFO) << L"Last warning: " << last_warning << std::endl;
    }
    if (last_error != 0) {
      LPWSTR messageBuffer = nullptr;
      size_t size = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, last_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&messageBuffer, 0, nullptr);

      const std::wstring message(messageBuffer, size);
      LOG(INFO) << L"Last error: " << message.c_str() << std::endl;
    }
  }

  return result;
}

std::vector<std::wstring> SplitString(const std::wstring& str, wchar_t ch) {
  std::vector<std::wstring> ret;
  size_t startPos = 0;
  size_t endPos = str.find(ch, startPos);
  while (endPos != std::wstring::npos) {
    ret.push_back(str.substr(startPos, endPos - startPos));
    startPos = endPos + 1;
    endPos = str.find(ch, startPos);
  }
  ret.push_back(str.substr(startPos, str.length() - startPos));
  return ret;
}

ResultCode SetupFileRules(scoped_refptr<TargetPolicy> target_policy,
                          const wchar_t* rules) {
  ResultCode result = SBOX_ALL_OK;

  if (rules == nullptr)
    return result;

  std::wstring rules_string(rules);
  if (rules_string.length() == 0)
    return result;

  std::vector<std::wstring> rules_array = SplitString(rules_string, L'|');

  const auto rules_array_size = rules_array.size();
  if (rules_array.size() % 2 != 0) {
    LOG(INFO) << L"File rules are not correct: " << rules_string.c_str() << std::endl;
    return SBOX_ERROR_BAD_PARAMS;
  }

  for (size_t i = 0; i < rules_array_size; i += 2) {
    auto rule_path = rules_array[i];
    auto rule_sem = rules_array[i+1];
    if (rule_sem == L"RW") {
      result = target_policy->AddRule(TargetPolicy::SubSystem::SUBSYS_FILES,
                                      TargetPolicy::Semantics::FILES_ALLOW_ANY,
                                      rule_path.c_str());
    } else {
      result = target_policy->AddRule(TargetPolicy::SubSystem::SUBSYS_FILES,
                                      TargetPolicy::Semantics::FILES_ALLOW_READONLY,
                                      rule_path.c_str());
    }

    if (result != SBOX_ALL_OK)
      break;

    LOG(INFO) << L"Rule [FileSystem] added: " << rule_path.c_str() << std::endl;
  }

  return result;
}

ResultCode SetupRegistryRules(scoped_refptr<TargetPolicy> target_policy,
                          const wchar_t* rules) {
  ResultCode result = SBOX_ALL_OK;

  if (rules == nullptr)
    return result;

  std::wstring rules_string(rules);
  if (rules_string.length() == 0)
    return result;

  std::vector<std::wstring> rules_array = SplitString(rules_string, L'|');

  const auto rules_array_size = rules_array.size();
  if (rules_array.size() % 2 != 0) {
    LOG(INFO) << L"Registry rules are not correct: " << rules_string.c_str() << std::endl;
    return SBOX_ERROR_BAD_PARAMS;
  }

  for (size_t i = 0; i < rules_array_size; i += 2) {
    auto rule_path = rules_array[i];
    auto rule_sem = rules_array[i+1];
    if (rule_sem == L"RW") {
      result = target_policy->AddRule(TargetPolicy::SubSystem::SUBSYS_REGISTRY,
                                      TargetPolicy::Semantics::REG_ALLOW_ANY,
                                      rule_path.c_str());
    } else {
      result = target_policy->AddRule(TargetPolicy::SubSystem::SUBSYS_REGISTRY,
                                      TargetPolicy::Semantics::REG_ALLOW_READONLY,
                                      rule_path.c_str());
    }

    if (result != SBOX_ALL_OK)
      break;

    LOG(INFO) << L"Rule [Registry] added: " << rule_path.c_str() << std::endl;
  }

  return result;
}

ResultCode SetupEventRules(scoped_refptr<TargetPolicy> target_policy,
                          const wchar_t* rules) {
  ResultCode result = SBOX_ALL_OK;

  if (rules == nullptr)
    return result;

  std::wstring rules_string(rules);
  if (rules_string.length() == 0)
    return result;

  std::vector<std::wstring> rules_array = SplitString(rules_string, L'|');

  const auto rules_array_size = rules_array.size();
  if (rules_array.size() % 2 != 0) {
    LOG(INFO) << L"Event rules are not correct: " << rules_string.c_str() << std::endl;
    return SBOX_ERROR_BAD_PARAMS;
  }

  for (size_t i = 0; i < rules_array_size; i += 2) {
    auto rule_path = rules_array[i];
    auto rule_sem = rules_array[i+1];
    if (rule_sem == L"RW") {
      result = target_policy->AddRule(TargetPolicy::SubSystem::SUBSYS_SYNC,
                                      TargetPolicy::Semantics::EVENTS_ALLOW_ANY,
                                      rule_path.c_str());
    } else {
      result = target_policy->AddRule(TargetPolicy::SubSystem::SUBSYS_SYNC,
                                      TargetPolicy::Semantics::EVENTS_ALLOW_READONLY,
                                      rule_path.c_str());
    }

    if (result != SBOX_ALL_OK)
      break;

    LOG(INFO) << L"Rule [Event] added: " << rule_path.c_str() << std::endl;
  }

  return result;
}

ResultCode SetupNamedPipeRules(scoped_refptr<TargetPolicy> target_policy,
                               const wchar_t* rules) {
  ResultCode result = SBOX_ALL_OK;

  if (rules == nullptr)
    return result;

  std::wstring rules_string(rules);
  if (rules_string.length() == 0)
    return result;

  std::vector<std::wstring> rules_array = SplitString(rules_string, L'|');

  for (std::wstring rule : rules_array) {
    result = target_policy->AddRule(TargetPolicy::SubSystem::SUBSYS_NAMED_PIPES,
                                    TargetPolicy::Semantics::NAMEDPIPES_ALLOW_ANY,
                                    rule.c_str());
    if (result != SBOX_ALL_OK)
      break;

    LOG(INFO) << L"Rule [NamedPipeSystem] added: " << rule.c_str() << std::endl;
  }

  return result;
}

bool Initialize() {
  // Initialize broker logging first
  if (!InitializeLogging()) {
    std::cerr << "Failed to initialize logging" << std::endl;
  }
  
  LOG(INFO) << L"Broker Services initialize." << std::endl;
  BrokerServices* broker_services = SandboxFactory::GetBrokerServices();

  if (broker_services == nullptr) {
    LOG(ERROR) << "Failed to get broker services";
    return false;
  }

  LoadLibrary(L"userenv");

  return SBOX_ALL_OK == broker_services->Init();
}

int Spawn(const algo::TargetOptions* options,
          algo::TargetInformation* target_information) {
  ResultCode result_code;
  PROCESS_INFORMATION process_information;

  do {
    BrokerServices* broker_services = SandboxFactory::GetBrokerServices();
    if (broker_services == nullptr) {
      result_code = SBOX_ERROR_GENERIC;
      break;
    }

    scoped_refptr<TargetPolicy> target_policy
        = broker_services->CreatePolicy();

    result_code = SetupProtectedMode(target_policy, options->package_name, options->python_dll_path);
    if (result_code != SBOX_ALL_OK) {
      break;
    }

    // Check if Python DLL path is provided in the options
    if (options->python_dll_path && wcslen(options->python_dll_path) > 0) {
      LOG(INFO) << "Setting Python DLL path from config: " << options->python_dll_path;
      // Set the environment variable for the target process
      SetEnvironmentVariable(L"__CT_ALGOHOST_ENDPOINT_PYTHON_DLL_PATH", options->python_dll_path);
    } else {
      // Fallback to checking environment variable
      wchar_t python_dll_path[MAX_PATH] = {0};
      DWORD path_length = GetEnvironmentVariable(L"__CT_ALGOHOST_ENDPOINT_PYTHON_DLL_PATH",
                                               python_dll_path, MAX_PATH);
      if (path_length > 0) {
        LOG(INFO) << "Using Python DLL path from environment: " << python_dll_path;
        // Ensure it's available for the target process
        SetEnvironmentVariable(L"__CT_ALGOHOST_ENDPOINT_PYTHON_DLL_PATH", python_dll_path);
      } else {
        LOG(INFO) << "No Python DLL path found in config or environment";
      }
    }

    // Set the log filename with full path as an environment variable for the target process
    wchar_t log_dir[MAX_PATH];
    if (GetCurrentDirectory(MAX_PATH, log_dir) == 0) {
      LOG(ERROR) << "Failed to get current directory for log file path";
      // Can't break here since we're not in a loop, just continue with best effort
    }

    // Construct the full log path with proper directory separator
    std::wstring log_dir_path(log_dir);
    if (log_dir_path.back() != L'\\') {
      log_dir_path += L"\\";
    }
    std::wstring full_log_path = log_dir_path + GenerateTargetLogFilename();
    SetEnvironmentVariable(ENV_TARGET_LOG_FILE, full_log_path.c_str());
    LOG(INFO) << "Set target log filename: " << full_log_path.c_str();

    // Create a desktop log file for managed .NET app
    std::wstring desktop_log_path = log_dir_path + GenerateDesktopLogFilename();
    SetEnvironmentVariable(ENV_DESKTOP_LOG_FILE, desktop_log_path.c_str());
    LOG(INFO) << "Set desktop log filename: " << desktop_log_path.c_str();

    // Add log directory path to filesystem rules - allow writing to entire directory
    std::wstring modified_fs_rules;
    if (options->fs_rules && wcslen(options->fs_rules) > 0) {
      modified_fs_rules = std::wstring(options->fs_rules) + L"|" + full_log_path + L"|RW" + L"|" + desktop_log_path + L"|RW";
    } else {
      modified_fs_rules = full_log_path + L"|RW" + L"|" + desktop_log_path + L"|RW";
    }
    
    LOG(INFO) << L"Added log files to filesystem rules: " << full_log_path.c_str() << L", " << desktop_log_path.c_str();
    
    // Use the modified rules
    result_code = SetupFileRules(target_policy, modified_fs_rules.c_str());
    if (result_code != SBOX_ALL_OK) {
      break;
    }

    result_code = SetupRegistryRules(target_policy, options->reg_rules);
    if (result_code != SBOX_ALL_OK) {
      break;
    }

    result_code = SetupNamedPipeRules(target_policy, options->np_rules);
    if (result_code != SBOX_ALL_OK) {
      break;
    }

    result_code = SetupEventRules(target_policy, options->ev_rules);
    if (result_code != SBOX_ALL_OK) {
      break;
    }

    // Set Python environment variables for the target process if Python DLL path is provided
    if (options->python_dll_path && wcslen(options->python_dll_path) > 0) {
      LOG(INFO) << "Setting Python environment variables for target process: " << options->python_dll_path << std::endl;
      // Set the Python DLL path for the target process
      SetEnvironmentVariable(L"__CT_ALGOHOST_ENDPOINT_PYTHON_DLL_PATH", options->python_dll_path);
    }

    result_code = SpawnTarget(options->host_path,
                              options->command_line,
                              options->current_directory,
                              broker_services, target_policy,
                              &process_information);

    if (result_code != SBOX_ALL_OK) {
      break;
    }
  }
  while (false);

  if (result_code == SBOX_ALL_OK) {
    target_information->process_handle = process_information.hProcess;
    target_information->thread_handle = process_information.hThread;
    target_information->process_id = process_information.dwProcessId;
    target_information->thread_id = process_information.dwThreadId;

    LOG(INFO) << "Spawned child process with ID: " << process_information.dwProcessId;
  }

  return result_code;
}

bool Resume(const algo::TargetInformation* target_information) {
  return ResumeThread(target_information->thread_handle) >= 0;
}

void WaitAll() {
  BrokerServices* broker_services =
      SandboxFactory::GetBrokerServices();

  if (broker_services) {
    broker_services->WaitForAllTargets();
  }
}
