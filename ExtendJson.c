#include "ExtendJson.h"

#define EJ_BUFFER_SKIP(buffer, len) (buffer->offset = buffer->offset + (len))
#define EJ_SKIP_AND_VALID(buffer) ej_skip_whitespace(buffer)
#define EJ_ENSURE_CHAR(buffer, ch) (EJ_SKIP_AND_VALID(buffer) && *ej_read(buffer, 0) == ch)

#define EJ_STR_MAX (INT_MAX / 2)
#define ej_new0(struct_type, n_structs)  ej_malloc0(sizeof(struct_type) * n_structs)

const gchar* EJ_TYPE_NAMES[EJ_RAW] = { "Invalid", "Boolean", "String", "Array", "Number", "Object", "Null", "Raw" };

/* declare */
static void ej_print_value_inner(EJValue *data, gpointer user_data);
static void ej_print_object_pair_inner(EJObjectPair *value, gpointer user_data);
static void ej_print_array_value_inner(size_t arrlen, size_t index, EJValue *data, gpointer user_data);

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

void ej_free_number(EJNumber *data) {
  g_free(data);
}

void ej_free_value(EJValue *data) {
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
      case EJ_OBJECT: {
        g_ptr_array_unref(data->v.object);
        g_free(data);
        break;
      }
      default:
        break;
      }
  }
}

void ej_free_object_pair(EJObjectPair *data) {
  g_free(data->key);
  if (data->props != NULL) {
    g_hash_table_unref(data->props);
  }

  if(data->value != NULL) {
    ej_free_value(data->value);
  }
  g_free(data);
}

EJObjectPair *ej_object_pair_new() {
  EJObjectPair *pair = ej_new0(EJObjectPair, 1);

  return pair;
}

EJHash *ej_hash_new() {
  EJHash *obj = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify)g_free, (GDestroyNotify)ej_free_value);
  return obj;
}

EJArray *ej_array_new() {
  EJArray *arr = g_ptr_array_new_with_free_func((GDestroyNotify)ej_free_value);
  return arr;
}

const gchar *ej_read(EJBuffer *buffer, int pos) {
  return buffer->content + buffer->offset + pos;
}

EJBool ej_valid(EJBuffer *buffer, int pos) {
  if (!buffer || ((buffer->offset + pos) >= buffer->length)) {
    return false;
  }

  return true;
}

const gchar ej_access_c(EJBuffer *buffer, int pos) {
  gchar c;
  if (!ej_valid(buffer, pos)) {
    return -1;
  }
  c = *(buffer->content + buffer->offset + pos);

  return c;
}

EJBool ej_skip_whitespace(EJBuffer *buffer) {
  gchar c;
  while ((c = ej_access_c(buffer, 0))) {
    if (c == -1) { return false; }

    if (c != ' ' && c != '\n') {
      break;
    }

    EJ_BUFFER_SKIP(buffer, 1);
  }

  return true;
}

const gchar *ej_get_data_type_name(EJ_TYPE type) {
  if (type < 0 || type > EJ_RAW) {
    return NULL;
  }

  return EJ_TYPE_NAMES[type];
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
  gchar *str;

  g_assert(index < arrlen);

  value = g_string_new("");
  if (ej_print_value(data, &str)) {
    value = g_string_append(value, str); g_free(str);
  }

  if (index + 1 == arrlen) {
    *buffer = value->str;
  } 
  else {
    value = g_string_append(value, ", "); 
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

EJBool ej_print_object_pair_prop(EJHash *data, gchar **buffer) {
  gchar *key = NULL, *prop = NULL;
  EJObjectPair *op = NULL;
  GString *value;
  GHashTableIter itr;
  guint size, i;

  if (!data) { return false; }

  value = g_string_new("<");
  g_hash_table_iter_init(&itr, data);
  size = g_hash_table_size(data);

  for(i = 0;g_hash_table_iter_next(&itr, NULL, (gpointer)&op);i++) {

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
  gchar *prop = NULL, *value = NULL;

  g_assert(data->key != NULL && *data->key != '\0');

  if (!ej_print_object_pair_prop(data->props, &prop)) {
    prop = g_strdup("");
  }

  if (!ej_print_value(data->value, &value)) {
    free(prop);
    return false;
  }

  *buffer = g_strdup_printf("\"%s\"%s:%s", data->key, prop, value);

  g_free(prop);
  g_free(value);

  return true;
}

EJBool ej_print_object(EJObject *data, gchar **buffer) {
  GString *value;

  if (!data) { return false; }

  value = g_string_new("{");
  g_ptr_array_foreach(data, (GFunc)ej_print_object_pair_inner, buffer);
  if(!*buffer) { return false; }

  value = g_string_append(value, *buffer); g_free(*buffer);
  value = g_string_truncate(value, (value->len-1)); // delete last ','
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

EJBool ej_parse_array(EJBuffer *buffer, EJArray **data) {
  EJArray *arr;
  EJValue *value;
  size_t i = 0;
  gchar c;

  if (*ej_read(buffer, 0) != '[') {
    return false;
  }
  EJ_BUFFER_SKIP(buffer, 1);
  ej_skip_whitespace(buffer);

  arr = ej_array_new();
  while (true) {
    if (!ej_parse_value(buffer, &value)) {
      goto fail;
    }
    g_ptr_array_add(arr, (gpointer)value);

    ej_skip_whitespace(buffer);
    c = ej_access_c(buffer, 0);
    if (c == -1) { goto fail; }
    if (c == ']') { break; }
    if (c == ',') { EJ_BUFFER_SKIP(buffer, 1); }
  }

  if (*ej_read(buffer, 0) != ']') {
    goto fail;
  }
  EJ_BUFFER_SKIP(buffer, 1);

  *data = arr;
  return true;

fail:
  if (value != NULL) {
    ej_free_value(value);
  }

  g_ptr_array_unref(arr);
  return false;
}

EJBool ej_parse_string(EJBuffer *buffer, EJString **data) {
  size_t len = 0;
  gchar c, n;

  if (*ej_read(buffer, 0) != '\"') {
    return false;
  }
  EJ_BUFFER_SKIP(buffer, 1);

  for (len = 0; c = ej_access_c(buffer, len); len++) {
    if (c == -1) {
      return false;
    }

    if (c == '\\') {
      n = ej_access_c(buffer, len + 1);

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
        return false;
      }
    }

    if (c == '\"' || len + 1 > EJ_STR_MAX) {
      break;
    }
  }

  *data = g_strndup(ej_read(buffer, 0), len);

  EJ_BUFFER_SKIP(buffer, len + 1);
  return true;
}

EJBool ej_parse_key(EJBuffer *buffer, EJString **data) {
  if (!EJ_SKIP_AND_VALID(buffer)) { return false; }

  EJString *key = NULL;
  gchar c;
  size_t pos = 0;

  if (ej_parse_string(buffer, data)) {
    return true;
  }

  for (pos = 0; c = ej_access_c(buffer, pos); pos++) {
    if (c == -1 || c == ' ' || c == '<' || c == '>' || c == ':' || c == '\"' || c == '\n') {
      if (pos == 0) {
        g_debug("Key length cannot be zero.");
        return false; 
      }
      break;
    }

    if (pos + 1 > EJ_STR_MAX) {
      g_debug("Key position %c length %zu too long.", c, pos + 1);
      return false;
    }
  }

  key = g_strndup(ej_read(buffer, 0), pos);
  EJ_BUFFER_SKIP(buffer, pos);

  if (!ej_valid(buffer, 0)) {
    g_free(key);
    return false;
  }

  *data = key;
  return true;
}

EJBool ej_parse_number(EJBuffer *buffer, EJNumber **data) {
  EJNumber *num = ej_new0(EJNumber, 1);
  size_t len;
  gchar c;
  gchar *nstr;

  size_t type = EJ_INT;
  for (len = 0; c = ej_access_c(buffer, len); len++) {
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
  else if (num->type == EJ_INT) {
    num->v.i = (int)g_ascii_strtoll(nstr, NULL, 10);
  }
  g_free(nstr);

  EJ_BUFFER_SKIP(buffer, len);

  num->type = type;
  *data = num;
  return true;

fail:
  g_free(num);
  return false;
}

EJBool ej_parse_object_props(EJBuffer *buffer, EJHash **data) {
  EJHash *obj = NULL;
  EJObjectPair *pair = NULL;

  g_assert(ej_valid(buffer, 0) && (*ej_read(buffer, 0) == '<'));

  obj = ej_hash_new();
  if (*ej_read(buffer, 0) == '>') {
    goto success;
  }

  do {
    pair = ej_object_pair_new();
    EJ_BUFFER_SKIP(buffer, 1);
    /* parse key */
    if (!ej_parse_key(buffer, &pair->key)) {
      goto fail;
    }

    if (*ej_read(buffer, 0) == '<') {
      if (!ej_parse_object_props(buffer, &pair->props)) {
        goto fail;
      }
    }

    if (!EJ_ENSURE_CHAR(buffer, ':')) {
      goto fail;
    }
    EJ_BUFFER_SKIP(buffer, 1);

    /* parse value */
    if (!ej_parse_value(buffer, &pair->value)) {
      goto fail;
    }

    if (!EJ_SKIP_AND_VALID(buffer)) {
      goto fail;
    }

    g_hash_table_insert(obj, pair->key, pair);
  } while (*ej_read(buffer, 0) == ',');

  if (!EJ_SKIP_AND_VALID(buffer)) {
    goto fail;
  }

  if (*ej_read(buffer, 0) != '>') {
    goto fail;
  }
  EJ_BUFFER_SKIP(buffer, 1);

success:
  *data = obj;
  return true;

fail:
  if (pair != NULL) {
    ej_free_object_pair(pair);
  }

  g_hash_table_unref(obj);
  return false;
}

EJBool ej_parse_object(EJBuffer *buffer, EJObject **data) {
  EJObject *obj = NULL;
  EJObjectPair *pair = NULL;

  g_assert(ej_valid(buffer, 0) && ((*ej_read(buffer, 0) == '{') || (*ej_read(buffer, 0) == '<')));

  obj = ej_array_new();
  if ((*ej_read(buffer, 0) == '}') || (*ej_read(buffer, 0) == '>')) {
    goto success;
  }

  do {
    pair = ej_object_pair_new();
    EJ_BUFFER_SKIP(buffer, 1);
    /* parse key */
    if (!ej_parse_key(buffer, &pair->key)) {
      goto fail;
    }
 
    if (*ej_read(buffer, 0) == '<') {
      if (!ej_parse_object_props(buffer, &pair->props)) {
        goto fail;
      }
    }

    if (!EJ_ENSURE_CHAR(buffer, ':')) {
      goto fail;
    }
    EJ_BUFFER_SKIP(buffer, 1);

    /* parse value */
    if (!ej_parse_value(buffer, &pair->value)) {
      goto fail;
    }

    if (!EJ_SKIP_AND_VALID(buffer)) {
      goto fail;
    }

    g_ptr_array_add(obj, pair);
  } while (*ej_read(buffer, 0) == ',');

  if (!EJ_SKIP_AND_VALID(buffer)) {
    goto fail;
  }

  if (*ej_read(buffer, 0) != '>' && *ej_read(buffer, 0) != '}') {
    goto fail;
  }
  EJ_BUFFER_SKIP(buffer, 1);

success:
  *data = obj;
  return true;

fail:
  if (pair != NULL) {
    ej_free_object_pair(pair);
  }

  g_ptr_array_unref(obj);
  return false;
}

EJBool ej_parse_value(EJBuffer *buffer, EJValue **data) {
  EJValue *value = ej_new0(EJValue, 1);
 
  if (!EJ_SKIP_AND_VALID(buffer)) { return false; }

  value->type = EJ_RAW;
  if (!buffer || (buffer->content == NULL)) {
    goto fail;
  }

  if (ej_parse_bool(buffer, &value->v.bvalue)) {
    *data = value;
    value->type = EJ_BOOLEAN;
    EJ_BUFFER_SKIP(buffer, (value->v.bvalue ? 4 : 5));
    return true;
  } 
  else if (ej_valid(buffer, 4) && strncmp(ej_read(buffer, 0), "null", 4) == 0) {
    *data = value;
    value->type = EJ_NULL;
    EJ_BUFFER_SKIP(buffer, 4);
    return true;
  }

  if (ej_valid(buffer, 0)) {
    if (*ej_read(buffer, 0) == '-' || (*ej_read(buffer, 0) >= '0' && *ej_read(buffer, 0) <= '9') ) {
      value->type = EJ_NUMBER;
      if (!ej_parse_number(buffer, &value->v.number)) {
        goto fail;
      }
    } 
    else if (*ej_read(buffer, 0) == '\"') {
      value->type = EJ_STRING;
      if (!ej_parse_string(buffer, &value->v.string)) {
        goto fail;
      }
    } 
    else if (*ej_read(buffer, 0) == '[') {
      value->type = EJ_ARRAY;
      if (!ej_parse_array(buffer, &value->v.array)) {
        goto fail;
      }
    } 
    else if (*ej_read(buffer, 0) == '{') {
      value->type = EJ_OBJECT;
      if (!ej_parse_object(buffer, &value->v.object)) {
        goto fail;
      }
    } 
    else {
      goto fail;
    }
  }
  
  *data = value;
  return true;
fail:
  ej_free_value(value);
  return false;
}

static EJBool skip_utf8_bom(EJBuffer *const buffer) {
  if ((buffer == NULL) || (buffer->content == NULL) || (buffer->offset != 0)) {
    return false;
  }

  if (ej_valid(buffer, 4) && (strncmp(ej_read(buffer, 0), "\xEF\xBB\xBF", 3) == 0)) {
    EJ_BUFFER_SKIP(buffer, 3);
  }

  return true;
}

EJValue *ej_parse(const gchar *content) {
  EJBuffer buffer = { 0 };
  gchar *str1;
  EJValue *value = NULL;

  buffer.content = (gchar *)content;
  buffer.length = ej_strlen((gchar *)content, EJ_STR_MAX);
  buffer.offset = 0;

  skip_utf8_bom(&buffer);

  if (!ej_parse_value(&buffer, &value)) {
    if (ej_valid(&buffer, -10)) {
      str1 = g_strndup(ej_read(&buffer, -10), 10);
    }
    g_debug("parse buffer at %zu:[%s]", buffer.offset, str1);

    if (str1) { g_free(str1); }

    return NULL;
  }

  g_debug("parse success");
  return value;
}
