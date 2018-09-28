cmake_minimum_required(VERSION 2.8)
project(FaceRecognize)

find_package(OpenCV REQUIRED)

link_directories(../lib/dahua ../lib/arcsoft)

add_executable(FaceRecognize
               main.cpp)
target_link_libraries(FaceRecognize ${OpenCV_LIBS})
target_link_libraries(FaceRecognize -larcsoft_fsdk_face_detection -larcsoft_fsdk_face_recognition)
target_link_libraries(FaceRecognize -ldhnetsdk -ldhplay)
target_link_libraries(FaceRecognize -lSDL2)
