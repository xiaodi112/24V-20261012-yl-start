/* Host 单元测试入口 */
#include <stdio.h>
#include <string.h>

int g_test_failures = 0;

void test_json_all(void);
void test_sensor_buffer_all(void);
void test_threshold_all(void);
void test_valve_all(void);

int main(void)
{
    fprintf(stderr, "main start\n");
    printf("== JSON ==\n");      test_json_all();       fprintf(stderr, "json done\n");
    printf("== SENSOR BUFFER ==\n"); test_sensor_buffer_all(); fprintf(stderr, "sbuf done\n");
    printf("== THRESHOLD ==\n"); test_threshold_all();  fprintf(stderr, "thr done\n");
    printf("== VALVE (motor contract) ==\n"); test_valve_all(); fprintf(stderr, "valve done\n");

    if(g_test_failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d TEST(S) FAILED\n", g_test_failures);
    return 1;
}
