/* Host 单元测试入口 */
#include <stdio.h>
#include <string.h>

int g_test_failures = 0;

void test_json_all(void);
void test_sensor_buffer_all(void);
void test_threshold_all(void);
void test_valve_fsm_all(void);
void test_valve_all(void);

int main(void)
{
    printf("== JSON ==\n");      test_json_all();
    printf("== SENSOR BUFFER ==\n"); test_sensor_buffer_all();
    printf("== THRESHOLD ==\n"); test_threshold_all();
    printf("== VALVE FSM ==\n"); test_valve_fsm_all();
    printf("== VALVE (motor contract) ==\n"); test_valve_all();

    if(g_test_failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d TEST(S) FAILED\n", g_test_failures);
    return 1;
}
