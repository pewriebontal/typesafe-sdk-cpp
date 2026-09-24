if(TYPESAFE_USE_LIBCURL)
    add_library(typesafe_examples_helper STATIC openrouter_helper.cpp)
    target_link_libraries(typesafe_examples_helper PUBLIC typesafe_cpp)

    set(TYPESAFE_EXAMPLES
        01_classify_message
        02_triage_ticket
        03_custom_model
        04_parallel_classification
        05_error_handling
        06_custom_transport
        07_list_models
        08_openrouter
        09_openrouter_sync
        10_openrouter_async
        11_openrouter_suite
    )
else()
    # Only 06 injects its own transport; every other example needs libcurl.
    message(STATUS "TYPESAFE_USE_LIBCURL=OFF: building only 06_custom_transport")
    set(TYPESAFE_EXAMPLES 06_custom_transport)
endif()

foreach(example IN LISTS TYPESAFE_EXAMPLES)
    add_executable(${example} ${example}.cpp)
    if(TARGET typesafe_examples_helper)
        target_link_libraries(${example} PRIVATE typesafe_examples_helper)
    else()
        target_link_libraries(${example} PRIVATE typesafe_cpp)
    endif()
endforeach()
