# ExtendJson, a simple json parser with key property

## Example
1. For tree structure in json.
```json
{
  layout: {
    properties:[{ "key1": "layoutvalue", "key2": []}],
    children:[{
      "name": "child1",
      "properties":[{"childKey1": "childValue1"}]
      "children": []
    }]
  }
}
```

2. Here example
```json
{
  layout<key1: "layoutvalue", key2:[]>: {
    child1<childKey1: "childValue1">: []
  }
}
```

## Usage
```c
int main(int argc, char *argv[]) {
  gchar *output = NULL;
  gchar *content = NULL;
  GError *err = NULL;
  size_t bsize = 0;
  EJValue *value;
  const gchar *filename = "<file-path>";

  if (!g_file_get_contents(filename, &content, &bsize, &err)) {
    g_error("%s", err->message);
    return -1;
  }

  value = ej_parse(content);
  ej_print_value(value, &output);

  printf("%s", output);

  ej_free_value(value);
  g_free(output);

  return 0;
}
```

## Note

Property key is unique between `<` and `>`
