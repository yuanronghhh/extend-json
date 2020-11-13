#ifndef __EXTEND_JSON_H__
#define __EXTEND_JSON_H__

#define G_LOG_USE_STRUCTURED 1
#include <glib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

G_BEGIN_DECLS

#define EJ_PAIR_K(pair) (pair->key)

#define ej_free_ptr_array(obj) g_ptr_array_unref(obj)
#define ej_free_object(obj) ej_free_ptr_array(obj)
#define ej_free(v) g_free(v)

#define ej_ptr_array_new_with_func(func) g_ptr_array_new_with_free_func((GDestroyNotify)func)
#define ej_str_equal(v1, v2) g_str_equal(v1, v2)
#define ej_assert(v) g_assert(v)
#define ej_ascii_isdigit(c) g_ascii_isdigit(c)
#define ej_ascii_isalnum(c) g_ascii_isalnum(c)
#define ej_ascii_strtod(nstr, endptr) g_ascii_strtod(nstr, endptr)
#define ej_ascii_strtoll(nstr, endptr, base) g_ascii_strtoll(nstr, endptr, base)
#define ej_return_val_if_fail g_return_val_if_fail
#define ej_strcmp0(str1, str2) g_strcmp0(str1, str2)
#define ej_strdup_vprintf g_strdup_vprintf
#define ej_strdup_printf g_strdup_printf
#define ej_strndup(str, len) g_strndup(str, len)
#define ej_string_new(init) g_string_new(init)
#define ej_string_truncate(str, len) g_string_truncate(str, len)
#define ej_ptr_array_foreach(str, func, user_data) g_ptr_array_foreach(str, func, user_data)

#define ej_string_append(str, astr) g_string_append(str, astr)
#define ej_string_free(string, free_segment) g_string_free(string, free_segment)
#define ej_ptr_array_add(array, data) g_ptr_array_add(array, data)
#define ej_strdup(v) g_strdup(v)

#define EJFunc GFunc

G_BEGIN_DECLS

typedef struct _EJBuffer EJBuffer;
typedef enum _EJ_TYPE EJ_TYPE;
typedef enum _EJ_TOKEN_TYPE EJ_TOKEN_TYPE;
typedef enum _EJ_MODE_TYPE EJ_MODE_TYPE;

typedef enum _EJ_NUMBER_TYPE EJ_NUMBER_TYPE;
typedef bool EJBool;
typedef struct _EJValue EJValue;
typedef struct _EJNumber EJNumber;
typedef struct _GPtrArray EJObject;
typedef struct _GHashTable EJHash;
typedef struct _EJObjectPair EJObjectPair;
typedef struct _GPtrArray EJArray;
typedef struct _EJError EJError;
typedef gchar EJString;

typedef struct _EJLString EJLString;

enum _EJ_NUMBER_TYPE {
  EJ_DOUBLE,
  EJ_INT,
};

enum _EJ_MODE_TYPE {
  EJ_MODE_RECURSIVE,
  EJ_MODE_STEP,
};

enum _EJ_TYPE {
  EJ_INVALID = 1,
  EJ_BOOLEAN,
  EJ_STRING,
  EJ_ARRAY,
  EJ_NUMBER,
  EJ_OBJECT,
  EJ_EOBJECT,
  EJ_NULL,
  EJ_RAW,
};

enum _EJ_TOKEN_TYPE {
  EJ_TOKEN_TRUE,
  EJ_TOKEN_FALSE,
  EJ_TOKEN_NULL,
  EJ_TOKEN_CUR_START,
  EJ_TOKEN_BKT_START,
  EJ_TOKEN_COMMA,
  EJ_TOKEN_QMARK,
  EJ_TOKEN_AT,
  EJ_TOKEN_CUR_END,
  EJ_TOKEN_BKT_END,
  EJ_TOKEN_HYPHEN,
  EJ_TOKEN_LT,
  EJ_TOKEN_GT,
  EJ_TOKEN_COLON,
  EJ_TOKEN_DOT,
  EJ_TOKEN_STRING_END,
  EJ_TOKEN_RAW,
  EJ_TOKEN_NUMBER
};

struct _EJError {
  size_t row;
  size_t col;
  gchar *message;
};

struct _EJObjectPair {
  EJValue *key;
  EJArray *props;
  EJValue *value;
};

struct _EJNumber {
  EJ_NUMBER_TYPE type;

  union v {
    int i;
    double d;
  } v;
};

struct _EJValue {
  EJ_TYPE type;

  union value {
    EJObject *object;
    EJArray *array;
    EJBool bvalue;
    EJString *string;
    EJNumber *number;
  } v;
};

struct _EJLString {
  size_t len;
  gchar *value;
};

void* ej_malloc0(size_t size);
void ej_free_value(EJValue *data);
void ej_free_error(EJError *error);
void ej_free_buffer(EJBuffer *buffer);

const gchar *ej_get_data_type_name(EJ_TYPE type);
EJBool ej_object_get_value(EJObject *data, gchar *name, EJValue **value);

/* reader */
EJBool ej_valid(EJBuffer *buffer, int pos);
gchar *ej_read(EJBuffer *buffer, int pos);
gchar ej_read_c(EJBuffer *buffer, int pos);
gchar ej_next_c(EJBuffer *buffer);
void ej_skip_line(EJBuffer *buffer, int cols, int rows);
EJBool ej_skip_whitespace(EJBuffer *buffer);
EJBool ej_skip_utf8_bom(EJBuffer *buffer);
void ej_buffer_skip(EJBuffer *buffer, int pos);
EJBool ej_ensure_char(EJBuffer *buffer, EJ_TOKEN_TYPE ch);

EJBool ej_parse_bool(EJBuffer *buffer, EJBool *data);
EJBool ej_parse_array(EJBuffer *buffer, EJArray **data);
EJBool ej_parse_string(EJBuffer *buffer, EJString **data);
EJBool ej_parse_key_without_quote(EJBuffer *buffer, EJString **data);
EJBool ej_parse_key(EJBuffer *buffer, EJValue **data);
EJBool ej_parse_number(EJBuffer *buffer, EJNumber **data);
EJBool ej_parse_object_pair(EJBuffer *buffer, EJObject *obj, EJObjectPair **data);
EJBool ej_parse_object_props(EJBuffer *buffer, EJObject *object, EJArray **data);
EJBool ej_parse_object(EJBuffer *buffer, EJObject **data);
EJBool ej_parse_value(EJBuffer *buffer, EJValue **data);
void ej_set_error(EJBuffer *buffer, gchar *fmt, ...);
EJError *ej_get_error(EJBuffer *buffer);

EJBuffer *ej_buffer_new(const gchar *content, size_t len);
EJBuffer *ej_buffer_mode_new(const gchar *content, size_t len, EJ_MODE_TYPE mode);
EJArray *ej_value_array_new();
EJArray *ej_pair_array_new();
EJObjectPair *ej_object_pair_new();

EJValue *ej_parse(EJError **error, const gchar *content);

EJBool ej_print_number(EJNumber *data, gchar **buffer);
EJBool ej_print_bool(EJBool data, gchar **buffer);
EJBool ej_print_array_value(size_t arrlen, size_t index, EJValue *data, gchar **buffer);
EJBool ej_print_array(EJArray *data, gchar **buffer);
EJBool ej_print_object_pair_prop(EJArray *data, gchar **buffer);
EJBool ej_print_object_pair(EJObjectPair *data, gchar **buffer);
EJBool ej_print_object(EJObject *data, gchar **buffer);
EJBool ej_print_value(EJValue *data, gchar **buffer);

EJBool ej_token_ensure(EJBuffer *buffer, EJ_TOKEN_TYPE etype);

G_END_DECLS

#endif
