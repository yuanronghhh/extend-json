#ifndef __EXTEND_JSON_H__
#define __EXTEND_JSON_H__

#define G_LOG_USE_STRUCTURED 1
#include <glib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define EJ_PAIR_K(pair) (pair->key)

G_BEGIN_DECLS

typedef struct _EJBuffer EJBuffer;
typedef enum _EJ_TYPE EJ_TYPE;

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

enum _EJ_NUMBER_TYPE {
  EJ_DOUBLE,
  EJ_INT,
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

const gchar* EJ_TYPE_NAMES[EJ_RAW];

GLIB_AVAILABLE_IN_ALL void ej_free_value(EJValue *data);
GLIB_AVAILABLE_IN_ALL void ej_free_error(EJError *error);
GLIB_AVAILABLE_IN_ALL void ej_free_buffer(EJBuffer *buffer);
GLIB_AVAILABLE_IN_ALL const gchar *ej_get_data_type_name(EJ_TYPE type);
GLIB_AVAILABLE_IN_ALL EJBool ej_object_get_value(EJObject *data, gchar *name, EJValue **value);

/* reader */
EJBool ej_valid(EJBuffer *buffer, int pos);
gchar *ej_read(EJBuffer *buffer, int pos);
const gchar ej_read_c(EJBuffer *buffer, int pos);
void ej_skip_line(EJBuffer *buffer, int cols, int rows);
EJBool ej_skip_whitespace(EJBuffer *buffer);
EJBool ej_skip_utf8_bom(EJBuffer *const buffer);
void ej_buffer_skip(EJBuffer *buffer, int pos);

EJBool ej_parse_bool(EJBuffer *buffer, EJBool *data);
EJBool ej_parse_array(EJBuffer *buffer, EJArray **data);
EJBool ej_parse_string(EJBuffer *buffer, EJString **data);
EJBool ej_parse_key(EJBuffer *buffer, EJValue **data);
EJBool ej_parse_number(EJBuffer *buffer, EJNumber **data);
EJBool ej_parse_object_pair(EJBuffer *buffer, EJObject *obj, EJObjectPair **data);
EJBool ej_parse_object_props(EJBuffer *buffer, EJObject *object, EJArray **data);
EJBool ej_parse_object(EJBuffer *buffer, EJObject **data);
EJBool ej_parse_value(EJBuffer *buffer, EJValue **data);
void ej_set_error(EJBuffer *buffer, gchar *fmt, ...);
EJError *ej_get_error(EJBuffer *buffer);

GLIB_AVAILABLE_IN_ALL EJBuffer *ej_buffer_new(const gchar *content, size_t len);
GLIB_AVAILABLE_IN_ALL EJArray *ej_value_array_new();
GLIB_AVAILABLE_IN_ALL EJValue *ej_parse(EJError **error, const gchar *content);

GLIB_AVAILABLE_IN_ALL EJBool ej_print_number(EJNumber *data, gchar **buffer);
GLIB_AVAILABLE_IN_ALL EJBool ej_print_bool(EJBool data, gchar **buffer);
GLIB_AVAILABLE_IN_ALL EJBool ej_print_array_value(size_t arrlen, size_t index, EJValue *data, gchar **buffer);
GLIB_AVAILABLE_IN_ALL EJBool ej_print_array(EJArray *data, gchar **buffer);
GLIB_AVAILABLE_IN_ALL EJBool ej_print_object_pair_prop(EJArray *data, gchar **buffer);
GLIB_AVAILABLE_IN_ALL EJBool ej_print_object_pair(EJObjectPair *data, gchar **buffer);
GLIB_AVAILABLE_IN_ALL EJBool ej_print_object(EJObject *data, gchar **buffer);
GLIB_AVAILABLE_IN_ALL EJBool ej_print_value(EJValue *data, gchar **buffer);

EJObject *ej_value_array_new();

G_END_DECLS

#endif
