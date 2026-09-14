// l4d_boot_main.cpp
// Standalone entry point for the first-boot test.
// Build with the L4DRecompLib static library.
//
// Usage:
//   l4d_boot_test.exe [pe_dumps_dir]
//   Default pe_dumps_dir: C:\Users\Carlos\Projects\L4DRecomp\analysis\pe_dumps

#include "l4d_boot.h"
#include <cstdio>
#include <cstring>
#include <string>

int main(int argc, char* argv[])
{
    std::string peDumpsDir =
        R"(C:\Users\Carlos\Projects\L4DRecomp\analysis\pe_dumps)";

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--pe-dir") == 0 && (i + 1) < argc) {
            peDumpsDir = argv[++i];
        } else if (argv[i][0] != '-') {
            peDumpsDir = argv[i];
        }
    }

    printf("L4D PC Recomp — First Boot Test\n");
    printf("PE dumps directory: %s\n\n", peDumpsDir.c_str());

    bool ok = L4D_Boot(peDumpsDir);
    return ok ? 0 : 1;
}
