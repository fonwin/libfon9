#
# 在 CMakeLists.txt, 檢查是否有支援 f9card 的 source code.
# 要在 add_subdirectory(fon9) 之前使用 include() 加入此檔,
# 避免在多個路徑下皆有 f9/ 造成 libf9_s 重複定義;
#
#  include(fon9/AddF9card.cmake)
#  add_subdirectory(fon9)
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
