#
# 在 CMakeLists.txt, 檢查是否有支援 f9card 的 source code.
# 因 ${f9_s_lib} 須連結 fon9, 所以必須在 add_subdirectory(fon9) 之後匯入此檔:
#  add_subdirectory(fon9)
#  include(fon9/AddF9card.cmake)
#
if(NOT DEFINED F9PATH)
  set(F9PATH "${CMAKE_CURRENT_SOURCE_DIR}/f9")
  if(EXISTS "${F9PATH}/net" AND IS_DIRECTORY "${F9PATH}/net")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DF9CARD")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -DF9CARD")
    set(f9_s_lib "f9_s")
    add_subdirectory(f9)
  else()
    unset(F9PATH)
  endif()
endif()
