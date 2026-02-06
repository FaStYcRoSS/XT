#include <xt/arch/x86_64.h>

XTResult xtSerialInit();

XTResult xtArchInit() {
    return xtSerialInit();
}