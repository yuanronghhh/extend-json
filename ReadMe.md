# ExtendJson, a simple json parser with key property


## what is this ?

a simple extend json library for tree data.

## example

A tree structure in standard json like:
```json
{
  "layout": {
    "properties":[{ "key1": "layoutvalue", "key2": []}],
    "children":[{
      "name": "child1",
      "properties":[{"childKey1": "childValue1"}],
      "children": []
    }]
  }
}
```

but now you can use like this:

```json
{
  layout<key1: "layoutvalue", key2:[]>: {
    child1<childKey1: "childValue1">: []
  }
}
```

### support @ prefix for extend
```json
{
  layout<key1: "layoutvalue", key2:[]>: {
    child1<@{bind:"click"}: "click_handler">: @{bind: "value2"}
  }
}
```

## usage
please see test for example.

```c
// just use ej_parse function;
EJValue *value = ej_parse(&error, str);
if(error != NULL) {
  g_print("<%u,%u>%s", error->row, error->col, error->message);
}

ej_free_value(value);
```
