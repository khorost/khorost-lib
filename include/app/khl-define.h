#pragma once 

#define KHL_LOGGER_CONSOLE                              "console"
#define KHL_LOGGER_COMMON                               "core"

#define KHL_HTTP_PARAM__AUTHORIZATION   "Authorization"
#define KHL_JSON_PARAM__DURATION "0dur"

#define KHL_SET_TIMESTAMP_MILLISECONDS(json_object, json_tag, json_value)       json_object[json_tag] = khorost::data::epoch_diff(json_value).total_milliseconds()
#define KHL_SET_TIMESTAMP_MICROSECONDS(json_object, json_tag, json_value)       json_object[json_tag] = khorost::data::epoch_diff(json_value).total_microseconds()

#define KHL_SET_CPU_DURATION(json_object, json_tag, now_value) \
    json_object[json_tag] = (boost::posix_time::microsec_clock::universal_time() - now_value).total_microseconds()

#define S2H_PARAM_ACTION_AUTH               "auth"      // ����������� ������������
#define S2H_PARAM_ACTION_AUTH_CHECK         "ca"        // �������� �����������
#define S2H_PARAM_ACTION_AUTH_DO            "da"        // ������������� �����������
#define S2H_PARAM_ACTION_AUTH_PRE           "ba"        // ������ �����������
#define S2H_PARAM_ACTION_AUTH_RESET         "ra"        // ����� �����������
#define S2H_PARAM_ACTION_AUTH_CHANGEPASS    "chpwd"     // �������� ������

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
