#include "ExtendJson.h"

#define EJ_DEBUG false
#define EJ_LSTR(str) {sizeof(str) - 1, (EJString *)str}
#define EJ_STR_MAX (INT_MAX - 2)
#define ej_new0(struct_type, n_structs)  ej_malloc0(sizeof(struct_type) * n_structs)

struct _EJBuffer {
  const EJString *content;
  size_t length;
  size_t offset;
  EJError *error;
  EJ_MODE_TYPE mode;
};

static const EJString* EJ_TYPE_NAMES[EJ_RAW] = {
  "Invalid", "Boolean", "String", "Array",
  "Number", "Object", "EObject", "Null"
  "Raw"
};

static const EJLString EJ_TOKEN_STR[EJ_TOKEN_RAW] = {
  EJ_LSTR("true"), EJ_LSTR("false"), EJ_LSTR("null"), EJ_LSTR("{"), EJ_LSTR("["),
  EJ_LSTR(","), EJ_LSTR("\""), EJ_LSTR("@"), EJ_LSTR("}"), EJ_LSTR("]"), EJ_LSTR("-"),
  EJ_LSTR("<"), EJ_LSTR(">"), EJ_LSTR(":"), EJ_LSTR("."), EJ_LSTR("\0")
};

/* declare */
static void ej_print_value_inner(EJValue *data, gpointer user_data);
static void ej_print_object_pair_inner(EJObjectPair *value, gpointer user_data);
static void ej_print_array_value_inner(size_t arrlen, size_t index, EJValue *data, gpointer user_data);
static void ej_comment(EJBuffer *buffer);

static inline void ej_assert_object_pair(EJObjectPair *data) {
  ej_assert(data != NULL);
  if (data->key != NULL) {
    ej_assert(data->key->type < EJ_RAW && data->key->type > EJ_INVALID);
  }
  if (data->value != NULL) {
    ej_assert(data->value->type < EJ_RAW && data->value->type > EJ_INVALID);
  }
}

static inline void ej_assert_value(EJValue *data) {
  ej_assert(data != NULL);
  ej_assert(data->type < EJ_RAW && data->type >= EJ_INVALID);
}

EJ_MODULE_EXPORT(void*) ej_malloc0(size_t size) {
  void* pt = g_malloc0(size);

  if (!pt) {
    printf("%s ", "malloc failed");
    abort();
  }
  return pt;
}

static size_t ej_strlen(const EJString *s) {
  size_t len;

  for (len = 0; len < EJ_STR_MAX; len++, s++) {
    if (!*s) {
      break;
    }
  }
  return len;
}

EJBool ej_value_equal(EJValue *v1, EJValue *v2) {
  if (v1->type == EJ_STRING && v2->type == EJ_STRING) {
    return ej_str_equal(v1->v.string, v2->v.string);
  }
  else if (v1->type == EJ_NUMBER && v2->type == EJ_NUMBER) {
    return (v1->v.number->v.d - v2->v.number->v.d) == 0;
  }

  return false;
}

void ej_free_number(EJNumber *data) {
  ej_assert((data->type == EJ_INT) || (data->type == EJ_DOUBLE));
  ej_free(data);
}

EJ_MODULE_EXPORT(void) ej_free_value(EJValue *data) {
  ej_assert_value(data);

  if(data->v.object != NULL) {
    switch (data->type) {
      case EJ_BOOLEAN:
      case EJ_INVALID:
      case EJ_RAW:
      case EJ_NULL: {
        break;
      }
      case EJ_STRING: {
        ej_free(data->v.string);
        break;
      }
      case EJ_NUMBER: {
        ej_free_number(data->v.number);
        break;
      }
      case EJ_ARRAY: {
        ej_free_ptr_array(data->v.array);
        break;
      }
      case EJ_EOBJECT:
      case EJ_OBJECT: {
        ej_free_object(data->v.object);
        break;
      }
      default:
        break;
    }
  }

  ej_free(data);
  data = NULL;
}

void ej_free_object_pair(EJObjectPair *data) {
  ej_assert_object_pair(data);

  if (data->key != NULL) {
    ej_free_value(data->key);
  }

  if (data->props != NULL) {
    ej_free_object(data->props);
  }

  if (data->value != NULL) {
    ej_free_value(data->value);
  }
  ej_free(data);
}

EJ_MODULE_EXPORT(void) ej_free_error(EJError *error) {
  if (error != NULL) {
    ej_free(error->message);
    ej_free(error);
  }
}

EJ_MODULE_EXPORT(void) ej_free_buffer(EJBuffer *buffer) {
  if(buffer->error->message == NULL) {
    ej_free(buffer->error);
  }
  ej_free(buffer);
}

EJ_MODULE_EXPORT(EJObjectPair*) ej_object_pair_new() {
  EJObjectPair *pair = ej_new0(EJObjectPair, 1);

  return pair;
}

EJ_MODULE_EXPORT(EJArray*) ej_value_array_new() {
  EJArray *arr = ej_ptr_array_new_with_func(ej_free_value);
  return arr;
}

EJ_MODULE_EXPORT(EJArray*) ej_pair_array_new() {
  EJArray *arr = ej_ptr_array_new_with_func(ej_free_object_pair);
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
EJ_MODULE_EXPORT(EJBool) ej_valid(EJBuffer *buffer, int pos) {
  if (!buffer || ((buffer->offset + pos) < 0) || ((buffer->offset + pos) > buffer->length)) {
    return false;
  }

  return true;
}

static EJString *ej_read_inner(EJBuffer *buffer, int pos) {
  return (char *)(buffer->content + buffer->offset + pos);
}

EJ_MODULE_EXPORT(EJString*) ej_read(EJBuffer *buffer, int pos) {
  if(!ej_valid(buffer, 0)) {
    return '\0';
  }

  return ej_read_inner(buffer, pos);
}

static const EJString ej_read_c_inner(EJBuffer *buffer, int pos) {
  if (!ej_valid(buffer, pos)) {
    return '\0';
  }

  return *ej_read_inner(buffer, pos);
}

EJBool ej_token_is(EJBuffer *buffer, EJ_TOKEN_TYPE etype) {
  EJLString token = EJ_TOKEN_STR[etype];
  EJBool bl;
  if (!ej_valid(buffer, (int)token.len)) {
    return false;
  }

  bl = (strncmp(token.value, ej_read_inner(buffer, 0), token.len) == 0);
  return bl;
}

EJ_MODULE_EXPORT(EJBool) ej_ensure_char(EJBuffer *buffer, EJ_TOKEN_TYPE ch) {
  if (!ej_skip_whitespace(buffer)) {
    return false;
  }

  if (ej_token_is(buffer, ch)) {
    return true;
  }

  return false;
}

EJ_MODULE_EXPORT(EJString) ej_read_c(EJBuffer *buffer, int pos) {
  EJString c;
  if (!ej_valid(buffer, pos)) {
    return '\0';
  }
  ej_comment(buffer);

  if (!ej_valid(buffer, 0)) {
    return '\0';
  }
  c = ej_read_c_inner(buffer, pos);

  return c;
}

void ej_skip_c(EJBuffer *buffer, EJString c) {
  if (!ej_valid(buffer, 1)) {
    return;
  }

  if (c == '\n') {
    ej_skip_line(buffer, 1, 1);
  }
  else {
    ej_skip_line(buffer, 1, 0);
  }
}

EJ_MODULE_EXPORT(void) ej_skip_line(EJBuffer *buffer, int cols, int rows) {
  if (rows == 0) {
    buffer->error->col += cols;
  } else {
    buffer->error->col = 1;
#if EJ_DEBUG
    printf("[new line]%ld,%c\n", buffer->error->row, ej_read_c(buffer, -1));
#endif
  }

  buffer->error->row += rows;
  buffer->offset = buffer->offset + cols;
}

EJ_MODULE_EXPORT(void) ej_buffer_skip(EJBuffer *buffer, int pos) {
  if (buffer == NULL || pos == 0) { return; }

  ej_skip_line(buffer, pos, 0);
}

EJ_MODULE_EXPORT(EJBool) ej_skip_whitespace(EJBuffer *buffer) {
  EJString c;
  while (true) {
    c = ej_read_c_inner(buffer, 0);
    if (c == '\0') { return false;}
    if (c == ' ' || c == '\r' || c == '\n') {
      ej_skip_c(buffer, c);
      continue;
    }

    ej_comment(buffer);
    break;
  }

  return true;
}

EJ_MODULE_EXPORT(EJBool) ej_skip_utf8_bom(EJBuffer *buffer) {
  if ((buffer == NULL) || (buffer->content == NULL) || (buffer->offset != 0)) {
    return false;
  }

  if (ej_valid(buffer, 4) && (strncmp(ej_read_inner(buffer, 0), "\xEF\xBB\xBF", 3) == 0)) {
    ej_buffer_skip(buffer, 3);
  }

  return true;
}

static EJString ej_next_c_inner(EJBuffer *buffer) {
  ej_skip_c(buffer, ej_read_c_inner(buffer, 0));

  return ej_read_c_inner(buffer, 0);
}

EJ_MODULE_EXPORT(EJString) ej_next_c(EJBuffer *buffer) {
  if (buffer == NULL) { return '\0'; }

  return ej_next_c_inner(buffer);
}

static bool ej_comment_line(EJBuffer *buffer) {
  EJString c;
  while (true) {
    c = ej_next_c_inner(buffer);
    if (c == '\0') { return false; }

    if (c == '\n') { break; }
  }

  return true;
}

static bool ej_comment_multiple(EJBuffer *buffer) {
  EJString c;

  while (true) {
    c = ej_next_c_inner(buffer);
    if (c == '\0') { ej_set_error(buffer, "mutiple line comment not close"); return false; }
    if (c != '*') { continue; }
    ej_buffer_skip(buffer, 1);

    c = ej_read_c_inner(buffer, 0);
    if (c == -1) { ej_set_error(buffer, "mutiple line comment not close"); return false; }

    if (c == '/') { ej_buffer_skip(buffer, 1); break; }
  }

  return true;
}

static void ej_comment(EJBuffer *buffer) {
  EJString c;

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

EJ_MODULE_EXPORT(const EJString *) ej_get_data_type_name(EJ_TYPE type) {
  if (type < 0 || type > EJ_RAW) {
    return NULL;
  }

  return EJ_TYPE_NAMES[type];
}

EJ_MODULE_EXPORT(EJBool) ej_object_get_value(EJObject *data, EJString *key, EJValue **value) {
  size_t i;
  EJObjectPair *pair = NULL;

  for (i = 0; i < data->len; i++) {
    pair = (EJObjectPair *)data->pdata[i];
    ej_return_val_if_fail((pair != NULL) && (pair->key != NULL) && (pair->value != NULL), false);

    if (pair->key->type != EJ_STRING) {
      continue;
    }

    if (ej_strcmp0((const char *)key, (const char *)pair->key->v.string) == 0) {
      *value = pair->value;
      return true;
    }
  }

  return false;
}

void ej_set_errorv(EJBuffer *buffer, EJString *fmt, va_list args) {
  buffer->error->message = ej_strdup_vprintf(fmt, args);
  va_end(args);
}

EJ_MODULE_EXPORT(EJError*) ej_get_error(EJBuffer *buffer) {
  return buffer->error;
}

EJ_MODULE_EXPORT(void) ej_set_error(EJBuffer *buffer, EJString *fmt, ...) {
  va_list args;

  ej_assert(buffer != NULL && buffer->error != NULL && "buffer error should not be NULL");

  if (buffer->error->message != NULL) {
    return;
  }

  va_start(args, fmt);
  buffer->error->message = ej_strdup_vprintf(fmt, args);
  va_end(args);

#if EJ_DEBUG
  printf("Error in <%ld, %ld>: %s", buffer->error->row, buffer->error->col, buffer->error->message);
#endif 
}

/* print */
EJ_MODULE_EXPORT(EJBool) ej_print_number(EJNumber *data, EJString **buffer) {
  switch (data->type)
  {
    case EJ_INT:
      *buffer = ej_strdup_printf("%d", data->v.i);
      break;
    case EJ_DOUBLE:
      *buffer = ej_strdup_printf("%lf", data->v.d);
      break;
    default:
      break;
  }

  return true;
}

EJ_MODULE_EXPORT(EJBool) ej_print_bool(EJBool data, EJString **buffer) {
  *buffer = data ? ej_strdup("true"): ej_strdup("false");
  return true;
}

EJ_MODULE_EXPORT(EJBool) ej_print_array_value(size_t arrlen, size_t index, EJValue *data, EJString **buffer) {
  GString *value;
  EJString *str = NULL;

  ej_assert(index < arrlen);

  value = ej_string_new("");
  if (ej_print_value(data, &str)) {
    value = ej_string_append(value, str); ej_free(str);
  }

  if (index + 1 == arrlen) {
    *buffer = value->str;
  }
  else {
    value = ej_string_append(value, ",");
    *buffer = value->str;
  }
  ej_string_free(value, false);

  return true;
}

EJ_MODULE_EXPORT(EJBool) ej_print_array(EJArray *data, EJString **buffer) {
  GString *value;
  EJString *str;
  size_t i, len;
  if (!data) { return false; }

  value = ej_string_new("[");
  len = data->len;
  for (i = 0; i < len; i++) {
    ej_print_array_value_inner(len, i, data->pdata[i], &str);
    value = ej_string_append(value, str); ej_free(str);
  }
  value = ej_string_append(value, "]");
  *buffer = value->str; ej_string_free(value, false);

  return true;
}

EJ_MODULE_EXPORT(EJBool) ej_print_object_pair_prop(EJArray *data, EJString **buffer) {
  EJString *prop = NULL;
  EJObjectPair *op = NULL;
  GString *value;
  guint size, i;

  if (!data) { return false; }

  value = ej_string_new("<");
  size = data->len;

  for(i = 0; i < data->len;i++) {
    op = data->pdata[i];

    if (ej_print_object_pair(op, &prop)) {
      value = ej_string_append(value, prop);ej_free(prop);

      if(i + 1 < size) {
        value = ej_string_append(value, ",");
      }
    }
  }
  value = ej_string_append(value, ">");

  *buffer = value->str; ej_string_free(value, false);

  return true;
}

EJ_MODULE_EXPORT(EJBool) ej_print_object_pair(EJObjectPair *data, EJString **buffer) {
  ej_return_val_if_fail(data->key != NULL, false);

  EJString *prop = NULL, *value = NULL, *key = NULL;

  if (!ej_print_value(data->key, &key)) {
    goto fail;
  }

  if (!ej_print_object_pair_prop(data->props, &prop)) {
    prop = ej_strdup("");
  }

  if (!ej_print_value(data->value, &value)) {
    value = ej_strdup("");
  }

  *buffer = ej_strdup_printf("%s%s:%s", key, prop, value);

  ej_free(key);
  ej_free(prop);
  ej_free(value);
  return true;
fail:
  return false;
}

EJBool ej_print_eobject(EJObject *data, EJString **buffer) {
  GString *value;

  if (!data) { return false; }

  value = ej_string_new("@");
  if (!ej_print_object(data, buffer)) {
    return false;
  }

  value = ej_string_append(value, *buffer); ej_free(*buffer);
  *buffer = value->str; ej_string_free(value, false);

  return true;
}

EJ_MODULE_EXPORT(EJBool) ej_print_object(EJObject *data, EJString **buffer) {
  GString *value;

  if (!data) { return false; }

  value = ej_string_new("{");

  if (data->len > 0) {
    ej_ptr_array_foreach(data, (EJFunc)ej_print_object_pair_inner, buffer);
    if(*buffer == NULL) { return false; }

    value = ej_string_append(value, *buffer); ej_free(*buffer);
    value = ej_string_truncate(value, (value->len-1)); // delete last ','
  }

  value = ej_string_append(value, "}");

  *buffer = value->str; ej_string_free(value, false);

  return true;
}

EJ_MODULE_EXPORT(EJBool) ej_print_value(EJValue *data, EJString **buffer) {
  if (!data) { return false; };

  switch (data->type)
  {
    case EJ_BOOLEAN:
      return ej_print_bool(data->v.bvalue, buffer);
    case EJ_NULL: {
      *buffer = ej_strdup("null");
      return true;
    }
    case EJ_STRING: {
      *buffer = ej_strdup_printf("\"%s\"", data->v.string);
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
  EJString *str = NULL;
  EJString **udata;
  GString *value;

  udata = (EJString **)user_data;
  if(*udata != NULL) {
    value = ej_string_new(*udata); ej_free(*udata);
  }
  else {
    value = ej_string_new("");
  }

  if(!ej_print_object_pair(pair, &str)) {
    return;
  }

  value = ej_string_append(value, str); ej_free(str);
  value = ej_string_append(value, ",");

  *udata = (gpointer)value->str;
  ej_string_free(value, false);
}

static void ej_print_array_value_inner(size_t arrlen, size_t index, EJValue *data, gpointer user_data) {
  ej_print_array_value(arrlen, index, data, user_data);
}

/* parse */
EJ_MODULE_EXPORT(EJBool) ej_parse_bool(EJBuffer *buffer, EJBool *data) {
  ej_return_val_if_fail(data != NULL, false);

  if (ej_token_is(buffer, EJ_TOKEN_FALSE)) {
    *data = false;
  }
  else if (ej_token_is(buffer, EJ_TOKEN_TRUE)) {
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

  ej_buffer_skip(buffer, 1);

  arr = ej_value_array_new();
  if (ej_ensure_char(buffer, EJ_TOKEN_BKT_END)) { goto success; }

  while (true) {
    if (!ej_parse_value(buffer, &value)) {
      goto fail;
    }
    ej_ptr_array_add(arr, (gpointer)value);

    if (ej_ensure_char(buffer, EJ_TOKEN_COMMA)) {
      ej_buffer_skip(buffer, 1);

      if (ej_ensure_char(buffer, EJ_TOKEN_BKT_END)) {
        break;
      }
    } 
    else if (ej_token_is(buffer, EJ_TOKEN_BKT_END)) {
      break;
    }
    else {
      goto fail;
    }
  }

success:
  if (!ej_token_is(buffer, EJ_TOKEN_BKT_END)) {
    goto fail;
  }
  ej_buffer_skip(buffer, 1);

  *data = arr;
  return true;
fail:
  ej_set_error(buffer, "Parse array failed");
  ej_free_ptr_array(arr);
  return false;
}

EJ_MODULE_EXPORT(EJBool) ej_parse_array(EJBuffer *buffer, EJArray **data) {
  if (!ej_token_is(buffer, EJ_TOKEN_BKT_START)) {
    return false;
  }

  return ej_parse_array_inner(buffer, data);
}

static EJString *ej_remove_escaped_string(EJString *data, size_t len, size_t skip) {
  ej_assert(data != NULL && len <= ej_strlen(data) && skip < len);

  size_t i = 0, j = 0;
  EJString c, n;
  EJString *ndata = data;

  while(i < len) {
    c = *(data + i);
    if (c == '\0') { return data; }

    if (c == '\\') {
      i += 1;
      n = *(data + i);

      switch (n)
      {
      case 'b':
        c = '\b';
        break;
      case 'f':
        c = '\f';
        break;
      case 'n':
        c = '\n';
        break;
      case 'r':
        c = '\r';
        break;
      case 't':
        c = '\t';
        break;
      case '\"':
        c = '\"';
        break;
      case '\\':
        c = '\\';
        break;
      // case 'u':
      //   break;
      case '/':
        c = '/';
        break;
      default:
        goto fail;
      }
    }

    *(ndata + j) = c;
    j += 1;
    i += 1;
  }

  ndata[j] = '\0';
  return ndata;

fail:
  ej_free(ndata);
  return NULL;
}

static EJBool ej_parse_string_inner(EJBuffer *buffer, EJString **data) {
  size_t len = 0;
  EJString c, n;
  size_t skip = 0;
  EJString *ndata;

  ej_buffer_skip(buffer, 1);

  len = 0;
  while (true) {
    c = ej_read_c_inner(buffer, (int)len);
    if (c == '\0') { ej_set_error(buffer, "occur buffer end when parse string"); return false; }

    if (c == '\\') {
      n = ej_read_c(buffer, len + 1);
      skip += 1;

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

    len++;
  }

  ndata = (EJString *) ej_strndup(ej_read_inner(buffer, 0), len);
  *data = ej_remove_escaped_string(ndata, len, skip);

  if(*data == NULL) {
    ej_free(ndata);
    goto fail;
  }

  ej_buffer_skip(buffer, len + 1);
  return true;

fail:
  ej_set_error(buffer, "Parse string failed");
  return false;
}

EJ_MODULE_EXPORT(EJBool) ej_parse_string(EJBuffer *buffer, EJString **data) {
  if (!ej_token_is(buffer, EJ_TOKEN_QMARK)) {
    return false;
  }

  return ej_parse_string_inner(buffer, data);
}

EJ_MODULE_EXPORT(EJBool) ej_parse_key_without_quote(EJBuffer *buffer, EJString **data) {
  EJString c;
  size_t pos = 0;

  for (pos = 0; (c = ej_read_c(buffer, pos)) != '\0'; pos++) {
    if (!ej_ascii_isalnum(c) && c != '-' && c != '_') {
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
  if (pos == 0) { return false; }

  *data = ej_strndup(ej_read_inner(buffer, 0), pos);
  ej_buffer_skip(buffer, pos);

  return true;
}

EJ_MODULE_EXPORT(EJBool) ej_parse_key(EJBuffer *buffer, EJValue **data) {
  if (!ej_skip_whitespace(buffer)) { return false; }
  if (ej_token_is(buffer, EJ_TOKEN_CUR_END)) { return false; }

  EJValue *kv;

  kv = ej_new0(EJValue, 1);
  if (*ej_read_inner(buffer, 0) == '@') {
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

  kv->type = EJ_STRING;
  if (!ej_parse_key_without_quote(buffer, &kv->v.string)) {
    goto fail;
  }

  if (!ej_valid(buffer, 1)) {
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
  EJString c;
  EJString *nstr;

  size_t type = EJ_INT;
  for (len = 0; (c = ej_read_c(buffer, len)) != '\0'; len++) {
    if (c == '.') {
      if (type == EJ_DOUBLE) { goto fail; }
      type = EJ_DOUBLE;
    }
    else if (ej_ascii_isdigit(c)
      || c == '+'
      || c == '-'
      || c == 'E'
      || c == 'e') {
    }
    else {
      break;
    }
  }
  if (len == 0) { ej_set_error(buffer, "Zero length of number"); return false; };

  num = ej_new0(EJNumber, 1);
  nstr = ej_strndup(ej_read_inner(buffer, 0), len);

  if (type == EJ_DOUBLE) {
    num->v.d = ej_ascii_strtod(nstr, NULL);
  }
  else if (type == EJ_INT) {
    num->v.i = (int)ej_ascii_strtoll(nstr, NULL, 10);
  }
  ej_free(nstr);
  ej_buffer_skip(buffer, len);

  num->type = type;
  *data = num;
  return true;
fail:
  ej_set_error(buffer, "Parse number failed");
  return false;
}

EJ_MODULE_EXPORT(EJBool) ej_parse_number(EJBuffer *buffer, EJNumber **data) {
  if (!ej_token_is(buffer, EJ_TOKEN_HYPHEN) && !ej_ascii_isdigit(*ej_read_inner(buffer, 0))) {
    return false;
  }

  return ej_parse_number_inner(buffer, data);
}

EJ_MODULE_EXPORT(EJBool) ej_parse_object_props(EJBuffer *buffer, EJObject *object, EJArray **data) {
  EJArray *props = NULL;
  EJObjectPair *pair = NULL;

  ej_assert(ej_token_is(buffer, EJ_TOKEN_LT) && (object != NULL));
  ej_buffer_skip(buffer, 1);
  if (!ej_skip_whitespace(buffer)) { return false; }

  props = ej_pair_array_new();
  if (ej_ensure_char(buffer, EJ_TOKEN_GT)) {
    goto success;
  }

  while (true) {
    if (!ej_valid(buffer, 1)) {
      goto fail;
    }

    pair = ej_object_pair_new();
    /* parse key */
    if (!ej_parse_key(buffer, &pair->key)) {
      ej_free_object_pair(pair);
      goto fail;
    }

    if (!ej_skip_whitespace(buffer)) { goto fail; }

    if (ej_token_is(buffer, EJ_TOKEN_LT)) {
      if (!ej_parse_object_props(buffer, props, &pair->props)) {
        ej_free_object_pair(pair);
        ej_set_error(buffer, "Parse property failed");
        goto fail;
      }
    }

    if (!ej_token_is(buffer, EJ_TOKEN_COLON)) {
      ej_set_error(buffer, "Missing ':' before parse key property value");
      ej_free_object_pair(pair);
      goto fail;
    }
    ej_buffer_skip(buffer, 1);

    /* parse value */
    if (!ej_parse_value(buffer, &pair->value)) {
      ej_set_error(buffer, "Parse property value failed");
      ej_free_object_pair(pair);
      goto fail;
    }
    ej_ptr_array_add(props, pair);

    if (ej_token_is(buffer, EJ_TOKEN_COMMA)) {
      ej_buffer_skip(buffer, 1);
      if (ej_ensure_char(buffer, EJ_TOKEN_GT)) {
        break;
      }
    } else if (ej_token_is(buffer, EJ_TOKEN_GT)) {
      break;

    } else {
      goto fail;
    }
  }

success:
  if (!ej_token_is(buffer, EJ_TOKEN_GT)) {
    goto fail;
  }
  ej_buffer_skip(buffer, 1);

  *data = props;
  return true;

fail:
  ej_free_ptr_array(props);
  return false;
}

EJ_MODULE_EXPORT(EJBool) ej_parse_object_pair(EJBuffer *buffer, EJObject *obj, EJObjectPair **data) {
  EJObjectPair *pair;

  if (!ej_skip_whitespace(buffer)) { return false; }

  pair = ej_object_pair_new();
  /* parse key */
  if (!ej_parse_key(buffer, &pair->key)) {
    goto fail;
  }

  if (ej_ensure_char(buffer, EJ_TOKEN_LT)) {
    if (!ej_parse_object_props(buffer, obj, &pair->props)) {
      goto fail;
    }
  }

  if (!ej_ensure_char(buffer, EJ_TOKEN_COLON)) {
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

  ej_buffer_skip(buffer, 1);
  if (!ej_skip_whitespace(buffer)) { return false; }

  obj = ej_pair_array_new();

  if (ej_token_is(buffer, EJ_TOKEN_CUR_END)) {
    goto success;
  }

  while (true) {
    if (!ej_parse_object_pair(buffer, obj, &pair)) {
      goto fail;
    }

    if (!ej_skip_whitespace(buffer)) {
      ej_free_object_pair(pair);
      goto fail;
    }

    ej_ptr_array_add(obj, pair);
    if (ej_token_is(buffer, EJ_TOKEN_COMMA)) {
      ej_buffer_skip(buffer, 1);

      if (!ej_skip_whitespace(buffer)) { goto fail; }
      if (ej_token_is(buffer, EJ_TOKEN_CUR_END)) {
        break;
      }
    } 
    else if (ej_token_is(buffer, EJ_TOKEN_CUR_END)) {
      break;

    } else {
      ej_set_error(buffer, "Missing ',' before when parse object");
      goto fail;
    }
  }

success:
  if (!ej_token_is(buffer, EJ_TOKEN_CUR_END)) {
    ej_set_error(buffer, "Not end with } when parse object");
    goto fail;
  }
  ej_buffer_skip(buffer, 1);

  *data = obj;
  return true;
fail:
  ej_free_ptr_array(obj);

  return false;
}

EJ_MODULE_EXPORT(EJBool) ej_parse_object(EJBuffer *buffer, EJObject **data) {
  if (!ej_token_is(buffer, EJ_TOKEN_CUR_START)) {
    return false;
  }

  return ej_parse_object_inner(buffer, data);
}

EJ_MODULE_EXPORT(EJBool) ej_parse_value(EJBuffer *buffer, EJValue **data) {
  EJValue *value = ej_new0(EJValue, 1);

  value->type = EJ_RAW;
  ej_assert(data != NULL && buffer != NULL && buffer->content != NULL);

  if (!ej_skip_whitespace(buffer)) { return false; }

  if (ej_parse_bool(buffer, &value->v.bvalue)) {
    *data = value;
    value->type = EJ_BOOLEAN;
    ej_buffer_skip(buffer, (value->v.bvalue ? 4 : 5));
    return true;
  }
  else if (ej_token_is(buffer, EJ_TOKEN_NULL)) {
    *data = value;
    value->type = EJ_NULL;
    ej_buffer_skip(buffer, 4);
    return true;
  }

  if (ej_token_is(buffer, EJ_TOKEN_HYPHEN) || ej_ascii_isdigit(*ej_read_inner(buffer, 0))) {
    value->type = EJ_NUMBER;
    if (!ej_parse_number_inner(buffer, &value->v.number)) {
      goto fail;
    }
  }
  else if (ej_token_is(buffer, EJ_TOKEN_QMARK)) {
    value->type = EJ_STRING;
    if (!ej_parse_string_inner(buffer, &value->v.string)) {
      goto fail;
    }
  }
  else if (ej_token_is(buffer, EJ_TOKEN_BKT_START)) {
    value->type = EJ_ARRAY;
    if (!ej_parse_array_inner(buffer, &value->v.array)) {
      goto fail;
    }
  }
  else if (ej_token_is(buffer, EJ_TOKEN_CUR_START)) {
    value->type = EJ_OBJECT;
    if (!ej_parse_object_inner(buffer, &value->v.object)) {
      goto fail;
    }
  }
  else if(ej_token_is(buffer, EJ_TOKEN_AT)) {
    value->type = EJ_EOBJECT;

    ej_buffer_skip(buffer, 1);
    ej_skip_whitespace(buffer);

    if (!ej_parse_object_inner(buffer, &value->v.object)) {
      goto fail;
    }
  }
  else {
    value->type = EJ_INVALID;
    ej_set_error(buffer, "Value should starts with '[' or '{' or '\"' or boolean");
    goto fail;
  }
  *data = value;
  return true;
fail:
  ej_set_error(buffer, "Parse value failed");
  ej_free_value(value);
  return false;
}

EJ_MODULE_EXPORT(EJBuffer*) ej_buffer_mode_new(const EJString *content, size_t len, EJ_MODE_TYPE mode) {
  ej_return_val_if_fail(content != NULL, NULL);

  EJBuffer *buffer = ej_new0(EJBuffer, 1);

  buffer->content = content;
  buffer->length = len;
  buffer->offset = 0;
  buffer->error = ej_error_new();
  buffer->mode = mode;

  return buffer;
}

EJ_MODULE_EXPORT(EJBuffer *) ej_buffer_new(const EJString *content, size_t len) {
  return ej_buffer_mode_new(content, len, EJ_MODE_RECURSIVE);
}

EJ_MODULE_EXPORT(EJValue*) ej_parse(EJError **error, const EJString *content) {
  EJBuffer *buffer;
  EJValue *value = NULL;

  buffer = ej_buffer_new(content, ej_strlen((const EJString *)content));
  ej_skip_utf8_bom(buffer);

  if (!ej_parse_value(buffer, &value)) {
    *error = ej_get_error(buffer);
    goto fail;
  }
  ej_free_buffer(buffer);
  return value;

fail:
    ej_free_buffer(buffer);
    return NULL;
}
