set(f9omstw_src
 IvacNo.cpp
 OmsCore.cpp
 OmsTools.cpp
 OmsTree.cpp
 OmsIvBase.cpp
 OmsBrk.cpp
 OmsBrkTree.cpp
 OmsIvac.cpp
 OmsIvacTree.cpp
 OmsSubac.cpp
 OmsIvSymb.cpp
 OmsSymbTree.cpp
)
add_library(f9omstw_s STATIC ${f9omstw_src})
target_link_libraries(f9omstw_s fon9_s)
