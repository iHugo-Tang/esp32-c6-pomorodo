idf_component_register(
    SRCS "pomodoro_application.c" "pomodoro_display_policy.c"
         "pomodoro_settings_editor.c" "pomodoro_statistics_projection.c"
    INCLUDE_DIRS "include"
    REQUIRES pomodoro_core
)
