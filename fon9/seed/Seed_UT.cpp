// \file fon9/Seed_UT.cpp
//
// test: Named/Field/FieldMaker/Seed(Raw)
//
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/seed/Tab.hpp"
#include "fon9/TypeName.hpp"
#include "fon9/TestTools.hpp"

//--------------------------------------------------------------------------//

enum class EnumInt {
   Value0,
   Value1,
};

using EnumHex = fon9::FmtFlag;

enum class EnumChar : char {
   CharA = 'a',
   CharB = 'b',
};

fon9_WARN_DISABLE_PADDING;
struct Data {
   fon9_NON_COPY_NON_MOVE(Data);
   Data() = default;

   const std::string ConstStdStrValue_{"ConstStdStrValue"};
   std::string       StdStrValue_{"StdStrValue"};
   fon9::CharVector  CharVectorValue_{fon9::StrView{"CharVectorValue"}};

   const char        ConstCharValue_{'C'};
   char              CharValue_{'c'};
   const char        ConstChar4Value_[4]{'A', 'B', 'C', 'D'};
   char              Char4Value_[4]{'a', 'b', 'c', 'd'};

   using Char4Ary = fon9::CharAry<4>;
   const Char4Ary    ConstChar4Ary_{"WXYZ"};
   Char4Ary          Char4Ary_{"wxyz"};

   using Char7AryL = fon9::CharAryL<7>;
   const Char7AryL   ConstChar7Ary_{"4321"};
   Char7AryL         Char7Ary_{"1234567"};

   const int         ConstIntValue_{1234};
   unsigned          IntValue_{1234u};
   const EnumInt     ConstEnumIntValue_{EnumInt::Value0};
   EnumInt           EnumIntValue_{EnumInt::Value1};
   const EnumChar    ConstEnumCharValue_{EnumChar::CharA};
   EnumChar          EnumCharValue_{EnumChar::CharB};
   const EnumHex     ConstEnumHexValue_{EnumHex::BaseHEX};
   EnumHex           EnumHexValue_{EnumHex::BaseHex};

   using Byte4Ary = fon9::CharAry<4, fon9::byte>;
   const Byte4Ary    ConstByte4Ary_{"B4AY"};
   Byte4Ary          Byte4Ary_{"b4ay"};
   const fon9::byte  ConstBytes_[4]{'B', 'Y', 'T', 'E'};
   fon9::byte        Bytes_[4]{'b', 'y', 't', 'e'};
   const fon9::ByteVector  ConstBVec_{fon9::StrView{"BVEC"}};
   fon9::ByteVector        BVec_{fon9::StrView{"bvec"}};

   using Dec = fon9::Decimal<int, 3>;
   const Dec                  ConstDecValue_{12.34};
   Dec                        DecValue_{56.78};
   using Dec0 = fon9::Decimal<int, 3>;
   Dec0                       Dec0Value_{123, 0};
   Dec0                       Dec0Null_{Dec0::Null()};

   const fon9::TimeStamp      ConstTs_{fon9::YYYYMMDDHHMMSS_ToTimeStamp(20180607143758)};
   fon9::TimeStamp            Ts_{fon9::YYYYMMDDHHMMSS_ToTimeStamp(20180607143759)};
   const fon9::TimeInterval   ConstTi_{fon9::TimeInterval_Millisecond(12345)};
   fon9::TimeInterval         Ti_{fon9::TimeInterval_Millisecond(67890)};
};
fon9_WARN_POP;

//--------------------------------------------------------------------------//

class ReqDataRaw : public Data, public fon9::seed::Raw {
   fon9_NON_COPY_NON_MOVE(ReqDataRaw);
public:
   ReqDataRaw() = default;
};

class ReqRawData : public fon9::seed::Raw, public Data {
   fon9_NON_COPY_NON_MOVE(ReqRawData);
public:
   ReqRawData() = default;
};

class ReqData : public Data {
   fon9_NON_COPY_NON_MOVE(ReqData);
public:
   ReqData() = default;
};

//--------------------------------------------------------------------------//

class ReqRawIncData : public fon9::seed::Raw {
   fon9_NON_COPY_NON_MOVE(ReqRawIncData);
public:
   ReqRawIncData() = default;

   Data  Data_;
};

class ReqIncData {
   fon9_NON_COPY_NON_MOVE(ReqIncData);
public:
   ReqIncData() = default;

   Data  Data_;
};

//--------------------------------------------------------------------------//

const char  kFieldSplitter = '|';

std::string MakeFieldValuesStr(std::ostream* os, const fon9::seed::Tab& tab, const fon9::seed::RawRd& rd) {
   fon9::RevBufferFixedSize<1024> rbuf;
   std::string res;
   size_t      L = 0;
   while (const fon9::seed::Field* fld = tab.Fields_.Get(L)) {
      rbuf.RewindEOS();
      fld->CellRevPrint(rd, nullptr, rbuf);
      if (os) {
         *os << std::setw(15) << fld->Name_
            << "|ofs=" << std::setw(5) << fld->Offset_
            << "|'" << rbuf.GetCurrent() << "'\n";
      }
      res.append(rbuf.GetCurrent(), rbuf.GetUsedSize() - 1);//-1 for no EOS.
      res.push_back(kFieldSplitter);
      ++L;
   }
   return res;
}
void TestFieldBitv(const fon9::seed::Tab& tab, const fon9::seed::RawWr& wr, const std::string& vlist) {
   assert(MakeFieldValuesStr(nullptr, tab, wr) == vlist);
   // 將 wr 填入 rbitv, 然後清除 wr.
   fon9::RevBufferFixedSize<1024> rbitv;
   auto fldidx = tab.Fields_.size();
   while (fldidx > 0) {
      auto* fld = tab.Fields_.Get(--fldidx);
      if (!fon9::seed::IsEnumContains(fld->Flags_, fon9::seed::FieldFlag::Readonly)) {
         fld->CellToBitv(wr, rbitv);
         fld->SetNull(wr);
      }
   }
   // 已經設定 fld->SetNull(); 則內容必定與 vlist 不同.
   assert(MakeFieldValuesStr(nullptr, tab, wr) != vlist);
   // 從 rbitv 還原, 看看結果是否符合預期.
   fon9::DcQueueFixedMem dcq{rbitv};
   fldidx = 0;
   while (auto* fld = tab.Fields_.Get(fldidx++)) {
      if (!fon9::seed::IsEnumContains(fld->Flags_, fon9::seed::FieldFlag::Readonly))
         fld->BitvToCell(wr, dcq);
   }
   fon9_CheckTestResult("TestBitv", vlist == MakeFieldValuesStr(nullptr, tab, wr));
}

template <class ReqT>
std::string TestGetFields(std::ostream* os, const fon9::seed::Tab& tab, ReqT& req) {
   if (os)
      *os << fon9::TypeName::Make<ReqT>().get() << '\n';
   std::string res = MakeFieldValuesStr(os, tab, fon9::seed::SimpleRawRd{fon9::seed::CastToRawPointer(&req)});
   TestFieldBitv(tab, fon9::seed::SimpleRawWr{fon9::seed::CastToRawPointer(&req)}, res);
   return res;
}

template <class ReqT>
std::string TestGetFields(std::ostream* os, fon9::seed::Fields&& fields) {
   fon9::seed::TabSP tab{new fon9::seed::Tab{fon9::Named{"Req"}, std::move(fields)}};
   ReqT              req;
   return TestGetFields(os, *tab, req);
}

template <class ReqT>
fon9::seed::Fields MakeReqFields() {
   fon9::seed::Fields fields;
   fields.Add(fon9_MakeField2(ReqT, ConstStdStrValue));
   fields.Add(fon9_MakeField2(ReqT, StdStrValue));
   fields.Add(fon9_MakeField2(ReqT, CharVectorValue));
   fields.Add(fon9_MakeField2(ReqT, ConstCharValue));
   fields.Add(fon9_MakeField2(ReqT, CharValue));
   fields.Add(fon9_MakeField2(ReqT, ConstChar4Value));
   fields.Add(fon9_MakeField2(ReqT, Char4Value));
   fields.Add(fon9_MakeField2(ReqT, ConstChar4Ary));
   fields.Add(fon9_MakeField2(ReqT, Char4Ary));
   fields.Add(fon9_MakeField2(ReqT, ConstChar7Ary));
   fields.Add(fon9_MakeField2(ReqT, Char7Ary));
   fields.Add(fon9_MakeField2(ReqT, ConstIntValue));
   fields.Add(fon9_MakeField2(ReqT, IntValue));
   fields.Add(fon9_MakeField2(ReqT, ConstEnumIntValue));
   fields.Add(fon9_MakeField2(ReqT, EnumIntValue));
   fields.Add(fon9_MakeField2(ReqT, ConstEnumCharValue));
   fields.Add(fon9_MakeField2(ReqT, EnumCharValue));
   fields.Add(fon9_MakeField2(ReqT, ConstEnumHexValue));
   fields.Add(fon9_MakeField(ReqT,  EnumHexValue_, "EnumHexValue", "FmtFlag", "Test FmtFlag hex output"));
   fields.Add(fon9_MakeField2(ReqT, ConstByte4Ary));
   fields.Add(fon9_MakeField2(ReqT, Byte4Ary));
   fields.Add(fon9_MakeField2(ReqT, ConstBytes));
   fields.Add(fon9_MakeField2(ReqT, Bytes));
   fields.Add(fon9_MakeField2(ReqT, ConstBVec));
   fields.Add(fon9_MakeField2(ReqT, BVec));
   fields.Add(fon9_MakeField2(ReqT, ConstDecValue));
   fields.Add(fon9_MakeField2(ReqT, DecValue));
   fields.Add(fon9_MakeField2(ReqT, Dec0Value));
   fields.Add(fon9_MakeField2(ReqT, Dec0Null));
   fields.Add(fon9_MakeField2(ReqT, ConstTs));
   fields.Add(fon9_MakeField2(ReqT, Ts));
   fields.Add(fon9_MakeField2(ReqT, ConstTi));
   fields.Add(fon9_MakeField2(ReqT, Ti));
   return fields;
}

template <class ReqT>
fon9::seed::Fields MakeReqFieldsIncData() {
   fon9::seed::Fields fields;
   fields.Add(fon9_MakeField(ReqT, Data_.ConstStdStrValue_,   "ConstStdStrValue"));
   fields.Add(fon9_MakeField(ReqT, Data_.StdStrValue_,        "StdStrValue"));
   fields.Add(fon9_MakeField(ReqT, Data_.CharVectorValue_,    "CharVectorValue"));
   fields.Add(fon9_MakeField(ReqT, Data_.ConstCharValue_,     "ConstCharValue"));
   fields.Add(fon9_MakeField(ReqT, Data_.CharValue_,          "CharValue"));
   fields.Add(fon9_MakeField(ReqT, Data_.ConstChar4Value_,    "ConstChar4Value"));
   fields.Add(fon9_MakeField(ReqT, Data_.Char4Value_,         "Char4Value"));
   fields.Add(fon9_MakeField(ReqT, Data_.ConstChar4Ary_,      "ConstChar4Ary"));
   fields.Add(fon9_MakeField(ReqT, Data_.Char4Ary_,           "Char4Ary"));
   fields.Add(fon9_MakeField(ReqT, Data_.ConstChar7Ary_,      "ConstChar7Ary"));
   fields.Add(fon9_MakeField(ReqT, Data_.Char7Ary_,           "Char7Ary"));
   fields.Add(fon9_MakeField(ReqT, Data_.ConstIntValue_,      "ConstIntValue"));
   fields.Add(fon9_MakeField(ReqT, Data_.IntValue_,           "IntValue"));
   fields.Add(fon9_MakeField(ReqT, Data_.ConstEnumIntValue_,  "ConstEnumIntValue"));
   fields.Add(fon9_MakeField(ReqT, Data_.EnumIntValue_,       "EnumIntValue"));
   fields.Add(fon9_MakeField(ReqT, Data_.ConstEnumCharValue_, "ConstEnumCharValue"));
   fields.Add(fon9_MakeField(ReqT, Data_.EnumCharValue_,      "EnumCharValue"));
   fields.Add(fon9_MakeField(ReqT, Data_.ConstEnumHexValue_,  "ConstEnumHexValue"));
   fields.Add(fon9_MakeField(ReqT, Data_.EnumHexValue_,       "EnumHexValue", "FmtFlag", "Test FmtFlag hex output"));
   fields.Add(fon9_MakeField(ReqT, Data_.ConstByte4Ary_,      "ConstByte4Ary"));
   fields.Add(fon9_MakeField(ReqT, Data_.Byte4Ary_,           "Byte4Ary"));
   fields.Add(fon9_MakeField(ReqT, Data_.ConstBytes_,         "ConstBytes"));
   fields.Add(fon9_MakeField(ReqT, Data_.Bytes_,              "Bytes"));
   fields.Add(fon9_MakeField(ReqT, Data_.ConstBVec_,          "ConstBVec"));
   fields.Add(fon9_MakeField(ReqT, Data_.BVec_,               "BVec"));
   fields.Add(fon9_MakeField(ReqT, Data_.ConstDecValue_,      "ConstDecValue"));
   fields.Add(fon9_MakeField(ReqT, Data_.DecValue_,           "DecValue"));
   fields.Add(fon9_MakeField(ReqT, Data_.Dec0Value_,          "Dec0Value"));
   fields.Add(fon9_MakeField(ReqT, Data_.Dec0Null_,           "Dec0Null"));
   fields.Add(fon9_MakeField(ReqT, Data_.ConstTs_,            "ConstTs"));
   fields.Add(fon9_MakeField(ReqT, Data_.Ts_,                 "Ts"));
   fields.Add(fon9_MakeField(ReqT, Data_.ConstTi_,            "ConstTi"));
   fields.Add(fon9_MakeField(ReqT, Data_.Ti_,                 "Ti"));
   return fields;
}

//--------------------------------------------------------------------------//

void TestDyRec(const std::string& fldcfg, const std::string& vlist) {
   std::cout << fldcfg << std::endl;
   fon9::StrView      cfg{&fldcfg};
   fon9::seed::Fields fields;
   fon9::seed::MakeFields(cfg, '|', '\n', fields);
   const size_t       fieldsCount = fields.size();
   // 測試重複設定: 檢查 TypeId() 是否正確.
   cfg = fon9::ToStrView(fldcfg);
   fon9_CheckTestResult("MakeFields",
                        fon9::seed::MakeFields(cfg, '|', '\n', fields) == fon9::seed::OpResult::no_error);
   fon9_CheckTestResult("Dup.MakeFields", fieldsCount == fields.size());

   // 測試 DyRec 沒有使用 MakeDyMemRaw() 建立: 必定產生 exception!
   fon9::seed::TabSP tab{new fon9::seed::Tab{fon9::Named{"DyRec"}, std::move(fields)}};
   try {
      ReqRawData req;
      TestGetFields(nullptr, *tab, req);
      fon9_CheckTestResult("DyRec", false);
   }
   catch (const std::exception& e) {
      fon9_CheckTestResult(e.what(), true);
   }

   // 測試 DyRec 設定值 StrToCell().
   struct DyRaw : public fon9::seed::Raw {
      fon9_NON_COPY_NON_MOVE(DyRaw);
      DyRaw() = default;
   };
   auto dyreq = std::unique_ptr<DyRaw>(fon9::seed::MakeDyMemRaw<DyRaw>(*tab));
   cfg = fon9::ToStrView(vlist);
   fon9::seed::SimpleRawWr wr{fon9::seed::CastToRawPointer(dyreq.get())};
   for (size_t idx = 0;; ++idx) {
      const fon9::seed::Field* wrfld = tab->Fields_.Get(idx);
      if (!wrfld)
         break;
      wrfld->StrToCell(wr, fon9::StrFetchNoTrim(cfg, kFieldSplitter));
   }
   fon9_CheckTestResult("DyRec.StrToCell", vlist == TestGetFields(nullptr, *tab, *dyreq));
}

//--------------------------------------------------------------------------//

void TestDeserializeNamed(fon9::StrView src, char chSpl, int chTail) {
   std::cout << "\n" << src.begin() << " => ";
   fon9::Named n1 = fon9::DeserializeNamed(src, chSpl, chTail);
   std::cout << "name=" << n1.Name_ << "|title=" << n1.GetTitle() << "|desc=" << n1.GetDescription() << std::endl;

   fon9_CheckTestResult("DeserializeNamed",
      (n1.Name_ == "fonwin" && n1.GetTitle() == "Fon9 title" && n1.GetDescription() == "Fon9 Description"));
   if (chTail == -1)
      fon9_CheckTestResult("DeserializeNamed:src.begin()==src.end()", (src.begin() == src.end()));
   else
      fon9_CheckTestResult("DeserializeNamed:src.begin()[-1]==chTail", (*(src.begin() - 1) == chTail));

   std::string s1;
   fon9::SerializeNamed(s1, n1, chSpl, chTail);
   src = fon9::ToStrView(s1);
   fon9::Named n2 = fon9::DeserializeNamed(src, chSpl, chTail);
   fon9_CheckTestResult(("SerializeNamed()=" + s1).c_str(),
      (n1.Name_ == n2.Name_ && n1.GetTitle() == n2.GetTitle() && n1.GetDescription() == n2.GetDescription()));
}
void TestDeserializeNamed() {
   TestDeserializeNamed("fonwin|Fon9 title|Fon9 Description", '|', -1);
   TestDeserializeNamed("fonwin|Fon9 title|Fon9 Description\t", '|', '\t');
   TestDeserializeNamed("  fonwin  |   Fon9 title  |  Fon9 Description   ", '|', -1);
   TestDeserializeNamed("  fonwin  |   Fon9 title  |  Fon9 Description   \t" "test tail message", '|', '\t');
   TestDeserializeNamed("  fonwin  |  'Fon9 title' |  \"Fon9 Description\"   ", '|', -1);
   TestDeserializeNamed("  fonwin  |  'Fon9 title' |  \"Fon9 Description\"   \t" "test tail message", '|', '\t');
}

//--------------------------------------------------------------------------//

int main(int argc, char** args) {
   (void)argc; (void)args;
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   //_CrtSetBreakAlloc(176);
#endif
   fon9::AutoPrintTestInfo utinfo{"Seed/FieldMaker"};

   TestDeserializeNamed();

   utinfo.PrintSplitter();
   const std::string vlist = TestGetFields<ReqRawData>(&std::cout, MakeReqFields<ReqRawData>());
   fon9_CheckTestResult("ReqDataRaw",    vlist == TestGetFields<ReqDataRaw>(nullptr, MakeReqFields<ReqDataRaw>()));
   fon9_CheckTestResult("ReqData",       vlist == TestGetFields<ReqData>   (nullptr, MakeReqFields<ReqData>()));
   fon9_CheckTestResult("ReqRawIncData", vlist == TestGetFields<ReqRawData>(nullptr, MakeReqFieldsIncData<ReqRawIncData>()));
   fon9_CheckTestResult("ReqIncData",    vlist == TestGetFields<ReqData>   (nullptr, MakeReqFieldsIncData<ReqIncData>()));

   TestDyRec(MakeFieldsConfig(MakeReqFields<ReqRawData>(), '|', '\n'), vlist);
}
