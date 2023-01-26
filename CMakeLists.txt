# CMakeList.txt: файл проекта CMake верхнего уровня; проведите глобальную конфигурацию
# и включите сюда подпроекты.
#
cmake_minimum_required (VERSION 3.8)

project ("si_client_server_filetransferer")

# Включите подпроекты.
add_subdirectory ("si_client")
add_subdirectory ("si_server")
