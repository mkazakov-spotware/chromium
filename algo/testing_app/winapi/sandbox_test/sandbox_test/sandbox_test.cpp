#ifndef _UNICODE
#define _UNICODE
#define UNICODE
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include <windows.h>

#include <netioapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include <iostream>
#include <string>


#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Advapi32.lib")

int win32_getifentry()
{
  DWORD dwRetVal = 0;
  unsigned int i;

  MIB_IF_TABLE2 *pIfTable;
  MIB_IF_ROW2 *pIfRow;

  dwRetVal = GetIfTable2(&pIfTable);

  if (dwRetVal != NO_ERROR) {
    fprintf(stderr, "GetIfTable2 return value is %lu\n", dwRetVal);
    LPSTR messageBuffer = nullptr;
    FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER
        | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, dwRetVal, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&messageBuffer, 0, NULL);

    fprintf(stderr, "Formatted message is %s\n", messageBuffer);
    LocalFree(messageBuffer);
    return -1;
  }

  fprintf(stderr, "\tNum Entries: %ld\n\n", pIfTable->NumEntries);
  for (i = 0; i < pIfTable->NumEntries; i++) {
    pIfRow = &pIfTable->Table[i];

    fprintf(stderr, "[%lu]:\t ", pIfRow->InterfaceIndex);
    fprintf(stderr, "%ws", pIfRow->Alias);

    fprintf(stderr, "   \t(%ws)", pIfRow->Description);
    fprintf(stderr, "\n");
  }
  return 0;
}

std::string GetLastErrorAsString() {
  DWORD errorMessageID = ::GetLastError();
  std::cerr << "LastError is " << errorMessageID << std::endl;
  if (errorMessageID == 0) {
    return std::string();
  }

  LPSTR messageBuffer = nullptr;
  size_t size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER
        | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&messageBuffer, 0, NULL);

  std::string message(messageBuffer, size);
  LocalFree(messageBuffer);

  return message;
}

int run()
{
  HKEY hKey;
  DWORD dwDisposition;
  std::wstring key = L"sandbox_test";
  LPCWSTR lpsKey = key.c_str();
  LSTATUS res = RegCreateKeyEx(
      HKEY_CURRENT_USER,
      lpsKey, 0,
      NULL, 0, KEY_WRITE, NULL, &hKey,
      &dwDisposition);
  if (res == ERROR_SUCCESS){
    std::wcerr << L"Key " << key << L" has been written successfully" << std::endl;
    RegCloseKey(hKey);
  }
  else {
    std::wcerr << L"Create key " << key << L" has failed" << std::endl;
    std::cerr << "Return code is " << res << std::endl;
    return -1;
  }

  char* profile;
  size_t requiredSize;

  getenv_s(&requiredSize, NULL, 0, "USERPROFILE");
  if (requiredSize == 0) {
    printf("USERPROFILE doesn't exist!\n");
    exit(1);
  }

  profile = (char*)malloc(requiredSize * sizeof(char));
  if (!profile) {
    printf("Failed to allocate memory!\n");
    exit(1);
  }

  getenv_s(&requiredSize, profile, requiredSize, "USERPROFILE");
  std::string profile_path = std::string(profile);
  auto profile_wide = std::wstring(profile_path.begin(), profile_path.end());
  std::wstring test_folder = profile_wide + std::wstring(L"\\sandbox_test");

  if (!CreateDirectoryW(test_folder.c_str(), NULL) && ERROR_ALREADY_EXISTS != GetLastError())
  {
    std::wcerr << L"Failed to create the folder " << test_folder << std::endl;
    std::cerr << GetLastErrorAsString() << std::endl;
    return -4;
  }

  std::wstring file = test_folder + std::wstring(L"\\sandbox_test.txt");
  std::wstring forbidden_file = profile_wide + std::wstring(L"\\forbidden");
  LPCWSTR lpsFile = file.c_str();
  LPCWSTR lpsForbiddenFile = forbidden_file.c_str();
  {
    HANDLE hFile = CreateFile(
        lpsFile,
        GENERIC_WRITE,
        FILE_SHARE_WRITE,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
      std::wcerr << L"Failed to create file " << file << std::endl;
      std::cerr << GetLastErrorAsString() << std::endl;
      return -2;
    }
    else {
      std::wcerr << L"File " << file << L" has been created successfully" << std::endl;
      CloseHandle(hFile);
    }
  }
  {
    HANDLE hFile = CreateFile(
        lpsForbiddenFile,
        GENERIC_WRITE,
        FILE_SHARE_WRITE,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
      std::wcerr << L"Creation of file " << forbidden_file <<
        " has been canceled by the broker in accordance with FS policy" << std::endl;
      CloseHandle(hFile);
    }
    else {
      std::wcerr << L"File " << file << L" has been created, but it shoudn't be!!!" << std::endl;
    }
  }

  LPCWSTR event_name = L"New_event";
  if (CreateEventW(NULL, false, true, event_name)) {
    std::wcerr << L"Event \"" << event_name << L"\" has been created successfully" << std::endl;
  }
  else {
    std::wcerr << L"Failed to create event " << event_name << std::endl;
    std::cerr << GetLastErrorAsString() << std::endl;
    return -3;
  }

  win32_getifentry();

  return 0;
}
