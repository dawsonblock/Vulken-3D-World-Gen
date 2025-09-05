# VoxelVK CPack Configuration
# ===========================
# Configures packaging for multiple platforms

# Basic package information
set(CPACK_PACKAGE_NAME "VoxelVK")
set(CPACK_PACKAGE_VENDOR "VoxelVK Team")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "6")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "VoxelVK - High-performance voxel rendering engine")
set(CPACK_PACKAGE_DESCRIPTION "VoxelVK is a modern, high-performance voxel rendering engine built with Vulkan. It features advanced lighting, weather systems, and AI-driven world generation.")

set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/voxelvk/voxelvk")
set(CPACK_PACKAGE_CONTACT "support@voxelvk.org")

# License and readme
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

# Package file name
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

# Installation directories
set(CPACK_PACKAGE_INSTALL_DIRECTORY "VoxelVK")
set(CPACK_PACKAGE_EXECUTABLES 
    "smoke_headless;VoxelVK Headless Test"
    "main_imgui_vulkan;VoxelVK GUI Demo"
)

# Platform-specific configurations
if(WIN32)
    # Windows-specific settings
    set(CPACK_GENERATOR "NSIS;ZIP")
    
    # NSIS installer settings
    set(CPACK_NSIS_DISPLAY_NAME "VoxelVK ${CPACK_PACKAGE_VERSION}")
    set(CPACK_NSIS_PACKAGE_NAME "VoxelVK")
    set(CPACK_NSIS_HELP_LINK "https://github.com/voxelvk/voxelvk")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/voxelvk/voxelvk")
    set(CPACK_NSIS_CONTACT "support@voxelvk.org")
    set(CPACK_NSIS_MODIFY_PATH ON)
    
    # Start menu shortcuts
    set(CPACK_NSIS_MENU_LINKS
        "bin/main_imgui_vulkan.exe" "VoxelVK GUI Demo"
        "README.md" "README"
        "LICENSE" "License"
    )
    
    # Registry settings
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin")
    
    # Include Visual C++ Redistributable check
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
        ; Check for Visual C++ Redistributable
        ReadRegStr $0 HKLM 'SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x64' 'Version'
        StrCmp $0 '' vcredist_missing vcredist_found
        vcredist_missing:
            MessageBox MB_YESNO 'Visual C++ Redistributable 2019 is required. Download and install it?' IDNO vcredist_skip
            ExecShell 'open' 'https://aka.ms/vs/16/release/vc_redist.x64.exe'
        vcredist_skip:
        vcredist_found:
    ")
    
elseif(APPLE)
    # macOS-specific settings
    set(CPACK_GENERATOR "DragNDrop;TGZ")
    
    # DMG settings
    set(CPACK_DMG_VOLUME_NAME "VoxelVK ${CPACK_PACKAGE_VERSION}")
    set(CPACK_DMG_FORMAT "UDZO")
    set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/docs/images/dmg_background.png")
    
else()
    # Linux-specific settings
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    
    # DEB package settings
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "VoxelVK Team <support@voxelvk.org>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS 
        "libgl1-mesa-glx, libx11-6, libxrandr2, libxinerama1, libxcursor1, libxi6, libc6 (>= 2.27)")
    set(CPACK_DEBIAN_PACKAGE_SUGGESTS "vulkan-tools, mesa-vulkan-drivers")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/voxelvk/voxelvk")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA 
        "${CMAKE_SOURCE_DIR}/packaging/debian/postinst"
        "${CMAKE_SOURCE_DIR}/packaging/debian/prerm")
    
    # RPM package settings
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Graphics")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_URL "https://github.com/voxelvk/voxelvk")
    set(CPACK_RPM_PACKAGE_REQUIRES 
        "mesa-libGL, libX11, libXrandr, libXinerama, libXcursor, libXi, glibc >= 2.27")
    set(CPACK_RPM_PACKAGE_SUGGESTS "vulkan-tools, mesa-vulkan-drivers")
    
endif()

# Components for fine-grained packaging
set(CPACK_COMPONENTS_ALL Runtime Development Samples Documentation)

# Runtime component
set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "VoxelVK Runtime")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "Core VoxelVK runtime files and executables")
set(CPACK_COMPONENT_RUNTIME_REQUIRED ON)

# Development component  
set(CPACK_COMPONENT_DEVELOPMENT_DISPLAY_NAME "Development Files")
set(CPACK_COMPONENT_DEVELOPMENT_DESCRIPTION "Headers and libraries for VoxelVK development")
set(CPACK_COMPONENT_DEVELOPMENT_DEPENDS Runtime)

# Samples component
set(CPACK_COMPONENT_SAMPLES_DISPLAY_NAME "Sample Assets")
set(CPACK_COMPONENT_SAMPLES_DESCRIPTION "Sample textures, meshes, and voxel data")
set(CPACK_COMPONENT_SAMPLES_DEPENDS Runtime)

# Documentation component
set(CPACK_COMPONENT_DOCUMENTATION_DISPLAY_NAME "Documentation")
set(CPACK_COMPONENT_DOCUMENTATION_DESCRIPTION "User guides and API documentation")

# Archive settings
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)

# Source package settings (for source distributions)
set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
set(CPACK_SOURCE_IGNORE_FILES
    "\\\\.git/"
    "\\\\.github/"
    "build.*/"
    "\\\\.cache/"
    "vcpkg_installed/"
    "\\\\.vs/"
    "\\\\.vscode/"
    "CMakeFiles/"
    "CMakeCache\\\\.txt"
    "\\\\.DS_Store"
    "Thumbs\\\\.db"
    "\\\\.gitignore"
    "\\\\.clang-format"
    "\\\\.clang-tidy"
)

# Install rules for components
install(TARGETS smoke_headless
    RUNTIME DESTINATION bin
    COMPONENT Runtime
)

if(NOT VOXELVK_HEADLESS_ONLY)
    install(TARGETS main_imgui_vulkan
        RUNTIME DESTINATION bin
        COMPONENT Runtime
    )
endif()

# Install configuration files
install(DIRECTORY config/
    DESTINATION config
    COMPONENT Runtime
    PATTERN "*.example.yaml" EXCLUDE
)

# Install sample assets
install(DIRECTORY assets/samples/
    DESTINATION assets
    COMPONENT Samples
)

# Install documentation
install(FILES README.md LICENSE CHANGELOG.md
    DESTINATION .
    COMPONENT Documentation
)

install(DIRECTORY docs/
    DESTINATION docs
    COMPONENT Documentation
    PATTERN "*.md"
)

# Install scripts
install(PROGRAMS scripts/fetch_assets.py
    DESTINATION scripts
    COMPONENT Runtime
)

# Create desktop entry for Linux
if(UNIX AND NOT APPLE)
    configure_file(
        "${CMAKE_SOURCE_DIR}/packaging/linux/voxelvk.desktop.in"
        "${CMAKE_BINARY_DIR}/voxelvk.desktop"
        @ONLY
    )
    
    install(FILES "${CMAKE_BINARY_DIR}/voxelvk.desktop"
        DESTINATION share/applications
        COMPONENT Runtime
    )
    
    # Install icon
    if(EXISTS "${CMAKE_SOURCE_DIR}/docs/images/voxelvk-icon.png")
        install(FILES "${CMAKE_SOURCE_DIR}/docs/images/voxelvk-icon.png"
            DESTINATION share/pixmaps
            RENAME voxelvk.png
            COMPONENT Runtime
        )
    endif()
endif()

# Post-install scripts
set(CPACK_POST_BUILD_SCRIPTS "${CMAKE_SOURCE_DIR}/packaging/post_build.cmake")

# Debug info
message(STATUS "CPack Configuration:")
message(STATUS "  Package Name: ${CPACK_PACKAGE_NAME}")
message(STATUS "  Version: ${CPACK_PACKAGE_VERSION}")
message(STATUS "  Generators: ${CPACK_GENERATOR}")
message(STATUS "  File Name: ${CPACK_PACKAGE_FILE_NAME}")

include(CPack)