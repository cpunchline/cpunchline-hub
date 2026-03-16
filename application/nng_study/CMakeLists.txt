if(NOT CPUNCHLINE_BUILD_THIRDPARTY)
    find_package(nng REQUIRED)
    if(TARGET nng::nng AND NOT TARGET nng)
        add_library(nng ALIAS nng::nng)
    endif()
endif()

set(NNG_STUDY_BUS_SRC nng_bus_study.c)
set(NNG_STUDY_PAIR_SRC nng_pair_study.c)
set(NNG_STUDY_PAIR_ASYNC_MANAGER_SRC nng_pair_async_manager_study.cpp)
set(NNG_STUDY_PAIR_ASYNC_ADAPTER_SRC nng_pair_async_adapter_study.cpp)
set(NNG_STUDY_PIPELINE_SRC nng_pipeline_study.c)
set(NNG_STUDY_PUBSUB_SRC nng_pubsub_study.c)
set(NNG_STUDY_REQRESP_SRC nng_reqresp_study.c)
set(NNG_STUDY_SURVERY_SRC nng_survey_study.c)

add_executable(nng_bus_study ${NNG_STUDY_BUS_SRC})
target_link_libraries(nng_bus_study PRIVATE Threads::Threads nng)

add_executable(nng_pair_study ${NNG_STUDY_PAIR_SRC})
target_link_libraries(nng_pair_study PRIVATE Threads::Threads nng)

add_executable(nng_pair_async_manager_study ${NNG_STUDY_PAIR_ASYNC_MANAGER_SRC})
target_link_libraries(nng_pair_async_manager_study PRIVATE Threads::Threads nng)

add_executable(nng_pair_async_adapter_study ${NNG_STUDY_PAIR_ASYNC_ADAPTER_SRC})
target_link_libraries(nng_pair_async_adapter_study PRIVATE Threads::Threads nng)

add_executable(nng_pipeline_study ${NNG_STUDY_PIPELINE_SRC})
target_link_libraries(nng_pipeline_study PRIVATE Threads::Threads nng)

add_executable(nng_pubsub_study ${NNG_STUDY_PUBSUB_SRC})
target_link_libraries(nng_pubsub_study PRIVATE Threads::Threads nng)

add_executable(nng_reqresp_study ${NNG_STUDY_REQRESP_SRC})
target_link_libraries(nng_reqresp_study PRIVATE Threads::Threads nng)

add_executable(nng_survey_study ${NNG_STUDY_SURVERY_SRC})
target_link_libraries(nng_survey_study PRIVATE Threads::Threads nng)

install(TARGETS nng_bus_study nng_pair_study nng_pipeline_study nng_pubsub_study nng_reqresp_study nng_survey_study nng_pair_async_adapter_study nng_pair_async_manager_study DESTINATION ${CMAKE_INSTALL_BINDIR})
