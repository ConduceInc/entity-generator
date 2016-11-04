
find_package(Boost 1.55 REQUIRED COMPONENTS program_options)

INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS} ${RAPIDJSON_INCLUDE_DIRS})

set (SRC
    entity-generator.cpp
)

add_executable(entity-generator ${SRC})
target_link_libraries(entity-generator ${Boost_LIBRARIES} curl)
