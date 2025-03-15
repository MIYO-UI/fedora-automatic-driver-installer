cmake_minimum_required(VERSION 3.10)
project(auto-driver-installer VERSION 1.0.0 LANGUAGES CXX)

# Ustawienia kompilatora
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Źródła programu
set(SOURCES
    auto-driver-installer.cpp
)

# Utwórz plik wykonywalny
add_executable(auto-driver-installer ${SOURCES})

# Biblioteki systemowe
find_package(Threads REQUIRED)
target_link_libraries(auto-driver-installer PRIVATE Threads::Threads)

# Instalacja
install(TARGETS auto-driver-installer DESTINATION bin)
install(FILES auto-driver-installer.desktop DESTINATION share/applications)

# Utwórz katalog konfiguracyjny
install(CODE "execute_process(COMMAND mkdir -p \$ENV{DESTDIR}/var/lib/driver-installer)")
install(CODE "execute_process(COMMAND chmod 755 \$ENV{DESTDIR}/var/lib/driver-installer)")

# Instalacja systemu jednostek systemd
install(FILES auto-driver-installer.service DESTINATION /usr/lib/systemd/system)

# Opcjonalnie włącz usługę
option(ENABLE_SERVICE "Enable the auto-driver-installer service" OFF)
if(ENABLE_SERVICE AND NOT DEFINED ENV{DESTDIR})
    install(CODE "execute_process(COMMAND systemctl enable auto-driver-installer.service)")
endif()

# Konfigurator pakietu RPM
set(CPACK_PACKAGE_NAME "auto-driver-installer")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_VENDOR "Fedora")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Automatic graphics driver installer for Fedora")
set(CPACK_PACKAGE_DESCRIPTION "Automatically detects and installs the recommended graphics drivers for your hardware")
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0")
set(CPACK_RPM_PACKAGE_GROUP "System Environment/Base")
set(CPACK_RPM_PACKAGE_REQUIRES "dnf, pciutils")

include(CPack)