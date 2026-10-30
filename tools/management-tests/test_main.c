#include <stdio.h>

#include "test_util.h"

int tests_run = 0;
int tests_failed = 0;

int main(void)
{
    printf("Running management interface unit tests\n");
    printf("---------------------------------------\n");

    run_time_tests();
    run_schedule_tests();
    run_auth_tests();
    run_events_tests();
    run_status_tests();
    run_menu_tests();

    printf("---------------------------------------\n");
    printf("%d checks, %d failed\n", tests_run, tests_failed);

    return tests_failed == 0 ? 0 : 1;
}
