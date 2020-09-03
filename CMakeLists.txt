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
target_link_libraries(extend-json
  ${GLIB_LIBRARIES}
)
