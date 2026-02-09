#include <xt/arch/x86_64.h>

XTResult xtDTInit();

XTResult xtArchInit() {
    
    return xtDTInit();
}