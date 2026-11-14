#include <orbis/libkernel.h>
#include <orbis/Pad.h>
#include <stdio.h>

int main() {
    int pad = scePadOpen(0, 0, 0, nullptr);
    OrbisPadData data;
    
    printf("PS4 AI-Doctor is Loading...\n");
    
    while (1) {
        if (scePadReadState(pad, &data) == 0) {
            if (data.buttons & ORBIS_PAD_BUTTON_CIRCLE) {
                break; // للخروج
            }
        }
        sceKernelUsleep(100000);
    }
    return 0;
}
