#if _WIN32

#include <windows.h>

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    return 0;
}

#else 

int main(int argc, char* argv[])
{
    // Entry point for non-Windows platforms
    return 0;
}

#endif
