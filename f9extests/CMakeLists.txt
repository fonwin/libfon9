set(f9extests_src
 SymbIn.cpp
 SymbDyTest.cpp
)
add_library(f9extests_s STATIC ${f9extests_src})
target_link_libraries(f9extests_s pthread fon9_s)

add_library(f9extests SHARED ${f9extests_src})
target_link_libraries(f9extests pthread fon9)
