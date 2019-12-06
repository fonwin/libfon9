// \file fon9/Bitv_UT.cpp
// \author fonwinz@gmail.com
#include "fon9/BitvArchive.hpp"
#include "fon9/BitvFixedInt.hpp"
#include "fon9/CharVector.hpp"
#include "fon9/TimeStamp.hpp"
#include "fon9/buffer/RevBufferList.hpp"
#include "fon9/buffer/DcQueueList.hpp"
#include "fon9/TestTools.hpp"
#include "fon9/Timer.hpp"
#include <vector>

//--------------------------------------------------------------------------//

enum class EnumCh : char {
   Value = 'V',
};
enum class EnumInt : int32_t {
   Value = 123,
};

fon9_WARN_DISABLE_PADDING;
struct CompositeRec {
   char     Ch_{'C'};
   EnumCh   ECh_{EnumCh::Value};
   EnumInt  EInt_{EnumInt::Value};
   bool     BoolTrue_{true};
   bool     BoolFalse_{false};
   int8_t   S8_{-1};
   int16_t  S16_{-1234};
   int32_t  S32_{-1234567};
   int64_t  S64_{-112233445566778899};
   uint8_t  U8_{1};
   uint16_t U16_{1234};
   uint32_t U32_{1234567};
   uint64_t U64_{112233445566778899};
   uint64_t V1_{1};
   fon9::Decimal<int64_t, 6>  Dec_{123.4};
   fon9::CharAry<6>           CharAry_{"abcd"};
   std::array<char, 6>        CAry_{"1234"};
   fon9::TimeStamp            TimeStamp_{1531025265.1234};
   fon9::TimeInterval         TimeInterval_{123.45};

   bool operator==(const CompositeRec& rhs) const {
      return this->Ch_ == rhs.Ch_
         && this->ECh_ == rhs.ECh_
         && this->EInt_ == rhs.EInt_
         && this->BoolTrue_ == rhs.BoolTrue_
         && this->BoolFalse_ == rhs.BoolFalse_
         && this->S8_ == rhs.S8_
         && this->S16_ == rhs.S16_
         && this->S32_ == rhs.S32_
         && this->S64_ == rhs.S64_
         && this->U8_ == rhs.U8_
         && this->U16_ == rhs.U16_
         && this->U32_ == rhs.U32_
         && this->U64_ == rhs.U64_
         && this->V1_ == rhs.V1_
         && this->Dec_ == rhs.Dec_
         && this->CharAry_ == rhs.CharAry_
         && this->CAry_ == rhs.CAry_
         && this->TimeStamp_ == rhs.TimeStamp_
         && this->TimeInterval_ == rhs.TimeInterval_
         ;
   }
   bool operator!=(const CompositeRec& rhs) const {
      return !this->operator==(rhs);
   }
};
fon9_WARN_POP;

template <class Archive>
void SerializeV0(Archive& ar, fon9::ArchiveWorker<Archive, CompositeRec>& rec) {
   ar(rec.Ch_, rec.ECh_, rec.EInt_, rec.BoolTrue_, rec.BoolFalse_,
      rec.S8_, rec.S16_, rec.S32_, rec.S64_,
      rec.U8_, rec.U16_, rec.U32_, rec.U64_);
}

template <class Archive>
void SerializeVer(Archive& ar, fon9::ArchiveWorker<Archive, CompositeRec>& rec, unsigned ver) {
   if (ver == 0)
      return SerializeV0(ar, rec);
   return ar(std::make_pair(&SerializeV0<Archive>, &rec), rec.V1_,
             rec.Dec_, rec.CharAry_, rec.CAry_, rec.TimeStamp_, rec.TimeInterval_);
}

template <class Archive>
void Serialize(Archive& ar, fon9::ArchiveWorker<Archive, CompositeRec>& rec) {
   // 包含版本號的編碼.
   fon9::CompositeVer<decltype(rec)> ver{rec, 1};
   ar(ver);
}

bool TestBitvCompositeRecFill(int fill) {
   fon9::RevBufferList  rbuf{128};
   fon9::BitvOutArchive oar{rbuf};
   const CompositeRec   src;
   oar(src);

   fon9::DcQueueList ibuf{rbuf.MoveOut()};
   if (fill == 0)
      std::cout << "|size=" << ibuf.CalcSize();

   fon9::BitvInArchive  iar{ibuf};
   CompositeRec         out;
   memset(&out, fill, sizeof(out));
   iar(out);

   return (src == out);
}
void DumpTestBitvCompositeRec(const char* strResult) {
   std::cout << "\r" << strResult << "\n";
   fon9::RevBufferList  rbuf{128};
   fon9::BitvOutArchive oar{rbuf};
   const CompositeRec   src;
   oar(src);
   std::string ostr = fon9::BufferTo<std::string>(rbuf.MoveOut());
   for (auto c : ostr)
      fprintf(stdout, "%02x ", static_cast<fon9::byte>(c));
   std::cout << std::endl << std::dec;
}

void TestBitvCompositeRec() {
   std::cout << "[TEST ] Bitv Serialize/Deserialize Record";
   if (TestBitvCompositeRecFill(0) && TestBitvCompositeRecFill(0xff))
      DumpTestBitvCompositeRec("[OK   ]");
   else {
      DumpTestBitvCompositeRec("[ERROR]");
      abort();
   }
}

//--------------------------------------------------------------------------//

template <class StrT>
void TestBitvString(const char* itemName) {
   std::cout << "[TEST ] Bitv Serialize/Deserialize " << itemName;
   fon9::RevBufferList rbuf{16};
   for (unsigned L = 0; L < 1024 * 4; ++L) {
      fon9::BitvOutArchive oar{rbuf};
      StrT  ostr;
      using value_type = typename StrT::value_type;
      ostr.append(L, static_cast<value_type>((L % 10) + '0'));
      oar(ostr);

      fon9::DcQueueList    ibuf{rbuf.MoveOut()};
      fon9::BitvInArchive  iar{ibuf};
      StrT                 istr;
      iar(istr);

      if (ostr != istr) {
         std::cout << "|L=" << L << "\r" "[ERROR]" << std::endl;
         abort();
      }
   }
   std::cout << "\r" "[OK   ]" << std::endl;
}

struct Blob : public fon9_Blob {
   using value_type = fon9::byte;
   Blob() { fon9::ZeroStruct(static_cast<fon9_Blob*>(this)); }
   ~Blob() { fon9_Blob_Free(this); }
   void append(size_t sz, value_type ch) {
      fon9_Blob_Append(this, nullptr, static_cast<fon9_Blob_Size_t>(sz));
      memset(this->MemPtr_, ch, sz);
   }
   bool operator!=(const Blob& rhs) const {
      return fon9_CompareBytes(this->MemPtr_, this->MemUsed_, rhs.MemPtr_, rhs.MemUsed_) != 0;
   }
   friend inline fon9::StrView ToStrView(const Blob& rthis) {
      return fon9::StrView{reinterpret_cast<const char*>(rthis.MemPtr_), rthis.MemUsed_};
   }
};

//--------------------------------------------------------------------------//

// c++17 才提供 std::size(); 所以在此自訂.
template <class C>           constexpr auto   csize(const C& c) -> decltype(c.size()) { return c.size(); }
template <class T, size_t N> constexpr size_t csize(const T (&)[N]) noexcept { return N; }
// MSVC: std:equal(first1,last1,first2) 需要額外 `#define _SCL_SECURE_NO_WARNINGS` 所以自己寫一個.
template<class InputIt1, class InputIt2>
bool cequal(InputIt1 first1, InputIt1 last1, InputIt2 first2) {
   for (; first1 != last1; ++first1, ++first2) {
      if (*first1 != *first2)
         return false;
   }
   return true;
}

template <class Container>
void TestBitvContainer(const char* itemName, Container&& val1, Container&& val2 = Container{}) {
   for (size_t L = csize(val1); L > 0;) {
      --L;
      val1[L] = L;
   }
   std::cout << "[TEST ] Bitv Encode/Decode " << itemName << "|count=" << csize(val1);
   fon9::RevBufferList rbuf{16};
   fon9::ToBitv(rbuf, val1);

   fon9::DcQueueList   ibuf{rbuf.MoveOut()};
   std::cout << "|totsz=" << ibuf.CalcSize();

   fon9::BitvTo(ibuf, val2);
   if (csize(val1) == csize(val2)
       && cequal(std::begin(val1), std::end(val1), std::begin(val2))) {
      std::cout << "\r" "[OK   ]" << std::endl;
      return;
   }
   std::cout << "\r" "[ERROR]" << std::endl;
   abort();
}
//--------------------------------------------------------------------------//
template <size_t kBitvLen>
void TestBitvFixedUInt() {
   fon9::RevBufferList            rbuf{1024};
   fon9::BitvFixedUInt<kBitvLen>  val;
   const unsigned                 kTimes = 1000;
   std::cout << "[TEST ] BitvFixedUInt<" << val.kBitvLen << '>';
   val.Value_ = 0;
   for (unsigned L = 0; L < kTimes; ++L) {
      ++val.Value_;
      fon9::ToBitv(rbuf, val);
   }
   if (kTimes * (val.kBitvLen + 1) != fon9::CalcDataSize(rbuf.cfront())) {
      std::cout << "|OutputSize=" << fon9::CalcDataSize(rbuf.cfront())
         << "|expected=" << kTimes * (val.kBitvLen + 1)
         << "\r[ERROR]" << std::endl;
      abort();
   }
   val.Value_ = std::numeric_limits<decltype(val.Value_)>::max();
   fon9::DcQueueList dcq{rbuf.MoveOut()};
   for (unsigned L = kTimes; L > 0; --L) {
      fon9::BitvTo(dcq, val);
      if (val.Value_ != static_cast<typename fon9::BitvFixedUInt<kBitvLen>::Type>(L)) {
         std::cout
            << "|BitvFixedUInt(); val=" << val.Value_
            << "|expected=" << static_cast<typename fon9::BitvFixedUInt<kBitvLen>::Type>(L)
            << "\r[ERROR]"
            << std::endl;
         abort();
      }
   }
   std::cout << "\r[OK   ]" << std::endl;
}

template <size_t kBitvBitSize>
void TestBitvFixedScale(const unsigned kTimes) {
   const size_t               kBitvBytes = (2 + (kBitvBitSize + 7 - 2) / 8);
   fon9::Decimal<uint32_t, 2> val;
   fon9::RevBufferList        rbuf{1024};
   std::cout << "[TEST ] TestBitvFixedScale<" << kBitvBitSize << '>';
   for (unsigned L = 0; L < kTimes; ++L) {
      fon9::ToBitvFixedScale<kBitvBitSize>(rbuf, val);
      val.SetOrigValue(val.GetOrigValue() + 1);
   }
   if (kTimes * kBitvBytes != fon9::CalcDataSize(rbuf.cfront())) {
      std::cout << "|OutputSize=" << fon9::CalcDataSize(rbuf.cfront())
         << "|expected=" << kTimes * kBitvBytes
         << "\r[ERROR]" << std::endl;
      abort();
   }
   val.AssignNull();
   fon9::DcQueueList dcq{rbuf.MoveOut()};
   for (unsigned L = kTimes; L > 0;) {
      fon9::BitvTo(dcq, val);
      if (val.GetOrigValue() != --L) {
         std::cout
            << "|ToBitvFixedScale(); OrigValue=" << val.GetOrigValue()
            << "|expected=" << L
            << "\r[ERROR]"
            << std::endl;
         abort();
      }
   }
   std::cout << "\r[OK   ]" << std::endl;
}
//--------------------------------------------------------------------------//
int main(int argc, char** args) {
   (void)argc; (void)args;
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
   fon9::AutoPrintTestInfo utinfo("Bitv/Serialize/Deserialize");
   fon9::GetDefaultTimerThread();
   std::this_thread::sleep_for(std::chrono::milliseconds{100});

   //--------------------------------------------------------------------------//
   TestBitvFixedUInt<1>();
   TestBitvFixedUInt<2>();
   TestBitvFixedUInt<3>();
   TestBitvFixedUInt<4>();
   TestBitvFixedUInt<5>();
   TestBitvFixedUInt<6>();
   TestBitvFixedUInt<7>();

   //--------------------------------------------------------------------------//
   TestBitvFixedScale<fon9::CalcValueBits(0x3ff)>(0x3ff);
   TestBitvFixedScale<fon9::CalcValueBits(0x3ffff)>(0x3ffff);
   // 86401000000 = 14 1DE6 A240‬ => 37
   static_assert(fon9::CalcValueBits((24 * 60 * 60 + 1) * fon9::DayTime::Divisor) == 37,
                 "CalcValueBits(DayTime) err.");
   // 0xffffffff * 1000000 = F 423F FFF0 BDC0‬ => 52
   static_assert(fon9::CalcValueBits(0xffffffff * fon9::TimeStamp::Divisor) == 52,
                 "CalcValueBits(TimeStamp) err.");

   fon9::RevBufferList  rbuf{16};
   using DecU8d2 = fon9::Decimal<uint64_t, 2>;
   fon9::ToBitvFixedScale(rbuf, DecU8d2{});
   fon9::ToBitvFixedScale(rbuf, DecU8d2::Null());
   fon9::ToBitvFixedScale(rbuf, DecU8d2{123.4});
   fon9::DcQueueList ibuf{rbuf.MoveOut()};
   DecU8d2 vZero,vNull,v123d4;
   fon9::BitvTo(ibuf, v123d4);
   fon9::BitvTo(ibuf, vNull);
   fon9::BitvTo(ibuf, vZero);
   if (!ibuf.empty() || !vZero.IsZero() || !vNull.IsNull() || v123d4.To<double>() != 123.4) {
      std::cout << "[ERROR] ToBitvFixedScale()" << std::endl;
      abort();
   }

   //--------------------------------------------------------------------------//
   fon9::TimeStamp tm1,tm2,now{fon9::UtcNow()};
   fon9::ToBitv(rbuf, now.ToDecimal());
   fon9::ToBitv(rbuf, now);
   ibuf.push_back(rbuf.MoveOut());
   fon9::BitvTo(ibuf, tm1);
   fon9::BitvTo(ibuf, tm2);
   if (tm1 != now || tm2 != now || !ibuf.empty()) {
      std::cout << "[ERROR] BitvTo/ToBitv(TimeStamp)" << std::endl;
      abort();
   }

   //--------------------------------------------------------------------------//
   TestBitvString<std::string>("std::string");
   TestBitvString<fon9::CharVector>("CharVector");
   TestBitvString<fon9::ByteVector>("ByteVector");
   TestBitvString<Blob>("fon9_Blob");

   //--------------------------------------------------------------------------//
   std::cout << "[TEST ] Bitv Encode/Decode Decimal<int64_t,3> 0..1,000,000";
   size_t totsz = 0;
   for (int32_t L = 0; L < 1000 * 1000; ++L) {
      using Dec = fon9::Decimal<int32_t, 3>;
      Dec idec{L,3};
      fon9::ToBitv(rbuf, idec);
      ibuf.push_back(rbuf.MoveOut());
      totsz += ibuf.CalcSize();
      Dec odec{Dec::Null()};
      fon9::BitvTo(ibuf, odec);

      if (ibuf.CalcSize() != 0 || idec != odec) {
         std::cout << "|L=" << L << "\r" "[ERROR]" << std::endl;
         abort();
      }
   }
   std::cout << "|totsz=" << totsz << "\r" "[OK   ]" << std::endl;

   //--------------------------------------------------------------------------//
   utinfo.PrintSplitter();
   TestBitvCompositeRec();

   using ElementType = size_t;
   constexpr size_t kContainerSize = 1000;
   ElementType ary1[kContainerSize];
   ElementType ary2[kContainerSize];
   TestBitvContainer("size_t[]      ", ary1, ary2);
   TestBitvContainer("array<size_t> ", std::array<ElementType, kContainerSize>{});
   TestBitvContainer("vector<size_t>", std::vector<ElementType>(kContainerSize));

   //--------------------------------------------------------------------------//
   // Benchmark: "fon9/ext/yas/main.cpp"
}
