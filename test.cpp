#include <stdio.h>
#include <stdlib.h>

#include "mms.h"

int main(int argc, char* argv[]) {    
    mms_set_program_name("test");

    if (mms_init() == NULL) {
        printf("[test] Failed to init mms\n");
        return -1;
    }

    int err;
    char* test_region = mms_malloc(64, &err);
    if (test_region == NULL) {
        printf("[test] Failed to allocate test region\n");
        return -1;
    }

    if (mms_memset(test_region, 'A', 64) != 0) {
        printf("[test] Failed to set test region\n");
        return -1;
    }

    printf("[test] Printing test region...\n");
    if (mms_print(test_region+2, 62) != 0) {
        printf("[test] Failed to print test region\n");
        return -1;
    }
    printf("\n[test] Done!\n");

    getchar();

    char* test_region2 = mms_malloc(31, &err);
    if (test_region2 == NULL) {
        printf("[test] Failed to allocate test region\n");
        return -1;
    }

    if (mms_memset(test_region2, 'B', 32) != 0) {
        printf("[test] Failed to set test region\n");
        return -1;
    }

    if ((err = mms_memcpy(test_region2, test_region, 13)) != 0) {
        printf("[test] Failed to memcpy test region (%d)\n", err);
        return -1;
    }

    printf("[test] Printing test region...\n");
    if (mms_print(test_region2, 32) != 0) {
        printf("[test] Failed to print test region\n");
        return -1;
    }
    printf("\n[test] Done!\n");

    char* external_test_region = (char*)malloc(26);
    if ((err = mms_memcpy(external_test_region, test_region2 + 10, 22)) != 0) {
        printf("[test] Failed to memcpy test region to external buf (%d)\n", err);
        return -1;
    }

    printf("[test] Printing external test region...\n");
    if (mms_print(external_test_region, 26) != 0) {
        printf("[test] Failed to print test region\n");
        return -1;
    }
    printf("\n[test] Done!\n");

    printf("[test] Printing external memory...\n");
    if (mms_print("Test test test\n", 16) != 0) {
        printf("[test] Failed to print external memory\n");
        return -1;
    }
    printf("\n[test] Done!\n");

    printf("[test] Freeing test region 1...\n");
    mms_free(test_region);
    printf("[test] Freeing test region 2...\n");
    mms_free(test_region2);

    mms_cleanup();

    return 0;
}