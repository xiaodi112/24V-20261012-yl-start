/* JSON 生成器单元测试 */
#include <stdio.h>
#include <string.h>
#include "json_builder.h"

extern int g_test_failures;
#define CHECK(cond) do { if(!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); g_test_failures++; } } while(0)

static void test_basic(void)
{
    char buf[256];
    JsonBuilder jb;

    jb_begin(&jb, buf, sizeof(buf));
    jb_add_str(&jb, "pressure_status", "pressure_normal", "2026-08-06 10:00:00");
    jb_add_float(&jb, "pressure", 123.456f, "2026-08-06 10:00:00");
    jb_end(&jb);

    CHECK(strstr(buf, "\"pressure_status\"") != 0);
    CHECK(strstr(buf, "\"pressure_normal\"") != 0);
    CHECK(strstr(buf, "\"pressure\"") != 0);
    CHECK(strstr(buf, "123.46") != 0);
    CHECK(buf[0] == '[');
    CHECK(buf[strlen(buf) - 1] == ']');
}

static void test_small_buffer(void)
{
    char buf[32];
    JsonBuilder jb;

    jb_begin(&jb, buf, sizeof(buf));
    jb_add_str(&jb, "long_id_that_overflows_the_buffer", "value", "ts");
    jb_end(&jb);
    /* 不越界: 必须以 NUL 结尾 */
    CHECK(buf[sizeof(buf) - 1] == '\0');
}

void test_json_all(void)
{
    test_basic();
    test_small_buffer();
}
