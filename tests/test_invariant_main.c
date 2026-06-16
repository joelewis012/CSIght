#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Simulate the vulnerable buffer copy logic from esp32/main/main.c
 * Since we cannot directly import ESP-IDF specific code in a unit test,
 * we test the invariant that the copy operation must respect buffer bounds */

#define BUF_SIZE 256

static int safe_name_copy(char *buf, size_t buf_size, size_t offset, 
                          const char *name, uint8_t name_len)
{
    /* Security invariant: name_len + 1 must fit in remaining buffer space */
    if (offset >= buf_size || (name_len + 1) > (buf_size - offset)) {
        return -1; /* Reject: would overflow */
    }
    memcpy(&buf[offset], name, name_len + 1);
    return 0;
}

START_TEST(test_ble_name_copy_bounds)
{
    /* Invariant: BLE device name copy must never write beyond buffer bounds */
    char buf[BUF_SIZE];
    
    struct {
        size_t offset;
        uint8_t name_len;
        const char *name;
        int should_succeed;
    } cases[] = {
        /* Exploit case: name_len=255 at offset near end overflows */
        {250, 255, "AAAAAAAAAA", 0},
        /* Boundary: exactly fills buffer (offset=0, len=255) */
        {0, 254, "valid", 1},
        /* Valid: normal short name */
        {0, 10, "DeviceName", 1},
        /* Boundary: offset at end, any copy fails */
        {255, 1, "X", 0},
    };
    int num_cases = sizeof(cases) / sizeof(cases[0]);

    for (int i = 0; i < num_cases; i++) {
        memset(buf, 0, BUF_SIZE);
        int result = safe_name_copy(buf, BUF_SIZE, cases[i].offset, 
                                    cases[i].name, cases[i].name_len);
        if (cases[i].should_succeed) {
            ck_assert_int_eq(result, 0);
        } else {
            ck_assert_int_eq(result, -1);
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_ble_name_copy_bounds);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}