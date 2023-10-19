set(f9tws_src
 ExgMdPkReceiver.cpp
 ExgMdPlayer.cpp
 ExgMdIndices.cpp
 ExgMdSymbs.cpp
 ExgMdSystemBase.cpp
 ExgMdSystem.cpp
 ExgMdFmt1Handler.cpp
 ExgMdFmt6Handler.cpp
 ExgMdFmt9Handler.cpp
 ExgMdFmt19Handler.cpp

 # 零股
 ExgMdFmt22Handler.cpp
 ExgMdFmt23Handler.cpp
 ExgMdFmt13Handler.cpp

 # 指數
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
 
 # 額外的型別/工具.
 TwsTools.cpp
)
add_library(f9tws_s STATIC ${f9tws_src})
target_link_libraries(f9tws_s fon9_s pthread rt dl)

add_library(f9tws SHARED ${f9tws_src})
target_link_libraries(f9tws pthread rt dl)

#########################################################################
############################### Unit Test ###############################
if(CMAKE_FON9_BUILD_UNIT_TEST)

   add_executable(f9twsExgMkt_UT ExgMkt_UT.cpp)
   target_link_libraries(f9twsExgMkt_UT fon9_s f9tws_s f9extests_s)

   add_executable(f9twsTrade_UT f9twsTrade_UT.cpp ../fon9/framework/Fon9CoRun.cpp)
   target_link_libraries(f9twsTrade_UT fon9_s f9tws_s)

endif()
############################## Unit Test END ############################
#########################################################################
