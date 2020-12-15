set(INC
  ./
  ${GLIB_INCLUDE_DIRS}
)

set(SRC
  ./ExtendJson.c
  ./ExtendJson.h
)
include_directories("${INC}")
add_library(extend-json "${SRC}")
set_property(TARGET extend-json PROPERTY FOLDER ExtendJsonProject)

target_link_libraries(extend-json
  ${GLIB_LIBRARIES}
)

# TEST
set(TEST-INC
  ./
  ${GLIB_INCLUDE_DIRS}
  ${UNITY_INCLUDE_DIRS}
)

set(TEST-SRC
  ./ExtendJson-test.c
  ./ExtendJson-test.h
)

include_directories("${TEST-INC}")
add_executable(extend-json-test "${TEST-SRC}")
set_property(TARGET extend-json-test PROPERTY FOLDER ExtendJsonProject)

target_link_libraries(extend-json-test
  ${GLIB_LIBRARIES}
  extend-json
  ${UNITY_LIBRARIES}
)
