idf_component_register(SRCS "test_modem.cpp" "LoopbackTerm.cpp"
                       INCLUDE_DIRS "$ENV{IDF_PATH}/tools/catch"
                       REQUIRES esp_modem)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${COMPONENT_LIB}  PRIVATE Threads::Threads)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)
target_compile_definitions(${COMPONENT_LIB} PRIVATE "-DCONFIG_IDF_TARGET_LINUX")

if(CONFIG_GCOV_ENABLED)
    target_compile_options(${COMPONENT_LIB} PUBLIC --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(${COMPONENT_LIB} PUBLIC  --coverage -fprofile-arcs -ftest-coverage)

    idf_component_get_property(esp_modem esp_modem COMPONENT_LIB)
    target_compile_options(${esp_modem} PUBLIC --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(${esp_modem} PUBLIC  --coverage -fprofile-arcs -ftest-coverage)
endif()
