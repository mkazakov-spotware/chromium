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
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/app_container_base.h"
#include "sandbox/win/src/restricted_token_utils.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/security_level.h"

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
bool InitializeLogging() {
  // Generate log filename with timestamp
  std::wstring log_filename = L"broker_" + GetCurrentDateTimeString() + L".log";
  
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_FILE | logging::LOG_TO_STDERR;
  settings.log_file_path = log_filename.c_str();
  
  return logging::InitLogging(settings);
}

// Initialize logging for a child target process
bool InitializeChildProcessLogging(const wchar_t* prefix, DWORD process_id) {
  // Generate log filename with prefix, process ID and timestamp
  std::wstring log_filename = L"";
  
  if (prefix && wcslen(prefix) > 0) {
    // Extract the base name without path or extension
    std::wstring prefix_str(prefix);
    size_t last_slash = prefix_str.find_last_of(L"\\/");
    if (last_slash != std::wstring::npos) {
      prefix_str = prefix_str.substr(last_slash + 1);
    }
    size_t dot = prefix_str.find_last_of(L'.');
    if (dot != std::wstring::npos) {
      prefix_str = prefix_str.substr(0, dot);
    }
    
    log_filename += prefix_str + L"_";
  }
  
  // Add process ID and timestamp to the filename
  log_filename += L"proc_" + std::to_wstring(process_id) + L"_" + GetCurrentDateTimeString() + L".log";
  
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_FILE | logging::LOG_TO_STDERR;
  settings.log_file_path = log_filename.c_str();
  
  LOG(INFO) << L"Initializing child process logging to: " << log_filename.c_str();
  
  return logging::InitLogging(settings);
}

ResultCode SetupProtectedMode(
  BrokerServices* broker,
  const std::unique_ptr<TargetPolicy>& target_policy,
  const wchar_t* package_name) {
  ResultCode result;

  // If stdout/stderr point to a Windows console, these calls will
  // have no effect. These calls can fail with SBOX_ERROR_BAD_PARAMS.
  target_policy->SetStdoutHandle(GetStdHandle(STD_OUTPUT_HANDLE));
  target_policy->SetStderrHandle(GetStdHandle(STD_ERROR_HANDLE));

  auto* config = target_policy->GetConfig();

  do {
    result = config->SetTokenLevel(
      USER_RESTRICTED_SAME_ACCESS, TokenLevel::USER_LOCKDOWN);
    if (result != SBOX_ALL_OK)
      break;

    config->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_UNTRUSTED);

    result = config->SetProcessMitigations(
      sandbox::MITIGATION_DEP | sandbox::MITIGATION_DEP_NO_ATL_THUNK |
      sandbox::MITIGATION_SEHOP | sandbox::MITIGATION_HEAP_TERMINATE |
      sandbox::MITIGATION_BOTTOM_UP_ASLR |
      sandbox::MITIGATION_HIGH_ENTROPY_ASLR |
      sandbox::MITIGATION_WIN32K_DISABLE |
      sandbox::MITIGATION_EXTENSION_POINT_DISABLE |
      sandbox::MITIGATION_NONSYSTEM_FONT_DISABLE |
      sandbox::MITIGATION_HARDEN_TOKEN_IL_POLICY |
      sandbox::MITIGATION_IMAGE_LOAD_NO_REMOTE |
      sandbox::MITIGATION_IMAGE_LOAD_NO_LOW_LABEL);

    // RELOCATE_IMAGE and RELOCATE_IMAGE_REQUIRED could be in
    // SetProcessMitigations above, but they need to be delayed in Debug builds.
    // It's easier to just set them up as delayed for both Debug and Release
    // builds.
    result = config->SetDelayedProcessMitigations(
      sandbox::MITIGATION_RELOCATE_IMAGE |
      sandbox::MITIGATION_RELOCATE_IMAGE_REQUIRED |
      sandbox::MITIGATION_STRICT_HANDLE_CHECKS |
      sandbox::MITIGATION_DLL_SEARCH_ORDER);

    if (result != SBOX_ALL_OK)
      break;

    result = broker->CreateAlternateDesktop(sandbox::Desktop::kAlternateWinstation);
    if (result != SBOX_ALL_OK)
      break;

    result = config->SetJobLevel(JobLevel::kLockdown, 0);
    if (result != SBOX_ALL_OK)
      break;

//  result = config->AddAppContainerProfile(
//    package_name, true);

//  if (result == SBOX_ERROR_UNSUPPORTED)
//  {
//    LOG(INFO) << L"AppContainer profile is not supported" << std::endl;
//    result = SBOX_ALL_OK;
//  }

    result = config->AddRule(SubSystem::kWin32kLockdown, Semantics::kFakeGdiInit, nullptr);
    if (result != SBOX_ALL_OK)
      break;

    config->SetLockdownDefaultDacl();
  }
  while (false);

  return result;
}

ResultCode SpawnTarget(const wchar_t* path,
                       const wchar_t* arguments,
                       const wchar_t* current_directory,
                       BrokerServices* broker_services,
                       std::unique_ptr<TargetPolicy>& target_policy,
                       PROCESS_INFORMATION* process_information) {
  LOG(INFO) << L"Target path: " << path << std::endl;
  LOG(INFO) << L"Target arguments: " << arguments << std::endl;

  ResultCode last_warning = SBOX_ALL_OK;
  DWORD last_error = 0;

  const ResultCode result = broker_services->SpawnTarget(path,
                                                         arguments,
                                                         current_directory,
                                                         std::move(target_policy),
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

ResultCode SetupFileRules(std::unique_ptr<TargetPolicy>& target_policy,
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
      result = target_policy->GetConfig()->AddRule(sandbox::SubSystem::kFiles,
                                      sandbox::Semantics::kFilesAllowAny,
                                      rule_path.c_str());
    } else {
      result = target_policy->GetConfig()->AddRule(sandbox::SubSystem::kFiles,
                                      sandbox::Semantics::kFilesAllowReadonly,
                                      rule_path.c_str());
    }

    if (result != SBOX_ALL_OK)
      break;

    LOG(INFO) << L"Rule [FileSystem] added: " << rule_path.c_str() << std::endl;
  }

  return result;
}

ResultCode SetupRegistryRules(std::unique_ptr<TargetPolicy>& target_policy,
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
      result = target_policy->GetConfig()->AddRule(sandbox::SubSystem::kREGISTRY,
                                      sandbox::Semantics::REG_ALLOW_ANY,
                                      rule_path.c_str());
    } else {
      result = target_policy->GetConfig()->AddRule(sandbox::SubSystem::kREGISTRY,
                                      sandbox::Semantics::REG_ALLOW_READONLY,
                                      rule_path.c_str());
    }

    if (result != SBOX_ALL_OK)
      break;

    LOG(INFO) << L"Rule [Registry] added: " << rule_path.c_str() << std::endl;
  }

  return result;
}

ResultCode SetupEventRules(std::unique_ptr<TargetPolicy>& target_policy,
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
      result = target_policy->GetConfig()->AddRule(sandbox::SubSystem::SUBSYS_SYNC,
                                      sandbox::Semantics::EVENTS_ALLOW_ANY,
                                      rule_path.c_str());
    } else {
      result = target_policy->GetConfig()->AddRule(sandbox::SubSystem::SUBSYS_SYNC,
                                      sandbox::Semantics::EVENTS_ALLOW_READONLY,
                                      rule_path.c_str());
    }

    if (result != SBOX_ALL_OK)
      break;

    LOG(INFO) << L"Rule [Event] added: " << rule_path.c_str() << std::endl;
  }

  return result;
}

ResultCode SetupNamedPipeRules(std::unique_ptr<TargetPolicy>& target_policy,
                               const wchar_t* rules) {
  ResultCode result = SBOX_ALL_OK;

  if (rules == nullptr)
    return result;

  std::wstring rules_string(rules);
  if (rules_string.length() == 0)
    return result;

  std::vector<std::wstring> rules_array = SplitString(rules_string, L'|');

  for (std::wstring rule : rules_array) {
    result = target_policy->GetConfig()->AddRule(sandbox::SubSystem::kNamedPipes,
                                    sandbox::Semantics::kNamedPipesAllowAny,
                                    rule.c_str());
    if (result != SBOX_ALL_OK)
      break;

    LOG(INFO) << L"Rule [NamedPipeSystem] added: " << rule.c_str() << std::
        endl;
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

    std::unique_ptr<TargetPolicy> target_policy = broker_services->CreatePolicy();

    result_code = SetupProtectedMode(broker_services, target_policy, options->package_name);
    if (result_code != SBOX_ALL_OK) {
      break;
    }

    result_code = SetupFileRules(target_policy, options->fs_rules);
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
    
    // Initialize logging for the new child process
    InitializeChildProcessLogging(options->host_path, process_information.dwProcessId);
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
