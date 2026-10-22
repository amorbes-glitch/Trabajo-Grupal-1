cmake_minimum_required(VERSION 3.16)
project(ElBosquePerdioSusSombras LANGUAGES CXX)

add_executable(ElBosquePerdioSusSombras WIN32 main.cpp)
target_compile_features(ElBosquePerdioSusSombras PRIVATE cxx_std_17)
target_link_libraries(ElBosquePerdioSusSombras PRIVATE user32 gdi32 winmm)
