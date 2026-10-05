#include "calculator.h"
#include "unity.h"

static Calculator calc;
static const char *plugin_directory = "./build/plugins";

void setUp(void) {
    calc = (Calculator){0};
}

void tearDown(void) {
    calculator_unload_plugins(&calc);
}

static const LoadedOperation *find_loaded_operation(char symbol) {
    for (size_t i = 0; i < calc.operations.size; ++i) {
        const LoadedOperation *loaded = &calc.operations.items[i];
        if (loaded->plugin != NULL && loaded->plugin->symbol == symbol) return loaded;
    }

    return NULL;
}

static void assert_plugin_loaded(char symbol) {
    const LoadedOperation *loaded = find_loaded_operation(symbol);

    TEST_ASSERT_NOT_NULL_MESSAGE(loaded, "Плагин операции не загружен");
    TEST_ASSERT_NOT_NULL_MESSAGE(loaded->library_handle, "Не сохранён дескриптор динамической библиотеки");
    TEST_ASSERT_NOT_NULL_MESSAGE(loaded->plugin->function, "Не загружена функция операции");
}

static void assert_plugin_result(char symbol, double left, double right, double expected) {
    const LoadedOperation *loaded = find_loaded_operation(symbol);

    TEST_ASSERT_NOT_NULL_MESSAGE(loaded, "Плагин операции не загружен");
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, expected, loaded->plugin->function(left, right));
}

static void assert_expression_result(const char *expression, double expected) {
    TEST_ASSERT_EQUAL_INT(CALC_OK, process_command(expression, &calc));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, expected, calc.ans);
}

void test_dynamic_libraries_should_load_all_plugins(void) {
    TEST_ASSERT_TRUE(calculator_load_plugins(&calc, plugin_directory));
    TEST_ASSERT_EQUAL_size_t(5, calc.operations.size);

    assert_plugin_loaded('+');
    assert_plugin_loaded('-');
    assert_plugin_loaded('*');
    assert_plugin_loaded('/');
    assert_plugin_loaded('^');
}

void test_functions_from_dynamic_libraries_should_work(void) {
    TEST_ASSERT_TRUE(calculator_load_plugins(&calc, plugin_directory));

    assert_plugin_result('+', 7.0, 3.0, 10.0);
    assert_plugin_result('-', 7.0, 3.0, 4.0);
    assert_plugin_result('*', 7.0, 3.0, 21.0);
    assert_plugin_result('/', 6.0, 3.0, 2.0);
    assert_plugin_result('^', 2.0, 3.0, 8.0);
}

void test_calculator_should_use_loaded_plugin_functions(void) {
    TEST_ASSERT_TRUE(calculator_load_plugins(&calc, plugin_directory));

    assert_expression_result("2 + 3 * 4", 14.0);
    assert_expression_result("(2 + 3) * 4", 20.0);
    assert_expression_result("2 ^ 3 ^ 2", 512.0);
    TEST_ASSERT_EQUAL_INT(CALC_DIVISION_BY_ZERO, process_command("10 / 0", &calc));
}

void test_dynamic_libraries_should_unload(void) {
    TEST_ASSERT_TRUE(calculator_load_plugins(&calc, plugin_directory));
    TEST_ASSERT_GREATER_THAN_size_t(0, calc.operations.size);

    calculator_unload_plugins(&calc);

    TEST_ASSERT_EQUAL_size_t(0, calc.operations.size);
}

int main(int argc, char **argv) {
    if (argc > 1) plugin_directory = argv[1];

    UNITY_BEGIN();

    RUN_TEST(test_dynamic_libraries_should_load_all_plugins);
    RUN_TEST(test_functions_from_dynamic_libraries_should_work);
    RUN_TEST(test_calculator_should_use_loaded_plugin_functions);
    RUN_TEST(test_dynamic_libraries_should_unload);

    return UNITY_END();
}
