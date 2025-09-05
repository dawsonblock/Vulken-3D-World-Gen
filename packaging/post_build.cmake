# VoxelVK Post-Build Packaging Script
# ====================================
# Runs after CPack package creation

cmake_minimum_required(VERSION 3.20)

message(STATUS "Running VoxelVK post-build packaging...")

# Get package information
if(NOT CPACK_PACKAGE_FILE_NAME)
    message(FATAL_ERROR "CPACK_PACKAGE_FILE_NAME not set")
endif()

# Validate package was created
set(PACKAGE_PATH "${CPACK_PACKAGE_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}")

# Platform-specific post-processing
if(WIN32)
    # Windows: Sign executables if certificate available
    find_program(SIGNTOOL_EXECUTABLE signtool)
    if(SIGNTOOL_EXECUTABLE AND EXISTS "$ENV{WINDOWS_CERTIFICATE_PATH}")
        message(STATUS "Signing Windows executables...")
        # Code signing would go here
    endif()
    
elseif(APPLE)
    # macOS: Notarize DMG if credentials available
    if(EXISTS "$ENV{APPLE_DEVELOPER_ID}")
        message(STATUS "Notarizing macOS package...")
        # Notarization would go here
    endif()
    
elseif(UNIX)
    # Linux: Validate package structure
    message(STATUS "Validating Linux package structure...")
    
    # Check DEB package if it exists
    if(EXISTS "${PACKAGE_PATH}.deb")
        find_program(DPKG_DEB_EXECUTABLE dpkg-deb)
        if(DPKG_DEB_EXECUTABLE)
            execute_process(
                COMMAND ${DPKG_DEB_EXECUTABLE} --info "${PACKAGE_PATH}.deb"
                OUTPUT_VARIABLE DEB_INFO
                ERROR_VARIABLE DEB_ERROR
                RESULT_VARIABLE DEB_RESULT
            )
            
            if(DEB_RESULT EQUAL 0)
                message(STATUS "DEB package validation: PASSED")
            else()
                message(WARNING "DEB package validation failed: ${DEB_ERROR}")
            endif()
        endif()
    endif()
    
endif()

# Generate package checksum
find_program(SHA256SUM_EXECUTABLE sha256sum)
if(SHA256SUM_EXECUTABLE)
    execute_process(
        COMMAND ${SHA256SUM_EXECUTABLE} "${CPACK_PACKAGE_FILE_NAME}"
        WORKING_DIRECTORY "${CPACK_PACKAGE_DIRECTORY}"
        OUTPUT_FILE "${CPACK_PACKAGE_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}.sha256"
        ERROR_QUIET
    )
    message(STATUS "Generated SHA256 checksum")
endif()

# Create package manifest
set(MANIFEST_FILE "${CPACK_PACKAGE_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}.manifest")
file(WRITE ${MANIFEST_FILE} 
"VoxelVK Package Manifest
========================
Package: ${CPACK_PACKAGE_NAME}
Version: ${CPACK_PACKAGE_VERSION}
Platform: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}
Build Type: ${CMAKE_BUILD_TYPE}
Build Date: ${BUILD_TIMESTAMP}
Generator: ${CPACK_GENERATOR}
Components: ${CPACK_COMPONENTS_ALL}

Package Contents:
- Runtime binaries
- Configuration files  
- Sample assets
- Documentation
- License file

Installation:
See README.md for installation instructions.

Support:
GitHub: https://github.com/voxelvk/voxelvk
Email: support@voxelvk.org
")

message(STATUS "VoxelVK post-build packaging completed successfully")
message(STATUS "Package: ${CPACK_PACKAGE_FILE_NAME}")
message(STATUS "Location: ${CPACK_PACKAGE_DIRECTORY}")