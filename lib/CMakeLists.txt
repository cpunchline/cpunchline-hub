if(LIB_HV_OPTION)
    add_subdirectory(ipc_hv)
    add_subdirectory(local_registry)
endif()

if(LIB_NNG_OPTION)
    add_subdirectory(ipc_nng)
    add_subdirectory(ipc_nng_long)
endif()

if(LIB_LIBZMQ_OPTION)
    add_subdirectory(ipc_zmq)
endif()

add_subdirectory(shm)