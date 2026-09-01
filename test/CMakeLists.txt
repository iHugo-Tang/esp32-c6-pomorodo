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
    ../components/pomodoro_app/pomodoro_agent_status.c
    ../components/pomodoro_app/pomodoro_oc1_protocol.c
    ../components/pomodoro_core/pomodoro_core.c
    ../components/pomodoro_core/pomodoro_statistics.c
    ../components/pomodoro_core/pomodoro_persistence.c
)
target_include_directories(
    pomodoro_application_test PRIVATE
    ../components/pomodoro_app
    ../components/pomodoro_app/include
    ../components/pomodoro_core/include
)
target_compile_features(pomodoro_application_test PRIVATE c_std_11)
target_compile_options(pomodoro_application_test PRIVATE -Wall -Wextra -Werror -pedantic)

add_test(NAME pomodoro_application_test COMMAND pomodoro_application_test)

add_executable(
    pomodoro_orientation_test
    pomodoro_orientation_test.c
    ../components/pomodoro_platform/pomodoro_orientation.c
)
target_include_directories(
    pomodoro_orientation_test PRIVATE
    ../components/pomodoro_platform
)
target_compile_features(pomodoro_orientation_test PRIVATE c_std_11)
target_compile_options(pomodoro_orientation_test PRIVATE -Wall -Wextra -Werror -pedantic)

add_test(NAME pomodoro_orientation_test COMMAND pomodoro_orientation_test)

add_executable(
    pomodoro_audio_priority_test
    pomodoro_audio_priority_test.c
    ../components/pomodoro_platform/pomodoro_audio_priority.c
)
target_include_directories(
    pomodoro_audio_priority_test PRIVATE
    ../components/pomodoro_platform/include
)
target_compile_features(pomodoro_audio_priority_test PRIVATE c_std_11)
target_compile_options(
    pomodoro_audio_priority_test PRIVATE -Wall -Wextra -Werror -pedantic)

add_test(NAME pomodoro_audio_priority_test COMMAND pomodoro_audio_priority_test)

add_executable(
    pomodoro_agent_status_test
    pomodoro_agent_status_test.c
    ../components/pomodoro_app/pomodoro_agent_status.c
)
target_include_directories(
    pomodoro_agent_status_test PRIVATE
    ../components/pomodoro_app/include
)
target_compile_features(pomodoro_agent_status_test PRIVATE c_std_11)
target_compile_options(
    pomodoro_agent_status_test PRIVATE -Wall -Wextra -Werror -pedantic)

add_test(NAME pomodoro_agent_status_test COMMAND pomodoro_agent_status_test)

add_executable(
    pomodoro_oc1_protocol_test
    pomodoro_oc1_protocol_test.c
    ../components/pomodoro_app/pomodoro_oc1_protocol.c
)
target_include_directories(
    pomodoro_oc1_protocol_test PRIVATE
    ../components/pomodoro_app/include
)
target_compile_features(pomodoro_oc1_protocol_test PRIVATE c_std_11)
target_compile_options(
    pomodoro_oc1_protocol_test PRIVATE -Wall -Wextra -Werror -pedantic)

add_test(NAME pomodoro_oc1_protocol_test COMMAND pomodoro_oc1_protocol_test)
