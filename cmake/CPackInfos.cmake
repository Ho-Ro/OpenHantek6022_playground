# This file configures CPack. 
# A tgz, deb and rpm file is created for linux (tgz and deb tested on debian stretch and buster).
# A tgz file is created for osx (not tested).
# A zip file and an NSIS Installer exe is created on windows (not tested).

find_package(Git QUIET)

if (GIT_EXECUTABLE AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE CMD_RESULT
        OUTPUT_VARIABLE VCS_REVISION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )
endif()
# message( STATUS "VCS_REVISION: ${VCS_REVISION}" )

if(NOT DEFINED CMD_RESULT)
    set(VCS_BRANCH "master")
    set(GIT_COMMIT_HASH "1")
    set(VCS_REVISION "na")
else()
    execute_process(
        COMMAND git log -1 --format=%h
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    # message( STATUS "GIT_COMMIT_HASH: ${GIT_COMMIT_HASH}" )

    execute_process(
        COMMAND ${GIT_EXECUTABLE} status
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE CMD_RESULT
        OUTPUT_VARIABLE DESCRIBE_STATUS
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )

    string(REPLACE "\n" " " DESCRIBE_STATUS ${DESCRIBE_STATUS})
    string(REPLACE "\r" " " DESCRIBE_STATUS ${DESCRIBE_STATUS})
    string(REPLACE "\rn" " " DESCRIBE_STATUS ${DESCRIBE_STATUS})
    string(REPLACE " " ";" DESCRIBE_STATUS ${DESCRIBE_STATUS})
    list(GET DESCRIBE_STATUS 2 VCS_BRANCH)

    # message(STATUS "Branch: ${VCS_BRANCH}") # /${VCS_REVISION}")

    execute_process(
        COMMAND ${GIT_EXECUTABLE} config --get remote.origin.url
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE CMD_RESULT
        OUTPUT_VARIABLE VCS_URL
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    # message( STATUS "VCS_URL: ${VCS_URL}")

    set(ENV{LANG} "en_US")
    if(GIT_VERSION_STRING VERSION_LESS 2.6)
        set(CHANGELOG "")
    else()
        execute_process(
            COMMAND ${GIT_EXECUTABLE} log -n 10 "--date=format:%a %b %d %Y" "--pretty=format:* %ad %aN <%aE> %h - %s"
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE CMD_RESULT
            OUTPUT_VARIABLE CHANGELOG
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()
    file(WRITE "${CMAKE_BINARY_DIR}/changelog" "${CHANGELOG}")
endif()

string(TIMESTAMP DATE_VERSION "%Y%m%d")
string(TIMESTAMP CURRENT_TIME "%Y%m%d_%H:%M")

if (UNIX)
    execute_process(
        COMMAND uname -m
        WORKING_DIRECTORY "."
        OUTPUT_VARIABLE CPACK_ARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_GENERATOR TGZ)

    if (CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(CPACK_TARGET "")
        set(CPACK_GENERATOR ${CPACK_GENERATOR} DEB RPM)
        install(
            FILES utils/udev_rules/60-openhantek.rules
            DESTINATION lib/udev/rules.d
        )
    elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
        set(CPACK_TARGET "freebsd_")
        set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")
        install(
            FILES utils/devd_rules_freebsd/openhantek.conf
            DESTINATION etc/devd
        )
    elseif(APPLE)
        set(CPACK_TARGET "osx_")
    endif()

    # install documentation
    FILE(GLOB PDF "docs/*.pdf")
    install(
        FILES CHANGELOG LICENSE README ${PDF}
        DESTINATION share/doc/openhantek
    )
    # install application starter and icons
    install(
        FILES utils/applications/OpenHantek.desktop
        DESTINATION share/applications
        )
    install(
        FILES openhantek/res/images/OpenHantek.png
        DESTINATION share/icons/hicolor/48x48/apps
    )
    install(
        FILES openhantek/res/images/OpenHantek.svg
        DESTINATION share/icons/hicolor/scalable/apps
    )

elseif(WIN32)
    set(CPACK_TARGET "win_")
    set(CPACK_GENERATOR ${CPACK_GENERATOR} ZIP NSIS)
    if ((MSVC AND CMAKE_GENERATOR MATCHES "Win64+") OR (CMAKE_SIZEOF_VOID_P EQUAL 8))
        set(CPACK_ARCH "amd64")
    else()
        set(CPACK_ARCH "x86")
    endif()
endif()

message(STATUS "Package: ${CPACK_GENERATOR}")
message(STATUS "Architecture: ${CPACK_ARCH}")

set(CPACK_PACKAGE_NAME "openhantek")
string(TOLOWER ${CPACK_PACKAGE_NAME} CPACK_PACKAGE_NAME)
set(CPACK_PACKAGE_VERSION "${DATE_VERSION}-${VCS_REVISION}")
set(CPACK_PACKAGE_CONTACT "contact@openhantek.org")
set(CPACK_PACKAGE_VENDOR "OpenHantek Community")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Digital oscilloscope software for Hantek DSO6022 USB hardware")
set(CPACK_PACKAGE_DESCRIPTION "OpenHantek is an oscilloscope software for\nVoltcraft/Darkwire/Protek/Acetech/Hantek USB devices")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/readme.md")
if (EXISTS "${CMAKE_SOURCE_DIR}/LICENSE")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
endif()

set(CPACK_STRIP_FILES 1)

include(CMakeDetermineSystem)

# Linux DEB (tested on debian stretch and buster)
# Architecture for package and file name are automatically detected
set(CPACK_DEBIAN_PACKAGE_SECTION "electronics")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")

# Linux RPM (not tested on debian)
# Architecture for package and file name are automatically detected
set(CPACK_RPM_PACKAGE_RELOCATABLE NO)
set(CPACK_RPM_PACKAGE_LICENSE "GPLv2+")
set(CPACK_RPM_PACKAGE_DESCRIPTION ${CPACK_PACKAGE_DESCRIPTION})
set(CPACK_RPM_CHANGELOG_FILE "${CMAKE_BINARY_DIR}/changelog")
set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")

set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}-1_${CPACK_TARGET}${CPACK_ARCH}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY ".")
SET(CPACK_OUTPUT_FILE_PREFIX packages)

include(CPack)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
include(InstallRequiredSystemLibraries)

cpack_add_install_type(Full DISPLAY_NAME "All")

set(VERSION ${CPACK_PACKAGE_VERSION})
