cmake_minimum_required(VERSION 3.16)
project(pomodoro_host_tests C)

enable_testing()

add_executable(
    pomodoro_core_test
    pomodoro_core_test.c
    ../components/pomodoro_core/pomodoro_core.c
    ../components/pomodoro_core/pomodoro_statistics.c
    ../components/pomodoro_core/pomodoro_persistence.c
)
target_include_directories(
    pomodoro_core_test PRIVATE
    ../components/pomodoro_core/include
)
target_compile_features(pomodoro_core_test PRIVATE c_std_11)
target_compile_options(pomodoro_core_test PRIVATE -Wall -Wextra -Werror -pedantic)

add_test(NAME pomodoro_core_test COMMAND pomodoro_core_test)

add_executable(
    pomodoro_application_test
    pomodoro_application_test.c
    ../components/pomodoro_app/pomodoro_application.c
    ../components/pomodoro_app/pomodoro_display_policy.c
    ../components/pomodoro_app/pomodoro_settings_editor.c
    ../components/pomodoro_app/pomodoro_statistics_projection.c
    ../components/pomodoro_core/pomodoro_core.c
    ../components/pomodoro_core/pomodoro_statistics.c
    ../components/pomodoro_core/pomodoro_persistence.c
)
target_include_directories(
    pomodoro_application_test PRIVATE
    ../components/pomodoro_app/include
    ../components/pomodoro_core/include
)
target_compile_features(pomodoro_application_test PRIVATE c_std_11)
target_compile_options(pomodoro_application_test PRIVATE -Wall -Wextra -Werror -pedantic)

add_test(NAME pomodoro_application_test COMMAND pomodoro_application_test)
