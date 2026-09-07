#include <windows.h>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    HMODULE python = LoadLibraryA(argv[1]);
    if (!python) return 3;
    auto initialize = reinterpret_cast<void(*)()>(GetProcAddress(python, "Py_Initialize"));
    auto run = reinterpret_cast<int(*)(const char*, void*)>(GetProcAddress(python, "PyRun_SimpleStringFlags"));
    auto noSite = reinterpret_cast<int*>(GetProcAddress(python, "Py_NoSiteFlag"));
    if (!initialize || !run || !noSite) return 4;
    *noSite = 1;
    std::ifstream input(argv[2], std::ios::binary);
    if (!input) return 5;
    std::string script((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    initialize();
    return run(script.c_str(), nullptr) == 0 ? 0 : 1;
}
