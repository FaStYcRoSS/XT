#include <xt/user.h>

int main(int argc, char** argv, char** envp);

XTResult __attribute__((weak)) xtMain(XTUserParameters* params) {
    return main(params->argc, params->argv, params->envp);
}