#include <stdio.h>
#include <Windows.h>

int main()
{
  printf("Hello world\n");

  const char *filename = "\\d\\a\\core\\core\\tests\\acceptance\\workdir\\__05_processes_01_matching_process_count_found_cf\\bin\\cf.events.dll";
  DWORD fileAttr;
  fileAttr = GetFileAttributes(filename);
  DWORD err = GetLastError();
  printf("err=%d\n", err);

  char msg[256];
  FormatMessage(
      FORMAT_MESSAGE_FROM_SYSTEM,
      NULL,
      err,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      msg,
      255,
      NULL);
  printf("msg=%s\n", msg);
  if (fileAttr == INVALID_FILE_ATTRIBUTES)
  {
      printf("The file \"%s\" does not exist?\n", filename);
  }

  return 0;
}
