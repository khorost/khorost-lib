#pragma once 

#define KHL_LOGGER_CONSOLE                              "console"
#define KHL_LOGGER_COMMON                               "core"
#define KHL_LOGGER_PROFILER                             "profiler"

constexpr auto khl_http_param_authorization = "Authorization";
constexpr auto khl_json_param_duration = "0dur";

constexpr auto time_seconds_in_minute = 60;
constexpr auto time_minutes_in_hour = 60 * time_seconds_in_minute;
constexpr auto time_hours_in_day = 24 * time_minutes_in_hour;
constexpr auto time_day_in_week = 7 * time_hours_in_day;

constexpr auto khl_token_type = "Bearer";
constexpr auto khl_token_append_time = 2 * time_seconds_in_minute;

constexpr auto khl_json_param_access_token = "access_token";
constexpr auto khl_json_param_access_expire = "access_expire";
constexpr auto khl_json_param_delta_access_time = "delta_access_time";
constexpr auto khl_json_param_refresh_token = "refresh_token";
constexpr auto khl_json_param_refresh_expire = "refresh_expire";
constexpr auto khl_json_param_delta_refresh_time = "delta_refresh_time";

#define KHL_SET_TIMESTAMP_MILLISECONDS(json_object, json_tag, json_value)       json_object[json_tag] = khorost::data::epoch_diff(json_value).total_milliseconds()
#define KHL_SET_TIMESTAMP_MICROSECONDS(json_object, json_tag, json_value)       json_object[json_tag] = khorost::data::epoch_diff(json_value).total_microseconds()

#define KHL_SET_CPU_DURATION(json_object, json_tag, now_value) \
    json_object[json_tag] = (boost::posix_time::microsec_clock::universal_time() - now_value).total_microseconds()

#define S2H_PARAM_ACTION_AUTH               "auth"      // авторизация пользователя
#define S2H_PARAM_ACTION_AUTH_CHECK         "ca"        // проверка авторизации
#define S2H_PARAM_ACTION_AUTH_DO            "da"        // подтверждение авторизации
#define S2H_PARAM_ACTION_AUTH_PRE           "ba"        // запрос авторизации
#define S2H_PARAM_ACTION_AUTH_RESET         "ra"        // сброс авторизации
#define S2H_PARAM_ACTION_AUTH_CHANGEPASS    "chpwd"     // изменить пароль

#define S2H_JSON_PONG                       "pong"
#define S2H_PARAM_ACTION_PING               "ping"

#define S2H_PARAM_HUMMAN_JSON               "hjs"

#define S2H_PARAM_QUESTION                  "q"
#define S2H_PARAM_HASH                      "h"
#define S2H_PARAM_ADMIN_PASSWORD            "admpwd"
#define S2H_PARAM_LOGIN_PASSWORD            "loginpwd"
#define S2H_PARAM_PASSWORD                  "pwd"

#define S2H_JSON_AUTH                       "auth"
#define S2H_JSON_NICKNAME                   "nickname"
#define S2H_JSON_SALT                       "salt"
#define S2H_JSON_REASON                     "reason"
#define S2H_JSON_ROLES                      "roles"

#define S2H_DEFAULT_TCP_PORT                7709
