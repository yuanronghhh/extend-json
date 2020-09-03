#ifndef __JSON_EXTEND_H__
#define __JSON_EXTEND_H__

#define G_LOG_USE_STRUCTURED 1
#include <glib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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
  EJ_NULL,
  EJ_RAW,
};

struct _EJObjectPair {
  gchar *key;
  EJHash *props;
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

void ej_free_number(EJNumber *data);
void ej_free_value(EJValue *data);
void ej_free_object_pair(EJObjectPair *data);

EJBool ej_parse_bool(EJBuffer *buffer, EJBool *data);
EJBool ej_parse_array(EJBuffer *buffer, EJArray **data);
EJBool ej_parse_string(EJBuffer *buffer, EJString **data);
EJBool ej_parse_key(EJBuffer *buffer, EJString **data);
EJBool ej_parse_number(EJBuffer *buffer, EJNumber **data);
EJBool ej_parse_object_props(EJBuffer *buffer, EJHash **data);
EJBool ej_parse_object(EJBuffer *buffer, EJObject **data);
EJBool ej_parse_value(EJBuffer *buffer, EJValue **data);
EJValue *ej_parse(const gchar *content);

EJBool ej_print_number(EJNumber *data, gchar **buffer);
EJBool ej_print_bool(EJBool data, gchar **buffer);
EJBool ej_print_array_value(size_t arrlen, size_t index, EJValue *data, gchar **buffer);
EJBool ej_print_array(EJArray *data, gchar **buffer);
EJBool ej_print_object_pair_prop(EJHash *data, gchar **buffer);
EJBool ej_print_object_pair(EJObjectPair *data, gchar **buffer);
EJBool ej_print_object(EJObject *data, gchar **buffer);
EJBool ej_print_value(EJValue *data, gchar **buffer);

EJObject *ej_array_new();

G_END_DECLS

#endif
