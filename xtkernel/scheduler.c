#include <xt/scheduler.h>
void xtSwitchTo(XTThread* prev, XTThread* next);

void xtSchedule() {
    XTThread* current = NULL;
    xtGetCurrentThread(&current);
    XTThread* gFirstThread = NULL;
    asm volatile("mov %%gs:8, %%rax":"=a"(gFirstThread));
    if (current == NULL)
        xtKernelPanic("Current is NULL", NULL, XT_ACCESS_VIOLATION);
    if (current->flags == 1) {
        --current->ticks;
        if (current->ticks > 0) return;
    }
    current->ticks = 1;
    XTThread* prev = current;
    // переключаемся на следующий, если есть, иначе на первый
    if (current->nextInQueue != NULL)
        current = current->nextInQueue;
    else
        current = gFirstThread;
    xtSwitchTo(prev, current);
}


void xtStartScheduler() {
    XTThread* gFirstThread = NULL;
    asm volatile("mov %%gs:8, %%rax\n":"=a"(gFirstThread));
    if (gFirstThread == NULL)
        xtKernelPanic("No threads to schedule", NULL, XT_ACCESS_VIOLATION);
    xtSetCurrentThread(gFirstThread);
    XTThread* current = gFirstThread;
    XTThread dummy = {0};   // фиктивный предыдущий поток
    xtSwitchTo(&dummy, current);
    // сюда никогда не вернёмся
}
