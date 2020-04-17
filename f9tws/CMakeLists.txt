set(f9tws_src
 ExgMdPkReceiver.cpp
 ExgMdPlayer.cpp
 ExgMdIndices.cpp
 ExgMdSymbs.cpp
 ExgMdSystem.cpp
 ExgMdFmt1Handler.cpp
 ExgMdFmt6Handler.cpp
 ExgMdFmt9Handler.cpp
 ExgMdFmt19Handler.cpp
 ExgMdFmtIxHandler.cpp
 ExgMdFmtIxNoList.cpp
 ExgMdReceiverSession.cpp
 ExgMdReceiverFactory.cpp

 ExgTradingLineMgr.cpp
 ExgLineFactory.cpp
 ExgLineArgs.cpp
 ExgLineTmp.cpp
 ExgLineTmpFactory.cpp
 ExgTradingLineFix.cpp
 ExgTradingLineFixFactory.cpp
)
add_library(f9tws_s STATIC ${f9tws_src})
target_link_libraries(f9tws_s pthread rt dl)

add_library(f9tws SHARED ${f9tws_src})
target_link_libraries(f9tws pthread rt dl)

# unit tests
add_executable(f9twsExgMkt_UT ExgMkt_UT.cpp)
target_link_libraries(f9twsExgMkt_UT fon9_s f9tws_s f9extests_s)
