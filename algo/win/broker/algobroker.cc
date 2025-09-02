#include <tchar.h>
#include <shlwapi.h>
#include <aclapi.h>
#include <iostream>
#include <string>
#include <ctime>
#include <shellapi.h>

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

std::wstring GenerateBrokerLogFilename() {
  return L"broker_" + GetCurrentDateTimeString() + L".log";
}

// Clean title to be filename-safe (helper function)
std::wstring CleanTitleForFilename(const std::wstring& title) {
    if (title.empty()) {
        return L"unknown";
    }

    std::wstring cleaned_title = title;

    // Remove invalid characters
    std::wstring invalid_chars = L"<>:\"/\\|?*";
    for (wchar_t c : invalid_chars) {
        std::replace(cleaned_title.begin(), cleaned_title.end(), c, L'_');
    }

    // Limit length to reasonable size
    if (cleaned_title.length() > 50) {
        cleaned_title = cleaned_title.substr(0, 50);
    }

    return cleaned_title;
}

// Get the title argument from command line (for direct-target mode)
std::wstring GetTitleArgument() {
    int argc;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    std::wstring title;
    if (argv != nullptr) {
        if (argc > 1) {
            title = std::wstring(argv[1]);
        }
    }

    LocalFree(argv);
    return CleanTitleForFilename(title);
}

// Generate target log filename with optional title parameter
std::wstring GenerateTargetLogFilename(bool is_protected = true, const wchar_t* title_override = nullptr) {
    std::wstring title;

    if (title_override != nullptr && wcslen(title_override) > 0) {
        // Use provided title (broker-target mode)
        title = CleanTitleForFilename(std::wstring(title_override));
    } else {
        // Fall back to command line argument (direct-target mode)
        title = GetTitleArgument();
    }

    std::wstring filename = L"target_";

    if (!title.empty() && title != L"unknown") {
        filename += title + L"_";
    }

    if (!is_protected) {
        filename += L"unprotected_";
    }

    filename += GetCurrentDateTimeString() + L".log";
    return filename;
}

// Generate desktop log filename with optional title parameter
std::wstring GenerateDesktopLogFilename(bool is_protected = true, const wchar_t* title_override = nullptr) {
    std::wstring title;

    if (title_override != nullptr && wcslen(title_override) > 0) {
        // Use provided title (broker-target mode)
        title = CleanTitleForFilename(std::wstring(title_override));
    } else {
        // Fall back to command line argument (direct-target mode)
        title = GetTitleArgument();
    }

    std::wstring filename = L"desktop_";

    if (!title.empty() && title != L"unknown") {
        filename += title + L"_";
    }

    if (!is_protected) {
        filename += L"unprotected_";
    }

    filename += GetCurrentDateTimeString() + L".log";
    return filename;
}

// Extract log directory path from executable path with fallback
std::wstring GetLogDirectoryPath() {
  wchar_t module_name[MAX_PATH];
  if (GetModuleFileName(nullptr, module_name, MAX_PATH) == 0) {
    LOG(ERROR) << "Failed to get executable path for log file path";

    // Fall back to current directory
    wchar_t current_dir[MAX_PATH];
    if (GetCurrentDirectory(MAX_PATH, current_dir) != 0) {
      std::wstring log_dir_path = std::wstring(current_dir);
      if (log_dir_path.back() != L'\\') {
        log_dir_path += L"\\";
      }
      return log_dir_path;
    }

    LOG(ERROR) << "Failed to get current directory, using empty path";
    return L"";
  }

  // Extract directory from executable path using rfind approach
  std::wstring log_dir_path = module_name;
  std::wstring::size_type last_backslash = log_dir_path.rfind(L'\\', log_dir_path.size());
  if (last_backslash != std::wstring::npos) {
    log_dir_path.erase(last_backslash + 1); // Keep the trailing backslash
  } else {
    LOG(ERROR) << "Invalid executable path format - no backslash found";
    return L"";
  }

  return log_dir_path;
}

bool IsFileLoggingEnabled() {
  wchar_t buffer[MAX_PATH];
  DWORD result = GetEnvironmentVariable(algo::CT_ALGOHOST_SESSION_LOG_FILE_ENABLED, buffer, MAX_PATH);

  if (result > 0 && result < MAX_PATH) {
    std::wstring value(buffer);
    return (value == L"True");
  }

  return false;
}

bool InitializeLogging() {
  bool enable_file_logging = IsFileLoggingEnabled();

  std::wstring log_filename;
  std::wstring log_dir_path = GetLogDirectoryPath();
  if (!log_dir_path.empty()) {
    log_filename = log_dir_path + GenerateBrokerLogFilename();
  } else {
    // Fallback to current directory if log directory path failed
    log_filename = GenerateBrokerLogFilename();
  }

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_STDERR;

  if (enable_file_logging) {
    settings.logging_dest |= logging::LOG_TO_FILE;
    settings.log_file_path = log_filename.c_str();
  }

  bool result = logging::InitLogging(settings);

  LOG(INFO) << L"enable_file_logging: " << enable_file_logging;
  LOG(INFO) << L"log_filename: " << log_filename;

  return result;
}

// Initialize logging for a child target process - reads log filename from environment variable
bool InitializeChildProcessLogging() {
  bool enable_file_logging = IsFileLoggingEnabled();

  if (!enable_file_logging) {
    logging::LoggingSettings settings;
    settings.logging_dest = logging::LOG_TO_STDERR;
    return logging::InitLogging(settings);
  }

  wchar_t buffer[MAX_PATH];
  DWORD env_result = GetEnvironmentVariable(algo::CT_ALGOHOST_TARGET_LOG_FILE_PATH, buffer, MAX_PATH);

  std::wstring log_filename;
  std::wstring log_message;

  if (env_result == 0 || env_result >= MAX_PATH) {
    // Environment variable not found or too long - generate new log filename with full path
    std::wstring log_dir_path = GetLogDirectoryPath();
    if (log_dir_path.empty()) {
      // Fall back to stderr only if we can't get log directory path
      logging::LoggingSettings settings;
      settings.logging_dest = logging::LOG_TO_STDERR;
      return logging::InitLogging(settings);
    }

    log_filename = log_dir_path + GenerateTargetLogFilename(false);
    log_message = L"CT_ALGOHOST_TARGET_LOG_FILE_PATH not found, using generated filename: " + log_filename;

    // Also create a desktop log file for managed .NET app (similar to broker behavior)
    std::wstring desktop_log_path = log_dir_path + GenerateDesktopLogFilename(false);
    SetEnvironmentVariable(algo::CT_ALGOHOST_SESSION_LOG_FILE_PATH, desktop_log_path.c_str());
    log_message += L"\nSet desktop log filename: " + desktop_log_path;

  } else {
    // Use log filename from environment variable
    log_filename = std::wstring(buffer);
    log_message = L"Initializing child process logging to (from env): " + log_filename;
  }

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_FILE | logging::LOG_TO_STDERR;
  settings.log_file_path = log_filename.c_str();

  bool result = logging::InitLogging(settings);

  LOG(INFO) << L"enable_file_logging: " << enable_file_logging;
  LOG(INFO) << L"log_filename: " << log_filename;
  LOG(INFO) << log_message;

  return result;
}

ResultCode SetupProtectedMode(
  const scoped_refptr<TargetPolicy>& target_policy,
  const wchar_t* package_name) {
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

    result_code = SetupProtectedMode(target_policy, options->package_name);
    if (result_code != SBOX_ALL_OK) {
      break;
    }

    bool enable_file_logging = IsFileLoggingEnabled();

    std::wstring modified_fs_rules;
    if (options->fs_rules && wcslen(options->fs_rules) > 0) {
      modified_fs_rules = options->fs_rules;
    }

    if (enable_file_logging) {
      // Use the common log directory path function
      std::wstring log_dir_path = GetLogDirectoryPath();
      if (log_dir_path.empty()) {
        LOG(ERROR) << "Failed to get log directory path";
        // Continue with best effort - don't fail the entire spawn operation
      } else {
        // Set the log filename with full path as an environment variable for the target process
        std::wstring full_log_path = log_dir_path + GenerateTargetLogFilename(true, options->title);
        SetEnvironmentVariable(algo::CT_ALGOHOST_TARGET_LOG_FILE_PATH, full_log_path.c_str());
        LOG(INFO) << "Set target log filename: " << full_log_path.c_str();

        // Create a desktop log file for managed .NET app
        std::wstring desktop_log_path = log_dir_path + GenerateDesktopLogFilename(true, options->title);
        SetEnvironmentVariable(algo::CT_ALGOHOST_SESSION_LOG_FILE_PATH, desktop_log_path.c_str());
        LOG(INFO) << "Set desktop log filename: " << desktop_log_path.c_str();

        // Add log directory path to filesystem rules - allow writing to entire directory
        if (!modified_fs_rules.empty()) {
          modified_fs_rules += L"|" + full_log_path + L"|RW" + L"|" + desktop_log_path + L"|RW";
        } else {
          modified_fs_rules = full_log_path + L"|RW" + L"|" + desktop_log_path + L"|RW";
        }

        LOG(INFO) << L"Added log files to filesystem rules: " << full_log_path.c_str() << L", " << desktop_log_path.c_str();
      }
    }

    // Use the modified rules (or original rules if no modifications were made)
    const wchar_t* fs_rules_to_use = modified_fs_rules.empty() ? options->fs_rules : modified_fs_rules.c_str();
    result_code = SetupFileRules(target_policy, fs_rules_to_use);
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
