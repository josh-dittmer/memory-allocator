#include <stdio.h>
#include <stdlib.h>

#include "mms.h"

int main(int argc, char* argv[]) {   
    mms_set_program_name("test2");

    if (mms_init() == NULL) {
        printf("[test] Failed to init mms\n");
        return -1;
    }

    int err;
    char* ptrs[16];
    printf("[test2] Filling up memory...\n");
    for (int i = 0; i < 16; i++) {
        ptrs[i] = mms_malloc(32, &err);
        if (ptrs[i] == NULL) {
            printf("[test2] Failed to allocate region!\n");
            return -1;
        }

        mms_memset(ptrs[i], 'A', 32);
    }

    printf("[test2] Memory filled!\n");
    
    getchar();

    printf("[test2] Freeing some pointers...\n");
    mms_free(ptrs[0]);
    mms_free(ptrs[7]);

    printf("[test2] Allocating new region...\n");
    ptrs[0] = mms_malloc(32, &err);
    if (ptrs[0] == NULL) {
        printf("[test2] Failed to allocate region!\n");
        return -1;
    }

    printf("[test2] Setting region...\n");
    if (mms_memset(ptrs[0], 'Z', 32) != 0) {
        printf("[test2] Failed to set region!\n");
        return -1;
    }

    printf("[test] Printing region...\n");
    if (mms_print(ptrs[0], 32) != 0) {
        printf("[test] Failed to print region\n");
        return -1;
    }

    printf("\n[test2] Allocating new region...\n");
    ptrs[7] = mms_malloc(32, &err);
    if (ptrs[7] == NULL) {
        printf("[test2] Failed to allocate region!\n");
        return -1;
    }

    printf("[test2] Setting region...\n");
    if (mms_memset(ptrs[7], 'Y', 32) != 0) {
        printf("[test2] Failed to set region!\n");
        return -1;
    }

    printf("[test2] Printing region...\n");
    if (mms_print(ptrs[7], 32) != 0) {
        printf("[test] Failed to print region\n");
        return -1;
    }

    getchar();

    printf("\n[test2] Freeing all memory...\n");
    for (int i = 0; i < 16; i++) {
        mms_free(ptrs[i]);
    }

    printf("[test2] Done!\n");

    mms_cleanup();

    return 0;
}