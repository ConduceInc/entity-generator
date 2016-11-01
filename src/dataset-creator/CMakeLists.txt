
find_package(Boost 1.55 REQUIRED COMPONENTS program_options)

INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS} ${RAPIDJSON_INCLUDE_DIRS})

set (SRC
    datatset-creator.cpp
)

add_executable(dataset-creator ${SRC})
target_link_libraries(dataset-creator ${Boost_LIBRARIES} curl)
