#include <stdbool.h>
#include "ExtendJson.h"
#include "ExtendJson-test.h"

void setUp(void) {
}

void tearDown(void) {
}

static void test_parse_free_var_type_array(void) {
  gchar *str = "{ \"name\": \n\
  \n\
 [\"arrayV1\", { \"arrayV2Key\": \"arrayV2Value\" }], age: 1 }";
  EJError *error = NULL;
  EJValue *value = ej_parse(&error, str);
  TEST_ASSERT_NULL(error);

  ej_free_value(value);
}

static void test_key_with_hyphen(void) {
  gchar *str = "{ name-key: 1 }";
  EJError *error = NULL;
  EJValue *value = ej_parse(&error, str);
  TEST_ASSERT_NULL(error);
  TEST_ASSERT_EQUAL_STRING(((EJObjectPair *)value->v.object->pdata[0])->key->v.string, "name-key");

  ej_free_value(value);
}

static void test_parse_value(void) {
  gchar *str = "{ \"name\": \"name1\", age: 1 }";
  EJError *error = NULL;

  EJValue *value = ej_parse(&error, str);
  TEST_ASSERT_NULL(error);
  
  TEST_ASSERT_TRUE(value != NULL);
  TEST_ASSERT_TRUE(value->v.object != NULL);
  TEST_ASSERT_TRUE(value->v.object->pdata != NULL);
  TEST_ASSERT_TRUE(1 == ((EJObjectPair *)(value->v.object->pdata[1]))->value->v.number->v.i);

  ej_free_value(value);
}

static void test_skip_bom(void) {
  gchar *str = "\xEF\xBB\xBF{ goodKey: @{bind: \"value1\"}, @{v1: 2}: \" hello\" }";
  EJError *error = NULL;
  gchar *out = NULL;
  EJValue *value = ej_parse(&error, str);
  TEST_ASSERT_NULL(error);
  TEST_ASSERT_NOT_NULL(value);

  TEST_ASSERT_TRUE(ej_print_value(value, &out));
  TEST_ASSERT_EQUAL_STRING(out, "{\"goodKey\":@{\"bind\":\"value1\"},@{\"v1\":2}:\" hello\"}");

  g_free(out);
  ej_free_value(value);
}

static void test_mutiple_bytes_string(void) {
  gchar *str = "[\"汉字\", {age: 2}]";
  EJError *error = NULL;
  gchar *out = NULL;
  EJValue *value = ej_parse(&error, str);
  TEST_ASSERT_NULL(error);
  TEST_ASSERT_NOT_NULL(value);

  TEST_ASSERT_TRUE(ej_print_value(value, &out));
  TEST_ASSERT_EQUAL_STRING(out, "[\"汉字\",{\"age\":2}]");

  g_free(out);
  ej_free_value(value);
}

static void test_parse_array(void) {
  gchar *str = "{ \"name\": \n\
  \n\
 [\"arrayV1\", { \"arrayV2Key\": \"arrayV2Value\" }, 2, true, ], age: 1 }";
  EJError *error = NULL;
  EJObjectPair *pair = NULL, *npair = NULL;
  EJArray *arr;
  EJValue *value = ej_parse(&error, str);
  TEST_ASSERT_NULL(error);
  TEST_ASSERT_TRUE(value != NULL);

  TEST_ASSERT_TRUE(value->v.object != NULL);
  TEST_ASSERT_TRUE(value->v.object->pdata[0] != NULL);
  
  pair = (EJObjectPair *)value->v.object->pdata[0];
  TEST_ASSERT_TRUE(pair->value->type == EJ_ARRAY);

  arr = pair->value->v.array;
  TEST_ASSERT_EQUAL_STRING(((EJValue *)arr->pdata[0])->v.string, "arrayV1");
  TEST_ASSERT_EQUAL(((EJValue *)arr->pdata[1])->v.object->len, 1);

  npair = (EJObjectPair *)(((EJValue *)arr->pdata[1])->v.object->pdata[0]);
  TEST_ASSERT_EQUAL_STRING(npair->key->v.string, "arrayV2Key");
  TEST_ASSERT_EQUAL_STRING(npair->value->v.string, "arrayV2Value");
  TEST_ASSERT_EQUAL(((EJValue *)arr->pdata[2])->v.number->v.i, 2);

  ej_free_value(value);
}

static void test_parse_value_with_comment(void) {
  gchar *str = "{ /* \n\
    hello \n\
    */ \"name\": \"name1\", age: 1 }";
  EJError *error = NULL;

  EJValue *value = ej_parse(&error, str);
  TEST_ASSERT_NULL(error);

  TEST_ASSERT_TRUE(value != NULL);
  TEST_ASSERT_TRUE(value->v.object != NULL);
  TEST_ASSERT_TRUE(value->v.object->pdata != NULL);
  TEST_ASSERT_TRUE(1 == ((EJObjectPair *)(value->v.object->pdata[1]))->value->v.number->v.i);

  ej_free_value(value);
}

static void test_comment_with_new_line(void) {
  gchar *str = "{ /* \n\
    hello \n\
    */ \"name\", \"name1\", age: 1 }";
  EJError *error = NULL;

  EJValue *value = ej_parse(&error, str);
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_EQUAL(error->row, 3);
  TEST_ASSERT_EQUAL(error->col, 14);

  ej_free_error(error);
}

static void test_comment_follow_comment(void) {
  gchar *str = "{ /* \n\
    hello \n\
    *//* comment2 */ \"name\", \"name1\", age: 1 }";
  EJError *error = NULL;

  EJValue *value = ej_parse(&error, str);
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_EQUAL(error->row, 3);
  TEST_ASSERT_EQUAL(error->col, 28);

  ej_free_error(error);
}

static void test_comment_not_close(void) {
  gchar *str = "{ /* \n\
    hello: \n\
    *//* comment2 \"name\", \"name1\", age: 1 }";
  EJError *error = NULL;

  EJValue *value = ej_parse(&error, str);
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_EQUAL(error->row, 3);
  TEST_ASSERT_EQUAL(error->col, 44);
  TEST_ASSERT_EQUAL_STRING(error->message, "mutiple line comment not close");

  ej_free_error(error);
}

static void test_parse_object(void) {
  gchar *str = "{ /* \n\
    hello \n\
    */ \"name\": \"name1\", age: 1 }";
  EJError *error = NULL;

  EJValue *value = ej_parse(&error, str);
  TEST_ASSERT_NULL(error);

  TEST_ASSERT_TRUE(value != NULL);
  TEST_ASSERT_TRUE(value->v.object != NULL);
  TEST_ASSERT_TRUE(value->v.object->pdata != NULL);
  TEST_ASSERT_TRUE(1 == ((EJObjectPair *)(value->v.object->pdata[1]))->value->v.number->v.i);

  ej_free_value(value);
}

static void test_parse_props(void) {
  gchar *str = "{ /* \n\
    hello \n\
    */ \"name\": \"name1\", age<p1:\"p1Value\", num: 12>: 1 }";
  EJError *error = NULL;
  EJObjectPair *pair = NULL;
  EJValue *value = ej_parse(&error, str);
  TEST_ASSERT_NULL(error);

  TEST_ASSERT_TRUE(value != NULL);
  TEST_ASSERT_TRUE(value->v.object != NULL);
  TEST_ASSERT_TRUE(value->v.object->pdata != NULL);
  TEST_ASSERT_TRUE(1 == ((EJObjectPair *)(value->v.object->pdata[1]))->value->v.number->v.i);

  pair = (EJObjectPair *)(((EJObjectPair *)(value->v.object->pdata[1]))->props->pdata[0]);
  TEST_ASSERT_TRUE(pair->key->type == EJ_STRING);
  TEST_ASSERT_EQUAL_STRING(pair->key->v.string, "p1");
  TEST_ASSERT_EQUAL_STRING(pair->value->v.string, "p1Value");

  ej_free_value(value);
}

static void test_parse_last_object_comma_skip(void) {
  gchar *str = "{ /* \n\
    hello \n\
    */ \"name\": \"name1\", age<p1:\"p1Value\", num: 12>: 1 , }";
  EJError *error = NULL;
  EJObjectPair *pair = NULL;
  EJValue *value = ej_parse(&error, str);
  TEST_ASSERT_NULL(error);

  TEST_ASSERT_TRUE(value != NULL);
  TEST_ASSERT_TRUE(value->v.object != NULL);
  TEST_ASSERT_TRUE(value->v.object->pdata != NULL);
  TEST_ASSERT_TRUE(1 == ((EJObjectPair *)(value->v.object->pdata[1]))->value->v.number->v.i);

  pair = (EJObjectPair *)(((EJObjectPair *)(value->v.object->pdata[1]))->props->pdata[0]);
  TEST_ASSERT_TRUE(pair->key->type == EJ_STRING);
  TEST_ASSERT_EQUAL_STRING(pair->key->v.string, "p1");
  TEST_ASSERT_EQUAL_STRING(pair->value->v.string, "p1Value");

  ej_free_value(value);
}

static void test_parse_extend_key(void) {
  gchar *str = "{ @{event:\"click\"}: \"name1\" }";
  EJError *error = NULL;
  EJObjectPair *pair = NULL, *npair = NULL;
  EJObject *obj = NULL;
  EJValue *value = ej_parse(&error, str);
  TEST_ASSERT_NULL(error);
  TEST_ASSERT_NOT_NULL(value);

  TEST_ASSERT_NOT_NULL(value->v.object->pdata[0]);
  pair = (EJObjectPair *)value->v.object->pdata[0];
  TEST_ASSERT_TRUE(pair->key->type == EJ_EOBJECT);
  TEST_ASSERT_NOT_NULL(pair->key->v.object);

  npair = (EJObjectPair *)pair->key->v.object->pdata[0];
  TEST_ASSERT_EQUAL_STRING(npair->key->v.string,"event");

  ej_free_value(value);
}

static void test_parse_extend_value(void) {
  gchar *str = "{ goodKey: @{bind: \"value1\"} }";
  EJError *error = NULL;
  EJObjectPair *pair = NULL, *npair = NULL;
  EJObject *obj = NULL;
  EJValue *value = ej_parse(&error, str);
  TEST_ASSERT_NULL(error);
  TEST_ASSERT_NOT_NULL(value);

  TEST_ASSERT_NOT_NULL(value->v.object->pdata[0]);
  pair = (EJObjectPair *)value->v.object->pdata[0];
  TEST_ASSERT_TRUE(pair->value->type == EJ_EOBJECT);
  TEST_ASSERT_NOT_NULL(pair->value->v.object);

  npair = (EJObjectPair *)pair->value->v.object->pdata[0];
  TEST_ASSERT_EQUAL_STRING(npair->key->v.string, "bind");
  TEST_ASSERT_EQUAL_STRING(npair->value->v.string, "value1");

  ej_free_value(value);
}

static void test_print_value(void) {
  gchar *str = "{ goodKey: @{bind: \"value1\"}, @{v1: 2}: \" hello\" }";
  EJError *error = NULL;
  gchar *out = NULL;
  EJValue *value = ej_parse(&error, str);
  TEST_ASSERT_NULL(error);
  TEST_ASSERT_NOT_NULL(value);

  TEST_ASSERT_TRUE(ej_print_value(value, &out));
  TEST_ASSERT_EQUAL_STRING(out, "{\"goodKey\":@{\"bind\":\"value1\"},@{\"v1\":2}:\" hello\"}");

  g_free(out);
  ej_free_value(value);
}

int main() {
  UNITY_BEGIN();
  {
    RUN_TEST(test_key_with_hyphen);
    RUN_TEST(test_mutiple_bytes_string);
    RUN_TEST(test_skip_bom);
    RUN_TEST(test_print_value);
    RUN_TEST(test_parse_array);
    RUN_TEST(test_comment_not_close);
    RUN_TEST(test_parse_value);
    RUN_TEST(test_parse_value_with_comment);
    RUN_TEST(test_parse_object);
    RUN_TEST(test_parse_props);
    RUN_TEST(test_parse_last_object_comma_skip);
    RUN_TEST(test_parse_extend_key);
    RUN_TEST(test_parse_extend_value);
    RUN_TEST(test_parse_free_var_type_array);
    RUN_TEST(test_comment_with_new_line);
    RUN_TEST(test_comment_follow_comment);
  }
  UNITY_END();
  return 0;
}

