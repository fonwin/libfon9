set(f9twf_src
 ExgTmpTypes.cpp
 ExgLineTmpArgs.cpp
 ExgLineTmpLog.cpp
 ExgLineTmpSession.cpp
 ExgLineTmpFactory.cpp
 ExgTradingLineMgr.cpp
 ExgMapFcmId.cpp
 ExgMapSessionDn.cpp
 ExgMapMgr.cpp

 ExgMdPkReceiver.cpp
 ExgMdSymbs.cpp
 ExgMcReceiver.cpp
 ExgMcReceiverFactory.cpp
 ExgMcChannel.cpp
 ExgMcGroup.cpp
 ExgMcToMiConv.cpp
 ExgMrRecover.cpp
 ExgMdFmtParsers.cpp
 ExgMdFmtMatchToRts.cpp
 ExgMdFmtBSToRts.cpp
 ExgMdFmtBasicInfoToRts.cpp
)
add_library(f9twf_s STATIC ${f9twf_src})
target_link_libraries(f9twf_s pthread rt dl fon9_s)

add_library(f9twf SHARED ${f9twf_src})
target_link_libraries(f9twf pthread rt dl fon9)

# unit tests
add_executable(f9twf_SymbId_UT f9twf_SymbId_UT.cpp)
target_link_libraries(f9twf_SymbId_UT fon9_s f9twf_s)

add_executable(f9twfExgMkt_UT ExgMkt_UT.cpp)
target_link_libraries(f9twfExgMkt_UT fon9_s f9twf_s f9extests_s)
