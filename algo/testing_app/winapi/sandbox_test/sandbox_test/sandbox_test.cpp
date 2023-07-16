#ifndef _UNICODE
#define _UNICODE
#define UNICODE
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <wininet.h>

#include <iphlpapi.h>
#include <icmpapi.h>

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include <iostream>
#include <string>


#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "WinInet.lib")

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

int ping_server(const char *ip) {
  HANDLE hIcmpFile;
  unsigned long ipaddr = INADDR_NONE;
  DWORD dwRetVal = 0;
  char SendData[32] = "Data Buffer";
  LPVOID ReplyBuffer = NULL;
  DWORD ReplySize = 0;

  ipaddr = inet_addr(ip);
  if (ipaddr == INADDR_NONE) {
    fprintf(stderr, "IP is None\n");
    return 1;
  }

  hIcmpFile = IcmpCreateFile();
  if (hIcmpFile == INVALID_HANDLE_VALUE) {
    fprintf(stderr, "\tUnable to open handle.\n");
    fprintf(stderr, "IcmpCreatefile returned error: %ld\n", GetLastError() );
    return 1;
  }

  ReplySize = sizeof(ICMP_ECHO_REPLY) + sizeof(SendData);
  ReplyBuffer = (VOID*) malloc(ReplySize);
  if (ReplyBuffer == NULL) {
    fprintf(stderr, "\tUnable to allocate memory\n");
    return 1;
  }

  dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, SendData, sizeof(SendData),
      NULL, ReplyBuffer, ReplySize, 1000);
  if (dwRetVal != 0) {
    PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;
    struct in_addr ReplyAddr;
    ReplyAddr.S_un.S_addr = pEchoReply->Address;
    fprintf(stderr, "\tSent icmp message to %s\n", ip);
    if (dwRetVal > 1) {
      fprintf(stderr, "\tReceived %ld icmp message responses\n", dwRetVal);
      fprintf(stderr, "\tInformation from the first response:\n");
    }
    else {
      fprintf(stderr, "\tReceived %ld icmp message response\n", dwRetVal);
      fprintf(stderr, "\tInformation from this response:\n");
    }
    fprintf(stderr, "\t  Received from %s\n", inet_ntoa( ReplyAddr ) );
    fprintf(stderr, "\t  Status = %ld\n", pEchoReply->Status);
    fprintf(stderr, "\t  Roundtrip time = %ld milliseconds\n",
        pEchoReply->RoundTripTime);
  }
  else {
    fprintf(stderr, "\tCall to IcmpSendEcho failed.\n");
    fprintf(stderr, "\tIcmpSendEcho returned error: %ld\n", GetLastError() );
    return 1;
  }
  return 0;
}

std::wstring CharPToWstring(const char* _charP)
{
  return std::wstring(_charP, _charP + strlen(_charP));
}

std::wstring SendHTTPSRequest_GET(const std::wstring& _server, const std::wstring& _page, const std::wstring& _params = L"")
{
  char szData[1024];
  HINTERNET hInternet = ::InternetOpen(TEXT("WinInet"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
  if (hInternet != NULL)
  {
    HINTERNET hConnect = ::InternetConnect(hInternet, _server.c_str(), INTERNET_DEFAULT_HTTPS_PORT, NULL,NULL, INTERNET_SERVICE_HTTP, 0, NULL);
    if (hConnect != NULL)
    {
      std::wstring request = _page + (_params.empty() ? L"" : (L"?" + _params));
      auto dwFlags =
          INTERNET_FLAG_NO_CACHE_WRITE |
          INTERNET_FLAG_KEEP_CONNECTION |
          INTERNET_FLAG_PRAGMA_NOCACHE |
          INTERNET_FLAG_SECURE |
          INTERNET_FLAG_IGNORE_CERT_CN_INVALID |
          INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
      LPCTSTR AcceptTypes[2] = {TEXT("*/*"), NULL};

      HINTERNET hRequest = ::HttpOpenRequest(hConnect, L"GET",
          (LPCWSTR)request.c_str(), NULL, NULL, (LPCTSTR*)AcceptTypes, dwFlags, NULL);

      if (hRequest != NULL)
      {
        BOOL isSend = ::HttpSendRequest(hRequest, NULL, 0, NULL, 0);

        if (isSend)
        {
          for(;;)
          {
            DWORD dwByteRead;
            BOOL isRead = ::InternetReadFile(hRequest, szData, sizeof(szData) - 1, &dwByteRead);

            if (isRead == FALSE || dwByteRead == 0)
              break;

            szData[dwByteRead] = 0;
          }
        }

        ::InternetCloseHandle(hRequest);
      }
      ::InternetCloseHandle(hConnect);
    }
    ::InternetCloseHandle(hInternet);
  }

  return CharPToWstring(szData);
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
        NULL
		);
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
  ping_server("8.8.8.8");
  auto response = SendHTTPSRequest_GET(
      L"www.google.com", L"/", L"");
  std::wcerr << L"https response is " << response << std::endl;
  return 0;
}
