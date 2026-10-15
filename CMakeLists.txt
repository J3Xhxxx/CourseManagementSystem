cmake_minimum_required(VERSION 3.24)
project(CoursePlanner VERSION 0.8.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 核心模型和算法可独立编译，Qt 只负责文件读写和界面。
option(COURSE_PLANNER_WITH_QT "Build Qt GUI and file tools" ON)
if(COURSE_PLANNER_WITH_QT)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Core Widgets)
endif()

add_library(course_core
    src/core/structures/TimeBitSet.cpp
    src/core/model/Meeting.cpp
    src/core/model/TeachingClass.cpp
    src/core/model/Schedule.cpp
    src/core/model/UserConstraints.cpp
    src/core/algorithms/ConflictDetector.cpp
    src/core/algorithms/ScheduleGenerator.cpp
    src/core/algorithms/ScheduleEvaluator.cpp
    src/core/algorithms/ScheduleValidator.cpp
    src/core/algorithms/BruteForceGenerator.cpp
    src/core/structures/ConflictGraph.cpp
    src/core/services/TeachingClassService.cpp
)
target_include_directories(course_core PUBLIC src)
set(planner_libraries course_core)

add_executable(course_demo src/demo/main.cpp)
target_link_libraries(course_demo PRIVATE course_core)

add_executable(course_benchmark src/demo/benchmark_main.cpp)
target_link_libraries(course_benchmark PRIVATE course_core)

if(COURSE_PLANNER_WITH_QT)
    add_library(course_io src/io/CsvImporter.cpp src/io/JsonProjectStore.cpp)
    target_link_libraries(course_io PUBLIC course_core Qt6::Core)
    add_executable(course_data src/demo/data_main.cpp)
    target_link_libraries(course_data PRIVATE course_io)
    add_library(course_ui
        src/ui/LoginDialog.cpp src/ui/LoginDialog.h
        src/ui/ProjectController.cpp src/ui/ProjectController.h
        src/ui/MainWindow.cpp src/ui/MainWindow.h
        src/ui/TeachingClassPanel.cpp src/ui/TeachingClassPanel.h
        src/ui/TimetableWidget.cpp src/ui/TimetableWidget.h
        src/ui/TimetableConverterWidget.cpp src/ui/TimetableConverterWidget.h
        src/ui/ConflictPanel.cpp src/ui/ConflictPanel.h
        src/ui/TeachingClassDialog.cpp src/ui/TeachingClassDialog.h
        src/ui/SearchController.cpp src/ui/SearchController.h
        src/ui/CandidatePanel.cpp src/ui/CandidatePanel.h
        src/ui/ConstraintPanel.cpp src/ui/ConstraintPanel.h
        src/ui/ReverseSpinBox.h
    )
    set_target_properties(course_ui PROPERTIES AUTOMOC ON)
    target_link_libraries(course_ui PUBLIC course_io Qt6::Widgets)
    list(APPEND planner_libraries course_io course_ui)
    add_executable(course_app src/app/main.cpp)
    target_link_libraries(course_app PRIVATE course_ui)
endif()

if(MSVC)
    target_compile_options(course_core PUBLIC /utf-8)
endif()
foreach(target IN LISTS planner_libraries)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
endforeach()
