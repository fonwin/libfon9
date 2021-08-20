// \file fon9/rc/RcSeedVisitor.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/RcSeedVisitor.hpp"
#include "fon9/StrTools.hpp"
#include "fon9/Log.hpp"
#include "fon9/BitvDecode.hpp"

namespace fon9 { namespace rc {

SvGvTable::~SvGvTable() {
   for (auto* vlist : this->RecList_)
      delete[] vlist;
}
void SvGvTable::InitializeGvList() {
   if (this->RecList_.empty())
      ZeroStruct(this->GvList_);
   else {
      this->GvList_.FieldArray_ = this->Fields_.GetVector();
      this->GvList_.FieldCount_ = static_cast<unsigned>(this->Fields_.size());
      this->GvList_.RecordCount_ = static_cast<unsigned>(this->RecList_.size());
      const StrView** recs = &this->RecList_[0];
      this->GvList_.RecordList_ = reinterpret_cast<const fon9_CStrView**>(recs);
   }
}

fon9_API void SvParseGvTables(SvGvTables& dst, std::string& srcstr) {
   StrView    src{&srcstr};
   SvGvTable* curTab = nullptr;
   while (!StrTrimHead(&src).empty()) {
      StrView ln = StrFetchNoTrim(src, *fon9_kCSTR_ROWSPL);
      if (ln.Get1st() == *fon9_kCSTR_LEAD_TABLE) {
         ln.SetBegin(ln.begin() + 1);
         *const_cast<char*>(ln.end()) = '\0';
         if (!dst.Add(SvGvTableSP{curTab = new SvGvTable{ln}})) {
            curTab = nullptr;
            fon9_LOG_ERROR("SvParseGvTables.AddTable|name=", ln);
            continue;
         }
         ln = StrFetchNoTrim(src, *fon9_kCSTR_ROWSPL);
         while (!StrTrimHead(&ln).empty()) {
            StrView fldName = StrFetchNoTrim(ln, *fon9_kCSTR_CELLSPL);
            *const_cast<char*>(fldName.end()) = '\0';
            curTab->Fields_.Add(SvField{fldName});
         }
      }
      else if (curTab) {
         size_t   fldsz = curTab->Fields_.size();
         StrView* vlist;
         curTab->RecList_.emplace_back(vlist = new StrView[fldsz]);
         while (fldsz > 0) {
            *vlist = StrFetchNoTrim(ln, *fon9_kCSTR_CELLSPL);
            *const_cast<char*>(vlist->end()) = '\0';
            ++vlist;
            --fldsz;
         }
      }
   }

   unsigned L = 0;
   while (SvGvTable* tab = dst.Get(L++)) {
      tab->InitializeGvList();
   }
}

fon9_API void SvParseGvTablesC(f9sv_GvTables& cTables, SvGvTables& dst, std::string& strbuf, StrView orig) {
   cTables.OrigStrView_ = orig.ToCStrView();
   strbuf = orig.ToString();
   dst.clear();
   SvParseGvTables(dst, strbuf);

   fon9_GCC_WARN_DISABLE("-Wold-style-cast");
   const SvGvTableSP* ppTabs = dst.GetVector();
   cTables.TableList_ = (const f9sv_GvTable**)(ppTabs);
   cTables.TableCount_ = static_cast<unsigned>(dst.size());
   fon9_GCC_WARN_POP;
}
//--------------------------------------------------------------------------//
void RcSvReqKey::LoadSeedName(DcQueue& rxbuf) {
   this->IsAllTabs_ = false;
   BitvTo(rxbuf, this->TreePath_);
   BitvTo(rxbuf, this->SeedKey_);

   const byte* tabi = rxbuf.Peek1();
   if (tabi == nullptr)
      Raise<BitvNeedsMore>("RcSeedVisitor: needs more");

   switch (*tabi) {
   default:
      if ((*tabi & fon9_BitvT_Mask) != fon9_BitvT_IntegerPos) {
         BitvTo(rxbuf, this->TabName_);
         this->IsAllTabs_ = seed::IsTabAll(this->TabName_.begin());
         break;
      }
      /* fall through */ // bitv = fon9_BitvT_IntegerPos: 取出 tabidx;
   case fon9_BitvV_NumberNull: // all tabs.
   case fon9_BitvV_Number0:
      f9sv_TabSize  tabidx = kTabAll;
      BitvTo(rxbuf, tabidx);
      if (tabidx == kTabAll) {
         this->IsAllTabs_ = true;
         this->TabName_.assign("*");
      }
      else if (tabidx != 0) {
         NumOutBuf nbuf;
         this->TabName_.assign(fon9::ToStrRev(nbuf.end(), tabidx), nbuf.end());
      }
      break;
   }
}
CharVector RcSvReqKey::SeedPath() const {
   CharVector seedPath{ToStrView(this->TreePath_)};
   seedPath.push_back('/');
   if (this->SeedKey_.empty())
      seedPath.append("''");
   else if (this->SeedKey_.size() == 1 && *this->SeedKey_.begin() == '\t') {
      // tree op. 不用加上 key.
   }
   else
      seedPath.append(ToStrView(this->SeedKey_));
   return seedPath;
}
void RcSvReqKey::LoadFrom(SvFunc fc, DcQueue& rxbuf) {
   this->LoadSeedName(rxbuf);
   if (GetSvFuncCode(fc) == SvFuncCode::Subscribe) {
      this->SubrIndex_ = kSubrIndexNull;
      BitvTo(rxbuf, this->SubrIndex_);
   }
}
fon9_API void RevPrint(RevBuffer& rbuf, const RcSvReqKey& reqKey) {
   if (reqKey.SubrIndex_ != kSubrIndexNull)
      RevPrint(rbuf, "|subrIndex=", reqKey.SubrIndex_);
   if (reqKey.IsAllTabs_)
      RevPrint(rbuf, '*');
   else
      RevPrint(rbuf, reqKey.TabName_);
   RevPrint(rbuf, "|path=", reqKey.TreePath_, "|key=", reqKey.SeedKey_, "|tab=");
}

} } // namespaces
