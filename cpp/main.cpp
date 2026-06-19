// BrutePin v1.1 - C++ Wrapper (delegates to C engine)
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <vector>

int main(int argc, char **argv) {
    (void)argc;
    std::vector<const char*> args;
    args.push_back("./core/brutepin");
    for (int i = 1; i < argc; i++) args.push_back(argv[i]);
    args.push_back(nullptr);
    execvp(args[0], (char* const*)args.data());
    perror("execvp");
    return 1;
}
