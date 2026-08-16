idf_component_register(
    SRCS "pomodoro_platform_stub.c" "pomodoro_nvs_journal.c"
    INCLUDE_DIRS "include"
    REQUIRES pomodoro_core nvs_flash
)
