set(f9omstw_src
 ../../fon9/framework/Fon9Co.cpp
 tws/TwsOmsCore.cpp
 tws/TwsSymb.cpp
 tws/TwsIvSymb.cpp
 tws/TwsIvSc.cpp
 tws/TwsSubac.cpp
 tws/TwsIvac.cpp
 tws/TwsBrk.cpp
)
include_directories(BEFORE .)
add_executable(f9omstw ${f9omstw_src})
target_link_libraries(f9omstw fon9_s f9omstw_s)
