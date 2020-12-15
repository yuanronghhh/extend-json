#include "ExtendJson.h"

#define EJ_DEBUG false
#define EJ_SPECIAL_CHAR '@'
#define EJ_SKIP_AND_VALID(buffer) ej_skip_whitespace(buffer)

#define EJ_STR_MAX (INT_MAX / 2)
#define ej_new0(struct_type, n_structs)  ej_malloc0(sizeof(struct_type) * n_structs)

struct _EJBuffer {
  const gchar *content;
  size_t length;
  size_t offset;
  EJError *error;
};

const gchar* EJ_TYPE_NAMES[EJ_RAW] = { "Invalid", "Boolean", "String", "Array", "Number", "Object", "EObject", "Null", "Raw" };

/* declare */
static void ej_print_value_inner(EJValue *data, gpointer user_data);
static void ej_print_object_pair_inner(EJObjectPair *value, gpointer user_data);
static void ej_print_array_value_inner(size_t arrlen, size_t index, EJValue *data, gpointer user_data);
static void ej_comment(EJBuffer *const buffer);

static inline void ej_assert_object_pair(EJObjectPair *data) {
  g_assert(data != NULL);
  g_assert(data->key != NULL);
  g_assert(data->key->type < EJ_RAW && data->key->type > EJ_INVALID);
  if (data->value != NULL) {
    g_assert(data->value->type < EJ_RAW && data->value->type > EJ_INVALID);
  }
}

static inline void ej_assert_value(EJValue *data) {
  g_assert(data != NULL);
  g_assert(data->type < EJ_RAW && data->type >= EJ_INVALID);
}

gpointer ej_malloc0(size_t size) {
  gpointer pt = g_malloc0(size);
  if (!pt) {
    printf("%s ", "malloc failed");
    abort();
  }
  return pt;
}

size_t ej_strlen(gchar *s, size_t maxlen) {
  size_t len;

  for (len = 0; len < maxlen; len++, s++) {
    if (!*s) {
      break;
    }
  }
  return len;
}

EJBool ej_value_equal(EJValue *v1, EJValue *v2) {
  if (v1->type == EJ_STRING && v2->type == EJ_STRING) {
    return g_str_equal(v1->v.string, v2->v.string);
  }
  else if (v1->type == EJ_NUMBER && v2->type == EJ_NUMBER) {
    return (v1->v.number->v.d - v2->v.number->v.d) == 0;
  }

  return false;
}

void ej_free_number(EJNumber *data) {
  g_assert((data->type == EJ_INT) || (data->type == EJ_DOUBLE));
  g_free(data);
}

void ej_free_value(EJValue *data) {
  ej_assert_value(data);

  if (data->v.bvalue) {
    switch (data->type)
    {
      case EJ_BOOLEAN:
      case EJ_INVALID:
      case EJ_RAW:
      case EJ_NULL: {
        g_free(data);
        break;
      }
      case EJ_STRING: {
        g_free(data->v.string);
        break;
      }
      case EJ_NUMBER: {
        ej_free_number(data->v.number);
        g_free(data);
        break;
      }
      case EJ_ARRAY: {
        g_ptr_array_unref(data->v.array);
        g_free(data);
        break;
      }
      case EJ_EOBJECT:
      case EJ_OBJECT: {
        g_ptr_array_unref(data->v.object);
        g_free(data);
        break;
      }
      default:
        break;
      }

    data = NULL;
  }
}

void ej_free_object_pair(EJObjectPair *data) {
  ej_assert_object_pair(data);

  g_free(data->key);
  if (data->props != NULL) {
    g_ptr_array_unref(data->props);
  }

  if (data->value != NULL) {
    ej_free_value(data->value);
  }
  g_free(data);
}

void ej_free_error(EJError *error) {
  if (error != NULL) {
    g_free(error->message);
    g_free(error);
  }
}

void ej_free_buffer(EJBuffer *buffer) {
  g_free(buffer);
}

EJObjectPair *ej_object_pair_new() {
  EJObjectPair *pair = ej_new0(EJObjectPair, 1);

  return pair;
}

static EJArray *ej_value_array_new() {
  EJArray *arr = g_ptr_array_new_with_free_func((GDestroyNotify)ej_free_value);
  return arr;
}

static EJArray *ej_pair_array_new() {
  EJArray *arr = g_ptr_array_new_with_free_func((GDestroyNotify)ej_free_object_pair);
  return arr;
}

EJError *ej_error_new() {
  EJError *error = ej_new0(EJError, 1);
  error->row = 1;
  error->col = 1;
  error->message = NULL;

  return error;
}

/* reader */
EJBool ej_valid(EJBuffer *buffer, int pos) {
  if (!buffer || ((buffer->offset + pos) < 0) || ((buffer->offset + pos) >= buffer->length)) {
    return false;
  }

  return true;
}

static gchar *ej_read_inner(EJBuffer *buffer, int pos) {
  return (char *)(buffer->content + buffer->offset + pos);
}

gchar *ej_read(EJBuffer *buffer, int pos) {
  return ej_read_inner(buffer, pos);
}

static const gchar ej_read_c_inner(EJBuffer *buffer, int pos) {
  if (!ej_valid(buffer, pos)) {
    return -1;
  }

  return *ej_read_inner(buffer, pos);
}

static EJBool ej_ensure_char(EJBuffer *buffer, char ch) {
  if (!EJ_SKIP_AND_VALID(buffer)) {
    return false;
  }

  if (*ej_read_inner(buffer, 0) == ch) {
    return true;
  }
  
  return false;
}

const gchar ej_read_c(EJBuffer *buffer, int pos) {
  gchar c;
  if (!ej_valid(buffer, pos)) {
    return -1;
  }
  ej_comment(buffer);

  if (!ej_valid(buffer, 0)) {
    return -1;
  }
  c = ej_read_c_inner(buffer, pos);

  return c;
}

void ej_skip_c(EJBuffer *buffer, gchar c) {
  if (c == '\n') {
    ej_skip_line(buffer, 1, 1);
  }
  else {
    ej_skip_line(buffer, 1, 0);
  }
}

void ej_skip_line(EJBuffer *buffer, int cols, int rows) {
  if (rows == 0) {
    buffer->error->col += cols;
  } else {
    buffer->error->col = 1;
#if EJ_DEBUG
    g_print("[new line]%d,%c\n", buffer->error->row, ej_read_c(buffer, -1));
#endif
  }

  buffer->error->row += rows;
  buffer->offset = buffer->offset + cols;
}

void ej_buffer_skip(EJBuffer *buffer, int pos) {
  ej_skip_line(buffer, pos, 0);
}

EJBool ej_skip_whitespace(EJBuffer *buffer) {
  gchar c;
  while ((c = ej_read_c_inner(buffer, 0))) {
    if (c == -1) { return false; }
    if (c == ' ' || c == '\r' || c == '\n') {
      ej_skip_c(buffer, c);
      continue;
    }

    ej_comment(buffer);
    break;
  }

  return true;
}

EJBool ej_skip_utf8_bom(EJBuffer *const buffer) {
  if ((buffer == NULL) || (buffer->content == NULL) || (buffer->offset != 0)) {
    return false;
  }

  if (ej_valid(buffer, 4) && (strncmp(ej_read(buffer, 0), "\xEF\xBB\xBF", 3) == 0)) {
    ej_buffer_skip(buffer, 3);
  }

  return true;
}

static const gchar ej_next_c_inner(EJBuffer *const buffer) {
  if (!ej_valid(buffer, 0)) {
    return -1;
  }

  gchar c = ej_read_c_inner(buffer, 0);
  if (c == -1 || !ej_valid(buffer, 0)) {
    return -1;
  }

  ej_skip_c(buffer, c);
  return c;
}

const gchar ej_next_c(EJBuffer *const buffer) {
  ej_skip_whitespace(buffer);

  return ej_next_c_inner(buffer);
}

static bool ej_comment_line(EJBuffer *const buffer) {
  gchar c;
  while ((c = ej_next_c_inner(buffer)) != -1) {
    if (c == '\n') { break; }
  }

  return true;
}

static bool ej_comment_multiple(EJBuffer *const buffer) {
  gchar c;

  while (c = ej_next_c_inner(buffer)) {
    if (c == -1) { ej_set_error(buffer, "mutiple line comment not close"); return false; }
    if (c != '*') { continue; }

    c = ej_read_c_inner(buffer, 0);
    if (c == -1) { ej_set_error(buffer, "mutiple line comment not close"); return false; }

    if (c == '/') { ej_buffer_skip(buffer, 1); break; }
  }

  return true;
}

static void ej_comment(EJBuffer *const buffer) {
  gchar c;

  c = ej_read_c_inner(buffer, 0);
  if (c == '/') {
    c = ej_read_c_inner(buffer, 1);

    if (c == '/') {
      ej_buffer_skip(buffer, 2);
      ej_comment_line(buffer);

    } else if (c == '*') {
      ej_buffer_skip(buffer, 2);
      ej_comment_multiple(buffer);
    }
  } else {
    return;
  }

  ej_skip_whitespace(buffer);
}

const gchar *ej_get_data_type_name(EJ_TYPE type) {
  if (type < 0 || type > EJ_RAW) {
    return NULL;
  }

  return EJ_TYPE_NAMES[type];
}

EJBool ej_object_get_value(EJObject *data, gchar *key, EJValue **value) {
  size_t i;
  EJObjectPair *pair = NULL;

  for (i = 0; i < data->len; i++) {
    pair = (EJObjectPair *)data->pdata[i];
    g_return_val_if_fail((pair != NULL) && (pair->key != NULL) && (pair->value != NULL), false);
    
    if (pair->key->type != EJ_STRING) {
      continue;
    }

    if (g_strcmp0(key, pair->key->v.string) == 0) {
      *value = pair->value;
      return true;
    }
  }

  return false;
}

void ej_set_errorv(EJBuffer *buffer, gchar *fmt, va_list args) {
  buffer->error->message = g_strdup_vprintf(fmt, args);
  va_end(args);
}

void ej_set_error(EJBuffer *buffer, gchar *fmt, ...) {
  va_list args;

  g_assert(buffer != NULL && buffer->error != NULL && "buffer error should not be NULL");

  if (buffer->error->message != NULL) {
    return;
  }

  va_start(args, fmt);
  buffer->error->message = g_strdup_vprintf(fmt, args); 
  va_end(args);

#if EJ_DEBUG
  g_warning("Error in <%d, %d>: %s", buffer->error->row, buffer->error->col, buffer->error->message);
#endif 
}

EJError *ej_get_error(EJBuffer *buffer) {
  return buffer->error;
}

/* print */
EJBool ej_print_number(EJNumber *data, gchar **buffer) {
  switch (data->type)
  {
    case EJ_INT:
      *buffer = g_strdup_printf("%d", data->v.i);
      break;
    case EJ_DOUBLE:
      *buffer = g_strdup_printf("%lf", data->v.d);
      break;
    default:
      break;
  }

  return true;
}

EJBool ej_print_bool(EJBool data, gchar **buffer) {
  *buffer = data ? g_strdup("true"): g_strdup("false");
  return true;
}

EJBool ej_print_array_value(size_t arrlen, size_t index, EJValue *data, gchar **buffer) {
  GString *value;
  gchar *str = NULL;

  g_assert(index < arrlen);

  value = g_string_new("");
  if (ej_print_value(data, &str)) {
    value = g_string_append(value, str); g_free(str);
  }

  if (index + 1 == arrlen) {
    *buffer = value->str;
  }
  else {
    value = g_string_append(value, ",");
    *buffer = value->str;
  }
  g_string_free(value, false);

  return true;
}

EJBool ej_print_array(EJArray *data, gchar **buffer) {
  GString *value;
  gchar *str;
  size_t i, len;
  if (!data) { return false; }

  value = g_string_new("[");
  len = data->len;
  for (i = 0; i < len; i++) {
    ej_print_array_value_inner(len, i, data->pdata[i], &str);
    value = g_string_append(value, str); g_free(str);
  }
  value = g_string_append(value, "]");
  *buffer = value->str; g_string_free(value, false);

  return true;
}

EJBool ej_print_object_pair_prop(EJArray *data, gchar **buffer) {
  gchar *key = NULL, *prop = NULL;
  EJObjectPair *op = NULL;
  GString *value;
  guint size, i;

  if (!data) { return false; }

  value = g_string_new("<");
  size = data->len;

  for(i = 0; i < data->len;i++) {
    op = data->pdata[i];

    if (ej_print_object_pair(op, &prop)) {
      value = g_string_append(value, prop);g_free(prop);

      if(i + 1 < size) {
        value = g_string_append(value, ",");
      }
    }
  }
  value = g_string_append(value, ">");

  *buffer = value->str; g_string_free(value, false);

  return true;
}

EJBool ej_print_object_pair(EJObjectPair *data, gchar **buffer) {
  g_return_val_if_fail(data->key != NULL, false);

  gchar *prop = NULL, *value = NULL, *key = NULL;

  if (!ej_print_value(data->key, &key)) {
    goto fail;
  }

  if (!ej_print_object_pair_prop(data->props, &prop)) {
    prop = g_strdup("");
  }

  if (!ej_print_value(data->value, &value)) {
    value = g_strdup("");
  }

  *buffer = g_strdup_printf("%s%s:%s", key, prop, value);

  g_free(key);
  g_free(prop);
  g_free(value);
  return true;
fail:
  return false;
}

EJBool ej_print_eobject(EJObject *data, gchar **buffer) {
  GString *value;

  if (!data) { return false; }

  value = g_string_new("@");
  if (!ej_print_object(data, buffer)) {
    return false;
  }

  value = g_string_append(value, *buffer); g_free(*buffer);
  *buffer = value->str; g_string_free(value, false);

  return true;
}

EJBool ej_print_object(EJObject *data, gchar **buffer) {
  GString *value;

  if (!data) { return false; }

  value = g_string_new("{");

  if (data->len > 0) {
    g_ptr_array_foreach(data, (GFunc)ej_print_object_pair_inner, buffer);
    if(*buffer == NULL) { return false; }

    value = g_string_append(value, *buffer); g_free(*buffer);
    value = g_string_truncate(value, (value->len-1)); // delete last ','
  }

  value = g_string_append(value, "}");

  *buffer = value->str; g_string_free(value, false);

  return true;
}

EJBool ej_print_value(EJValue *data, gchar **buffer) {
  if (!data) { return false; };

  switch (data->type)
  {
    case EJ_BOOLEAN:
      return ej_print_bool(data, buffer);
    case EJ_NULL: {
      *buffer = g_strdup("null");
      return true;
    }
    case EJ_STRING: {
      *buffer = g_strdup_printf("\"%s\"", data->v.string);
      return true;
    }
    case EJ_NUMBER: {
      return ej_print_number(data->v.number, buffer);
    }
    case EJ_ARRAY: {
      return ej_print_array(data->v.array, buffer);
    }
    case EJ_EOBJECT:
      return ej_print_eobject(data->v.object, buffer);
    case EJ_OBJECT: {
      return ej_print_object(data->v.object, buffer);
    }
    case EJ_INVALID:
    case EJ_RAW:
    default:
      return false;
  }
}

static void ej_print_value_inner(EJValue *data, gpointer user_data) {
  ej_print_value(data, user_data);
}

static void ej_print_object_pair_inner(EJObjectPair *pair, gpointer user_data) {
  gchar *str = NULL;
  gchar **udata;
  GString *value;

  udata = (gchar **)user_data;
  if(*udata != NULL) {
    value = g_string_new(*udata); g_free(*udata);
  }
  else {
    value = g_string_new("");
  }

  if(!ej_print_object_pair(pair, &str)) {
    return;
  }

  value = g_string_append(value, str); g_free(str);
  value = g_string_append(value, ",");

  *udata = (gpointer)value->str;
  g_string_free(value, false);
}

static void ej_print_array_value_inner(size_t arrlen, size_t index, EJValue *data, gpointer user_data) {
  ej_print_array_value(arrlen, index, data, user_data);
}

/* parse */
EJBool ej_parse_bool(EJBuffer *buffer, EJBool *data) {
  g_return_val_if_fail(data != NULL, false);

  if (ej_valid(buffer, 5) && strncmp(ej_read(buffer, 0), "false", 5) == 0) {
    *data = false;
  }
  else if (ej_valid(buffer, 4) && strncmp(ej_read(buffer, 0), "true", 4) == 0) {
    *data = true;
  }
  else {
    return false;
  }

  return true;
}

static EJBool ej_parse_array_inner(EJBuffer *buffer, EJArray **data) {
  EJArray *arr;
  EJValue *value = NULL;
  size_t i = 0;
  gchar c;

  ej_buffer_skip(buffer, 1);
  ej_skip_whitespace(buffer);

  arr = ej_value_array_new();

  c = ej_read_c(buffer, 0);
  if (c == ']') { goto success; }

  while (true) {
    if (!ej_parse_value(buffer, &value)) {
      goto fail;
    }
    g_ptr_array_add(arr, (gpointer)value);

    ej_skip_whitespace(buffer);

    c = ej_read_c(buffer, 0);
    if (c == ',') {
      ej_buffer_skip(buffer, 1);

      EJ_SKIP_AND_VALID(buffer);
      if (*ej_read_inner(buffer, 0) == ']') {
        break;
      }
    } 
    else if (c == ']') {
      break;
    }
    else {
      goto fail;
    }
  }

success:
  if (*ej_read_inner(buffer, 0) != ']') {
    goto fail;
  }
  ej_buffer_skip(buffer, 1);

  *data = arr;
  return true;
fail:
  ej_set_error(buffer, "Parse array failed");
  g_ptr_array_unref(arr);
  return false;
}

EJBool ej_parse_array(EJBuffer *buffer, EJArray **data) {
  if (*ej_read_inner(buffer, 0) != '[') {
    return false;
  }

  return ej_parse_array_inner(buffer, data);
}

static EJBool ej_parse_string_inner(EJBuffer *buffer, EJString **data) {
  size_t len = 0;
  gchar c, n;

  ej_buffer_skip(buffer, 1);

  for (len = 0; c = ej_read_c(buffer, (int)len); len++) {
    if (c == -1) {
      ej_set_error(buffer, "occur buffer end when parse string");
      return false;
    }

    if (c == '\\') {
      n = ej_read_c(buffer, len + 1);

      switch (n)
      {
      case 'b':
        break;
      case 'f':
        break;
      case 'n':
        break;
      case 'r':
        break;
      case 't':
        break;
      case '\"':
      case '\\':
      // case 'u':
      //   break;
      case '/':
        len = len + 1;
        break;
      default:
        ej_set_error(buffer, "occur not support escaped char %c when parse string", n);
        return false;
      }
    }

    if (c == '\"' || len + 1 > EJ_STR_MAX) {
      break;
    }
  }

  *data = g_strndup(ej_read(buffer, 0), len);

  ej_buffer_skip(buffer, len + 1);
  return true;
}

EJBool ej_parse_string(EJBuffer *buffer, EJString **data) {
  if (*ej_read_inner(buffer, 0) != '\"') {
    return false;
  }

  return ej_parse_string_inner(buffer, data);
}

EJBool ej_parse_key(EJBuffer *buffer, EJValue **data) {
  if (!EJ_SKIP_AND_VALID(buffer)) { return false; }
  if (*ej_read_inner(buffer, 0) == '}') { return false; }

  EJValue *kv;
  gchar c;
  size_t pos = 0;

  kv = ej_new0(EJValue, 1);
  if (*ej_read_inner(buffer, 0) == EJ_SPECIAL_CHAR) {
    ej_buffer_skip(buffer, 1);

    kv->type = EJ_EOBJECT;
    if (ej_parse_object(buffer, &kv->v.object)) {
      goto success;
    }
  }

  if (ej_parse_string(buffer, &kv->v.string)) {
    kv->type = EJ_STRING;
    goto success;
  }

  for (pos = 0; c = ej_read_c(buffer, pos); pos++) {
    if (c == -1 || (!g_ascii_isalnum(c) && c != '-' && c != '_')) {
      if (pos == 0) {
        ej_set_error(buffer, "Key length cannot be zero.");
        return false;
      }
      break;
    }

    if (pos + 1 > EJ_STR_MAX) {
      ej_set_error(buffer, "Key position %c length %zu too long.", c, pos + 1);
      return false;
    }
  }

  kv->type = EJ_STRING;
  kv->v.string = g_strndup(ej_read(buffer, 0), pos);
  ej_buffer_skip(buffer, pos);

  if (!ej_valid(buffer, 0)) {
    ej_set_error(buffer, "Occour buffer end when parse key.");
    goto fail;
  }

success:
  *data = kv;
  return true;
fail:
  ej_free_value(kv);
  return false;
}

static EJBool ej_parse_number_inner(EJBuffer *buffer, EJNumber **data) {
  EJNumber *num;
  size_t len;
  gchar c;
  gchar *nstr;

  num = ej_new0(EJNumber, 1);
  size_t type = EJ_INT;
  for (len = 0; c = ej_read_c(buffer, len); len++) {
    if (c == '.') {
      if (type == EJ_DOUBLE) { goto fail; }
      type = EJ_DOUBLE;
    }
    else if ((c >= '0' && c <= '9')
      || c == '+'
      || c == '-'
      || c == 'E'
      || c == 'e') {

    }
    else {
      break;
    }
  }
  nstr = g_strndup(ej_read(buffer, 0), len);

  if (type == EJ_DOUBLE) {
    num->v.d = g_ascii_strtod(nstr, NULL);
  }
  else if (type == EJ_INT) {
    num->v.i = (int)g_ascii_strtoll(nstr, NULL, 10);
  }
  g_free(nstr);

  ej_buffer_skip(buffer, len);

  num->type = type;
  *data = num;
  return true;
fail:
  ej_set_error(buffer, "Parse number failed");
  g_free(num);
  return false;
}

EJBool ej_parse_number(EJBuffer *buffer, EJNumber **data) {
  if (*ej_read_inner(buffer, 0) != '-' && (*ej_read_inner(buffer, 0) < '0' && *ej_read_inner(buffer, 0) > '9')) {
    return false;
  }

  return ej_parse_number_inner(buffer, data);
}

EJBool ej_parse_object_props(EJBuffer *buffer, EJObject *object, EJArray **data) {
  gchar c;
  EJArray *props = NULL;
  EJObjectPair *pair = NULL;

  g_assert(ej_valid(buffer, 0) && ((*ej_read_inner(buffer, 0) == '<')) && (object != NULL));
  ej_buffer_skip(buffer, 1);
  if (!EJ_SKIP_AND_VALID(buffer)) { return false; }

  props = ej_pair_array_new();
  if (*ej_read_inner(buffer, 0) == '>') {
    goto success;
  }

  while (c = ej_read_c(buffer, 0)) {
    if(c == -1) { goto fail; }

    pair = ej_object_pair_new();
    /* parse key */
    if (!ej_parse_key(buffer, &pair->key)) {
      goto fail;
    }

    if (*ej_read_inner(buffer, 0) == '<') {
      if (!ej_parse_object_props(buffer, props, &pair->props)) {
        ej_set_error(buffer, "Parse property failed");
        goto fail;
      }
    }

    if (!ej_ensure_char(buffer, ':')) {
      ej_set_error(buffer, "Missing ':' before parse key property value");
      goto fail;
    }
    ej_buffer_skip(buffer, 1);

    /* parse value */
    if (!ej_parse_value(buffer, &pair->value)) {
      ej_set_error(buffer, "Parse property value failed");
      goto fail;
    }

    if (!EJ_SKIP_AND_VALID(buffer)) {
      ej_free_object_pair(pair);
      goto fail;
    }

    g_ptr_array_add(props, pair);
    if (*ej_read_inner(buffer, 0) == ',') {
      ej_buffer_skip(buffer, 1);

      EJ_SKIP_AND_VALID(buffer);
      if (*ej_read_inner(buffer, 0) == '>') {
        break;
      }
    } 
    else if (*ej_read_inner(buffer, 0) == '>') {
      break;

    } else {
      goto fail;
    }
  }

success:
  if (*ej_read_inner(buffer, 0) != '>') {
    goto fail;
  }
  ej_buffer_skip(buffer, 1);

  *data = props;
  return true;
fail:
  g_ptr_array_unref(props);
  return false;
}

EJBool ej_parse_object_pair(EJBuffer *buffer, EJObject *obj, EJObjectPair **data) {
  EJObjectPair *pair;
  gchar c;

  c = ej_read_c(buffer, 0);
  if (c == -1) { return false; }

  pair = ej_object_pair_new();
  /* parse key */
  if (!ej_parse_key(buffer, &pair->key)) {
    goto fail;
  }

  if (*ej_read_inner(buffer, 0) == '<') {
    if (!ej_parse_object_props(buffer, obj, &pair->props)) {
      goto fail;
    }
  }

  if (!ej_ensure_char(buffer, ':')) {
    ej_set_error(buffer, "Missing ':' before parse object value");
    goto fail;
  }
  ej_buffer_skip(buffer, 1);

  /* parse value */
  if (!ej_parse_value(buffer, &pair->value)) {
    goto fail;
  }

  *data = pair;
  return true;
fail:
  ej_free_object_pair(pair);
  return false;
}

static EJBool ej_parse_object_inner(EJBuffer *buffer, EJObject **data) {
  EJObject *obj = NULL;
  EJObjectPair *pair = NULL;
  gchar c;

  ej_buffer_skip(buffer, 1);
  if (!EJ_SKIP_AND_VALID(buffer)) { return false; }

  obj = ej_pair_array_new();
  if (*ej_read_inner(buffer, 0) == '}') {
    goto success;
  }

  while (c = ej_read_c(buffer, 0)) {
    if (c == -1) { goto fail; }

    if (!ej_parse_object_pair(buffer, obj, &pair)) {
      goto fail;
    }

    if (!EJ_SKIP_AND_VALID(buffer)) {
      ej_free_object_pair(pair);
      goto fail;
    }

    g_ptr_array_add(obj, pair);
    if (*ej_read_inner(buffer, 0) == ',') {
      ej_buffer_skip(buffer, 1);

      EJ_SKIP_AND_VALID(buffer);
      if (*ej_read_inner(buffer, 0) == '}') {
        break;
      }
    } 
    else if (*ej_read_inner(buffer, 0) == '}') {
      break;

    } else {
      ej_set_error(buffer, "Missing ',' before when parse object");
      goto fail;
    }

  }

success:
  if (*ej_read_inner(buffer, 0) != '}') {
    ej_set_error(buffer, "Not end with } when parse object");
    goto fail;
  }
  ej_buffer_skip(buffer, 1);

  *data = obj;
  return true;
fail:
  g_ptr_array_unref(obj);

  return false;
}

EJBool ej_parse_object(EJBuffer *buffer, EJObject **data) {
  if (!ej_valid(buffer, 0) || (*ej_read_inner(buffer, 0) != '{')) {
    return false;
  }

  return ej_parse_object_inner(buffer, data);
}

EJBool ej_parse_value(EJBuffer *buffer, EJValue **data) {
  EJValue *value = ej_new0(EJValue, 1);

  value->type = EJ_RAW;
  g_assert(data != NULL && buffer != NULL && buffer->content != NULL);

  if (!EJ_SKIP_AND_VALID(buffer)) { return false; }

  if (ej_parse_bool(buffer, &value->v.bvalue)) {
    *data = value;
    value->type = EJ_BOOLEAN;
    ej_buffer_skip(buffer, (value->v.bvalue ? 4 : 5));
    return true;
  }
  else if (ej_valid(buffer, 4) && strncmp(ej_read(buffer, 0), "null", 4) == 0) {
    *data = value;
    value->type = EJ_NULL;
    ej_buffer_skip(buffer, 4);
    return true;
  }

  if (ej_valid(buffer, 0)) {
    if (*ej_read_inner(buffer, 0) == '-' || (*ej_read_inner(buffer, 0) >= '0' && *ej_read_inner(buffer, 0) <= '9')) {
      value->type = EJ_NUMBER;
      if (!ej_parse_number_inner(buffer, &value->v.number)) {
        goto fail;
      }
    }
    else if (*ej_read_inner(buffer, 0) == '\"') {
      value->type = EJ_STRING;
      if (!ej_parse_string_inner(buffer, &value->v.string)) {
        goto fail;
      }
    }
    else if (*ej_read_inner(buffer, 0) == '[') {
      value->type = EJ_ARRAY;
      if (!ej_parse_array_inner(buffer, &value->v.array)) {
        goto fail;
      }
    }
    else if (*ej_read_inner(buffer, 0) == '{') {
      value->type = EJ_OBJECT;
      if (!ej_parse_object_inner(buffer, &value->v.object)) {
        goto fail;
      }
    }
    else if(*ej_read_inner(buffer, 0) == EJ_SPECIAL_CHAR) {
      value->type = EJ_EOBJECT;
      EJ_SKIP_AND_VALID(buffer);

      ej_buffer_skip(buffer, 1);
      if (!ej_parse_object_inner(buffer, &value->v.object)) {
        goto fail;
      }
    }
    else {
      value->type = EJ_INVALID;
      ej_set_error(buffer, "Value should starts with '[' or '{' or '\"' or boolean");
      goto fail;
    }
  }

  *data = value;
  return true;
fail:
  ej_set_error(buffer, "Parse value failed");
  ej_free_value(value);
  return false;
}

EJBuffer *ej_buffer_new(const gchar *content, size_t len) {
  g_return_val_if_fail(content != NULL, NULL);

  EJBuffer *buffer = ej_new0(EJBuffer, 1);

  buffer->content = (gchar *)content;
  buffer->length = len;
  buffer->offset = 0;
  buffer->error = ej_error_new();

  return buffer;
}

EJValue *ej_parse(EJError **error, const gchar *content) {
  EJBuffer *buffer;
  EJValue *value = NULL;

  buffer = ej_buffer_new(content, ej_strlen((gchar *)content, EJ_STR_MAX));
  ej_skip_utf8_bom(buffer);

  if (!ej_parse_value(buffer, &value)) {
    *error = buffer->error;
    ej_free_buffer(buffer);
    return NULL;
  }

  g_debug("%s", "Parse success");
  return value;
}
