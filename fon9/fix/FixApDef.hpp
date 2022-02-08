/// \file fon9/fix/FixApDef.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fix_FixApDef_hpp__
#define __fon9_fix_FixApDef_hpp__
#include "fon9/fix/FixBase.hpp"

namespace fon { namespace fix {

#define f9fix_kTAG_Account                     1
#define f9fix_kTAG_Symbol                     55
#define f9fix_kTAG_TransactTime               60
#define f9fix_kTAG_TradeDate                  75
#define f9fix_kTAG_OrderQty                   38
#define f9fix_kTAG_Price                      44
#define f9fix_kTAG_HandlInst                  21
#define f9fix_kTAG_OrderID                    37
#define f9fix_kTAG_ClOrdID                    11
#define f9fix_kTAG_OrigClOrdID                41

#define f9fix_kTAG_NoPartyIDs                453
#define f9fix_kTAG_PartyID                   448
#define f9fix_kTAG_PartyIDSource             447
#define f9fix_kTAG_PartyRole                 452

#define f9fix_kTAG_OrdType                         40 // Price Type.
#define f9fix_kVAL_OrdType_Market                  "1"
#define f9fix_kVAL_OrdType_Limit                   "2"
#define f9fix_kVAL_OrdType_Stop                    "3"
#define f9fix_kVAL_OrdType_StopLimit               "4"
#define f9fix_kVAL_OrdType_MarketOnClose           "5"   // No longer used.
#define f9fix_kVAL_OrdType_WithOrWithout           "6"
#define f9fix_kVAL_OrdType_LimitOrBetter           "7"   // Deprecated.
#define f9fix_kVAL_OrdType_LimitWithOrWithout      "8"
#define f9fix_kVAL_OrdType_OnBasis                 "9"
#define f9fix_kVAL_OrdType_OnClose                 "A"   // No longer used.
#define f9fix_kVAL_OrdType_LimitOnClose            "B"   // No longer used.
#define f9fix_kVAL_OrdType_Forex                   "C"   // No longer used.
#define f9fix_kVAL_OrdType_PreviouslyQuoted        "D"
#define f9fix_kVAL_OrdType_PreviouslyIndicated     "E"
#define f9fix_kVAL_OrdType_ForexLimit              "F"   // No longer used.
#define f9fix_kVAL_OrdType_ForexSwap               "G"
#define f9fix_kVAL_OrdType_ForexPreviouslyQuoted   "H"   // No longer used.
#define f9fix_kVAL_OrdType_Funari                  "I"   // Limit Day Order with unexecuted portion handled as Market On Close.E.g.Japan.
#define f9fix_kVAL_OrdType_MarketIfTouched         "J"   // MIT.
#define f9fix_kVAL_OrdType_MarketWithLeftover_as_Limit   "K" // market order then unexecuted quantity becomes limit order at last price.
#define f9fix_kVAL_OrdType_HistoricPricing         "L"   // Previous Fund Valuation Point (Historic pricing) (for CIV)

#define f9fix_kTAG_TimeInForce                     59
#define f9fix_kVAL_TimeInForce_Day                 "0"
#define f9fix_kVAL_TimeInForce_GoodTillCancel      "1"
#define f9fix_kVAL_TimeInForce_GTC                 "1"
#define f9fix_kVAL_TimeInForce_AtTheOpening        "2"
#define f9fix_kVAL_TimeInForce_OPG                 "2"
#define f9fix_kVAL_TimeInForce_ImmediateOrCancel   "3"
#define f9fix_kVAL_TimeInForce_IOC                 "3"
#define f9fix_kVAL_TimeInForce_FillOrKill          "4"
#define f9fix_kVAL_TimeInForce_FOK                 "4"
#define f9fix_kVAL_TimeInForce_GoodTillCrossing    "5"
#define f9fix_kVAL_TimeInForce_GTX                 "5"
#define f9fix_kVAL_TimeInForce_GoodTillDate        "6"

#define f9fix_kTAG_ExecID                     17
#define f9fix_kTAG_SecondaryExecID           527
#define f9fix_kTAG_LastPx                     31
#define f9fix_kTAG_LastQty                    32
#define f9fix_kTAG_LeavesQty                 151
#define f9fix_kTAG_CumQty                     14
#define f9fix_kTAG_AvgPx                       6

#define f9fix_kTAG_Side                       54
#define f9fix_kVAL_Side_Buy                  "1"
#define f9fix_kVAL_Side_Sell                 "2"

#define f9fix_kTAG_PosEff                     77
#define f9fix_kVAL_PosEff_Open                "O"
#define f9fix_kVAL_PosEff_Close               "C"
#define f9fix_kVAL_PosEff_Rolled              "R"
#define f9fix_kVAL_PosEff_FIFO                "F"

#define f9fix_kTAG_CxlRejResponseTo              434
#define f9fix_kVAL_CxlRejResponseTo_Cancel       "1" // Order Cancel Request
#define f9fix_kVAL_CxlRejResponseTo_Replace      "2" // Order Cancel / Replace Request

#define f9fix_kTAG_CxlRejReason                  102
#define f9fix_kVAL_CxlRejReason_TooLate          "0" // Too late to cancel
#define f9fix_kVAL_CxlRejReason_UnknownOrder     "1" // Unknown order
#define f9fix_kVAL_CxlRejReason_BrokerExchange   "2" // Broker / Exchange Option
#define f9fix_kVAL_CxlRejReason_AlreadyAmending  "3" // Order already in Pending Cancel or Pending Replace status
#define f9fix_kVAL_CxlRejReason_UnableMass       "4" // Unable to process Order Mass Cancel Request
#define f9fix_kVAL_CxlRejReason_BadTime          "5" // OrigOrdModTime#586 did not match last TransactTime#60 of order
#define f9fix_kVAL_CxlRejReason_DupClOrdID       "6" // Duplicate ClOrdID#11 received
#define f9fix_kVAL_CxlRejReason_Other           "99"

#define f9fix_kTAG_OrdRejReason                     103
#define f9fix_kVAL_OrdRejReason_BrokerExchange      "0" // Broker / Exchange option
#define f9fix_kVAL_OrdRejReason_UnknownSymbol       "1" // Unknown symbol
#define f9fix_kVAL_OrdRejReason_ExchangeClosed      "2" // Exchange closed
#define f9fix_kVAL_OrdRejReason_ExceedsLimit        "3" // Order exceeds limit
#define f9fix_kVAL_OrdRejReason_TooLate             "4" // Too late to enter
#define f9fix_kVAL_OrdRejReason_UnknownOrder        "5" // Unknown Order
#define f9fix_kVAL_OrdRejReason_DupClOrdID          "6" // Duplicate Order (e.g. dupe ClOrdID#11)
#define f9fix_kVAL_OrdRejReason_DupVerbal           "7" // Duplicate of a verbally communicated order
#define f9fix_kVAL_OrdRejReason_Stale               "8" // Stale Order
#define f9fix_kVAL_OrdRejReason_TradeAlongReq       "9" // Trade Along required
#define f9fix_kVAL_OrdRejReason_InvalidInvestorID  "10" // Invalid Investor ID
#define f9fix_kVAL_OrdRejReason_Unsupported        "11" // Unsupported order characteristic
#define f9fix_kVAL_OrdRejReason_Surveillence       "12" // Surveillence Option
#define f9fix_kVAL_OrdRejReason_IncorrectQty       "13" // Incorrect quantity
#define f9fix_kVAL_OrdRejReason_IncorrectAllocQty  "14" // Incorrect allocated quantity
#define f9fix_kVAL_OrdRejReason_UnknownAccount     "15" // Unknown account(s)
#define f9fix_kVAL_OrdRejReason_Other              "99"

#define f9fix_kTAG_ExecTransType             20 // FIX.4.2
#define f9fix_kVAL_ExecTransType_New         "0"
#define f9fix_kVAL_ExecTransType_Status      "3"

#define f9fix_kTAG_ExecType                  150
#define f9fix_kVAL_ExecType_New              "0"
#define f9fix_kVAL_ExecType_PartialFill_42   "1"
#define f9fix_kVAL_ExecType_Fill_42          "2"
#define f9fix_kVAL_ExecType_DoneForDay       "3"
#define f9fix_kVAL_ExecType_Canceled         "4"
#define f9fix_kVAL_ExecType_Replaced         "5"
#define f9fix_kVAL_ExecType_PendingCancel    "6"
#define f9fix_kVAL_ExecType_Stopped          "7"
#define f9fix_kVAL_ExecType_Rejected         "8"
#define f9fix_kVAL_ExecType_Suspended        "9"
#define f9fix_kVAL_ExecType_PendingNew       "A"
#define f9fix_kVAL_ExecType_Calculated       "B"
#define f9fix_kVAL_ExecType_Expired          "C"
#define f9fix_kVAL_ExecType_Restated         "D"
#define f9fix_kVAL_ExecType_PendingReplace   "E"
#define f9fix_kVAL_ExecType_Trade_44         "F"//FIX.4.4: Trade (partial fill or fill)
#define f9fix_kVAL_ExecType_TradeCorrect     "G"//FIX.4.4: Trade Correct (formerly an ExecTransType#20)
#define f9fix_kVAL_ExecType_TradeCancel      "H"//FIX.4.4: (formerly an ExecTransType#20)
#define f9fix_kVAL_ExecType_OrderStatus      "I"//FIX.4.4: (formerly an ExecTransType#20)

/// \ingroup fix
/// OrderStatus, 台灣證交所:
/// - FIX.4.2: = ExecType(Tag#150).
/// - FIX.4.4: 除底下說明, 其餘與 ExecType(Tag#150) 相同:
///   - 改單成功: ExecType=Replace; OrdStatus=New
///   - 查詢成功: ExecType=Status;  OrdStatus=New
///   - 成交回報: ExecType=Trade;   部份成交 OrdStatus=PartialFill; 全部成交 OrdStatus=Fill
#define f9fix_kTAG_OrdStatus                 39
#define f9fix_kVAL_OrdStatus_New             "0"
#define f9fix_kVAL_OrdStatus_PartiallyFilled "1"
#define f9fix_kVAL_OrdStatus_Filled          "2"
#define f9fix_kVAL_OrdStatus_DoneForDay      "3"
#define f9fix_kVAL_OrdStatus_Canceled        "4"
#define f9fix_kVAL_OrdStatus_Replaced_42     "5"
#define f9fix_kVAL_OrdStatus_PendingCancel   "6"
#define f9fix_kVAL_OrdStatus_Stopped         "7"
#define f9fix_kVAL_OrdStatus_Rejected        "8"
#define f9fix_kVAL_OrdStatus_Suspended       "9"
#define f9fix_kVAL_OrdStatus_PendingNew      "A"
#define f9fix_kVAL_OrdStatus_Calculated      "B"
#define f9fix_kVAL_OrdStatus_Expired         "C"
#define f9fix_kVAL_OrdStatus_AcceptedForBidding "D"
#define f9fix_kVAL_OrdStatus_PendingReplace  "E"

// TWSE fields.
#if 1
#define f9fix_kTAG_TwseIvacnoFlag            10000
#define f9fix_kTAG_TwseOrdType               10001

// '0'=Regular, '3'=Foreign stock’s order price over up/down limit flag.
#define f9fix_kTAG_TwseExCode                10002

// Regular Checks the TransactTime to verify that it is within a given seconds of the system time.
// Y: if not, reject it.
// N: don’t check TransactTime.
#define f9fix_kTAG_TwseRejStaleOrd           10004
#endif// TWSE fields.

#define f9fix_kMSGTYPE_NewOrderSingle        "D"
#define f9fix_kMSGTYPE_OrderReplaceRequest   "G"
#define f9fix_kMSGTYPE_OrderCancelRequest    "F"
#define f9fix_kMSGTYPE_OrderStatusRequest    "H"

#define f9fix_kMSGTYPE_ExecutionReport       "8"
#define f9fix_kMSGTYPE_OrderCancelReject     "9"

#define f9fix_kMSGTYPE_NewOrderMultileg            "AB"
#define f9fix_kMSGTYPE_MultilegOrderReplaceRequest "AC"
#define f9fix_kTAG_NoLegs                          555
#define f9fix_kTAG_LegSymbol                       600
#define f9fix_kTAG_LegCFICode                      608
#define f9fix_kTAG_LegMaturityMonthYear            610
#define f9fix_kTAG_LegSide                         624
#define f9fix_kTAG_LegQty                          687
#define f9fix_kTAG_LegLastPx                       637
#define f9fix_kTAG_LegStrikePrice                  612

} } // namespaces
#endif//__fon9_fix_FixApDef_hpp__
