extern "C"
void kernel_main()
{
    volatile char* vga = (volatile char*)0xB8000;

    const char* text = "LosTacOS entered long mode";

    for (int i = 0; text[i]; i++)
    {
        vga[i * 2] = text[i];
        vga[i * 2 + 1] = 0x0F;
    }

    while (true)
    {
        asm volatile("hlt");
    }
}
