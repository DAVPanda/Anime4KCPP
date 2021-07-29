set(CMDLINE_H_URL https://github.com/TianZerL/cmdline/raw/master/cmdline.h)
set(SHA1_CMDLINE "383044e4fbc6066249d4102a39431d67ed3657c7")

if(EXISTS ${TOP_DIR}/ThirdParty/include/cmdline/cmdline.h)
    file(SHA1 ${TOP_DIR}/ThirdParty/include/cmdline/cmdline.h LOCAL_SHA1_CMDLINE)

    if(NOT ${LOCAL_SHA1_CMDLINE} STREQUAL ${SHA1_CMDLINE})
        message("Warning:")
        message("   Local SHA1 for comline.h:   ${LOCAL_SHA1_CMDLINE}")
        message("   Expected SHA1:              ${SHA1_CMDLINE}")
        message("   Mismatch SHA1 for cmdline.h, trying to download it...")

        file(
            DOWNLOAD ${CMDLINE_H_URL} ${TOP_DIR}/ThirdParty/include/cmdline/cmdline.h 
            SHOW_PROGRESS 
            EXPECTED_HASH SHA1=${SHA1_CMDLINE}
        )

    endif()
else()
    file(
        DOWNLOAD ${CMDLINE_H_URL} ${TOP_DIR}/ThirdParty/include/cmdline/cmdline.h 
        SHOW_PROGRESS 
        EXPECTED_HASH SHA1=${SHA1_CMDLINE}
    )
endif()

find_package(CURL)
if(${CURL_FOUND})
    message(STATUS "CLI: libcurl found, enable web image download support.")
    target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)
    add_definitions(-DENABLE_LIBCURL)
endif()

if(Use_TBB)
    include_directories(${TBB_INCLUDE_PATH})
    find_library(TBB_LIBS 
    NAMES tbb 
    PATHS ${TBB_LIB_PATH} 
    REQUIRED)
    target_link_libraries(${PROJECT_NAME} ${TBB_LIBS})
endif()

include_directories(${TOP_DIR}/ThirdParty/include/cmdline)

target_link_libraries(${PROJECT_NAME} PRIVATE Anime4KCPPCore)

if(Use_Boost_filesystem)
    find_package(Boost COMPONENTS filesystem REQUIRED)
    include_directories(${Boost_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0) # Just for G++-8 to enable filesystem
    target_link_libraries(${PROJECT_NAME} stdc++fs)
endif()
