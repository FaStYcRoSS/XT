
void outb(unsigned short port, unsigned char _ch) {
    asm volatile("outb %%al, %%dx"::"d"(port),"a"(_ch));
}

void kernel_main() {

   return;

}
