#include <xt/arch/x86_64.h>

XTResult xtDTInit();
XTResult xtEarlyRandomInit();

XTResult xtArchInit() {
    XT_TRY(xtDTInit());
    XT_TRY(xtSyscallInit());
    XT_TRY(xtEarlyRandomInit());
    return XT_SUCCESS;
}